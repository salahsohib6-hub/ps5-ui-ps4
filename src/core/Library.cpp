#include "core/Library.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "core/FileSystem.h"
#include "core/Log.h"
#include "core/Sfo.h"

namespace ps5ui {

const char* sortName(LibrarySort s) {
  switch (s) {
    case LibrarySort::TitleAZ: return "A-Z";
    case LibrarySort::TitleZA: return "Z-A";
    case LibrarySort::TitleId: return "Title ID";
    case LibrarySort::MostLaunched: return "Most launched";
    case LibrarySort::RecentlyLaunched: return "Recently launched";
  }
  return "?";
}

const char* filterName(LibraryFilter f) { return f == LibraryFilter::Favorites ? "Favorites" : "All"; }

bool Library::isGameCategory(const std::string& c) { return c.size() >= 2 && c[0] == 'g' && c[1] == 'd'; }

namespace {

std::string lower(std::string s) {
  for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

bool validTitleId(const std::string& id) {
  if (id.size() != 9) return false;
  for (size_t i = 0; i < 4; ++i)
    if (!std::isalpha(static_cast<unsigned char>(id[i]))) return false;
  for (size_t i = 4; i < 9; ++i)
    if (!std::isdigit(static_cast<unsigned char>(id[i]))) return false;
  return true;
}

}  // namespace

void Library::applyUserData(GameEntry& g) const {
  auto it = user_.find(g.titleId);
  if (it == user_.end()) return;
  g.favorite = it->second.favorite;
  g.hidden = it->second.hidden;
  g.launchCount = it->second.launchCount;
  g.lastLaunch = it->second.lastLaunch;
}

void Library::syncUserData(const GameEntry& g) {
  UserData& u = user_[g.titleId];
  u.favorite = g.favorite;
  u.hidden = g.hidden;
  u.launchCount = g.launchCount;
  u.lastLaunch = g.lastLaunch;
}

int Library::scan() {
  games_.clear();
  for (const std::string& root : roots_) {
    if (!fs::isDirectory(root)) continue;
    for (const std::string& name : fs::listDirectory(root)) {
      if (!validTitleId(name)) continue;
      std::string dir = fs::join(root, name);
      std::string sfoPath = fs::join(dir, "sce_sys/param.sfo");
      std::vector<uint8_t> bytes;
      if (!fs::readFile(sfoPath, bytes, 64 * 1024)) continue;
      SfoData sfo;
      if (!parseSfo(bytes.data(), bytes.size(), sfo)) {
        log::write("library: bad param.sfo in %s", dir.c_str());
        continue;
      }
      GameEntry g;
      sfo.getString("TITLE_ID", g.titleId);
      if (g.titleId.empty()) g.titleId = name;
      if (!sfo.getString("TITLE", g.title) || g.title.empty()) g.title = g.titleId;
      sfo.getString("CONTENT_ID", g.contentId);
      sfo.getString("CATEGORY", g.category);
      if (!sfo.getString("APP_VER", g.version)) sfo.getString("VERSION", g.version);
      sfo.getInt("SYSTEM_VER", g.minFirmware);
      if (!isGameCategory(g.category)) continue;  // skip patches / add-on content
      g.path = dir;
      g.iconPath = fs::join(dir, "sce_sys/icon0.png");
      if (indexOf(g.titleId) >= 0) continue;      // same title in two roots: keep first
      applyUserData(g);
      games_.push_back(std::move(g));
    }
  }
  log::write("library: %d title(s) found", static_cast<int>(games_.size()));
  return static_cast<int>(games_.size());
}

std::vector<int> Library::view(LibrarySort sort, LibraryFilter filter, bool includeHidden) const {
  std::vector<int> idx;
  for (size_t i = 0; i < games_.size(); ++i) {
    const GameEntry& g = games_[i];
    if (g.hidden && !includeHidden) continue;
    if (filter == LibraryFilter::Favorites && !g.favorite) continue;
    idx.push_back(static_cast<int>(i));
  }
  auto byTitle = [&](int a, int b) { return lower(games_[a].title) < lower(games_[b].title); };
  switch (sort) {
    case LibrarySort::TitleAZ:
      std::stable_sort(idx.begin(), idx.end(), byTitle);
      break;
    case LibrarySort::TitleZA:
      std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return byTitle(b, a); });
      break;
    case LibrarySort::TitleId:
      std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return games_[a].titleId < games_[b].titleId; });
      break;
    case LibrarySort::MostLaunched:
      std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        if (games_[a].launchCount != games_[b].launchCount) return games_[a].launchCount > games_[b].launchCount;
        return byTitle(a, b);
      });
      break;
    case LibrarySort::RecentlyLaunched:
      std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        if (games_[a].lastLaunch != games_[b].lastLaunch) return games_[a].lastLaunch > games_[b].lastLaunch;
        return byTitle(a, b);
      });
      break;
  }
  return idx;
}

int Library::indexOf(const std::string& titleId) const {
  for (size_t i = 0; i < games_.size(); ++i)
    if (games_[i].titleId == titleId) return static_cast<int>(i);
  return -1;
}

bool Library::toggleFavorite(const std::string& titleId) {
  int i = indexOf(titleId);
  if (i < 0) return false;
  games_[i].favorite = !games_[i].favorite;
  syncUserData(games_[i]);
  return games_[i].favorite;
}

bool Library::toggleHidden(const std::string& titleId) {
  int i = indexOf(titleId);
  if (i < 0) return false;
  games_[i].hidden = !games_[i].hidden;
  syncUserData(games_[i]);
  return games_[i].hidden;
}

void Library::recordLaunch(const std::string& titleId, int64_t unixSeconds) {
  int i = indexOf(titleId);
  if (i < 0) return;
  games_[i].launchCount++;
  games_[i].lastLaunch = unixSeconds;
  syncUserData(games_[i]);
}

bool Library::loadUserData(const std::string& path) {
  std::string text;
  if (!fs::readTextFile(path, text)) return false;
  user_.clear();
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    std::string line = text.substr(pos, eol - pos);
    pos = eol + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    // TITLEID|favorite|hidden|launchCount|lastLaunch
    std::vector<std::string> f;
    size_t s = 0;
    while (true) {
      size_t bar = line.find('|', s);
      if (bar == std::string::npos) {
        f.push_back(line.substr(s));
        break;
      }
      f.push_back(line.substr(s, bar - s));
      s = bar + 1;
    }
    if (f.size() < 5 || !validTitleId(f[0])) continue;
    UserData u;
    u.favorite = f[1] == "1";
    u.hidden = f[2] == "1";
    u.launchCount = static_cast<uint32_t>(std::strtoul(f[3].c_str(), nullptr, 10));
    u.lastLaunch = std::strtoll(f[4].c_str(), nullptr, 10);
    user_[f[0]] = u;
  }
  for (auto& g : games_) applyUserData(g);
  return true;
}

bool Library::saveUserData(const std::string& path) const {
  std::string t = "# titleId|favorite|hidden|launchCount|lastLaunchUnix\n";
  for (const auto& kv : user_) {
    const UserData& u = kv.second;
    if (!u.favorite && !u.hidden && u.launchCount == 0) continue;
    t += kv.first + "|" + (u.favorite ? "1" : "0") + "|" + (u.hidden ? "1" : "0") + "|" +
         std::to_string(u.launchCount) + "|" + std::to_string(u.lastLaunch) + "\n";
  }
  return fs::writeTextFile(path, t);
}

}  // namespace ps5ui
