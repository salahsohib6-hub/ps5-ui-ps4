#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ps5ui {

struct GameEntry {
  std::string titleId;     // e.g. "CUSA00000"
  std::string title;
  std::string contentId;
  std::string version;     // VERSION / APP_VER from param.sfo
  std::string category;    // "gd" = game, etc.
  std::string path;        // install directory
  std::string iconPath;    // <path>/sce_sys/icon0.png (may not exist / be readable)
  uint32_t minFirmware = 0;  // SYSTEM_VER (0x05050000 = 5.05), 0 if unknown
  bool favorite = false;
  bool hidden = false;
  uint32_t launchCount = 0;
  int64_t lastLaunch = 0;  // unix seconds, 0 = never
};

enum class LibrarySort { TitleAZ = 0, TitleZA, TitleId, MostLaunched, RecentlyLaunched };
enum class LibraryFilter { All = 0, Favorites };

const char* sortName(LibrarySort s);
const char* filterName(LibraryFilter f);

// Scans installed PS4 titles by reading each <root>/<TITLE_ID>/sce_sys/param.sfo.
// Only metadata is read; no game content is touched or copied.
class Library {
 public:
  void setScanRoots(const std::vector<std::string>& roots) { roots_ = roots; }
  const std::vector<std::string>& scanRoots() const { return roots_; }

  // Rebuilds the entry list from disk (user data such as favourites is kept). Returns entry count.
  int scan();

  const std::vector<GameEntry>& all() const { return games_; }
  // Indices into all(), filtered and sorted.
  std::vector<int> view(LibrarySort sort, LibraryFilter filter, bool includeHidden) const;

  int indexOf(const std::string& titleId) const;
  bool toggleFavorite(const std::string& titleId);  // returns new state
  bool toggleHidden(const std::string& titleId);
  void recordLaunch(const std::string& titleId, int64_t unixSeconds);

  bool loadUserData(const std::string& path);
  bool saveUserData(const std::string& path) const;

  // Accepts game-type categories only ("gd", "gde", ...). Patches/add-ons are skipped.
  static bool isGameCategory(const std::string& category);

 private:
  struct UserData {
    bool favorite = false;
    bool hidden = false;
    uint32_t launchCount = 0;
    int64_t lastLaunch = 0;
  };
  void applyUserData(GameEntry& g) const;
  void syncUserData(const GameEntry& g);

  std::vector<std::string> roots_;
  std::vector<GameEntry> games_;
  std::map<std::string, UserData> user_;
};

}  // namespace ps5ui
