#include "core/FileSystem.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>
#include <cstring>

namespace ps5ui {
namespace fs {

bool exists(const std::string& path) {
  struct stat st;
  return ::stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path) {
  struct stat st;
  if (::stat(path.c_str(), &st) != 0) return false;
  return S_ISDIR(st.st_mode);
}

bool makeDirs(const std::string& path) {
  if (path.empty()) return false;
  std::string cur;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i == path.size() || path[i] == '/') {
      if (!cur.empty() && !isDirectory(cur)) {
        if (::mkdir(cur.c_str(), 0777) != 0 && !isDirectory(cur)) return false;
      }
    }
    if (i < path.size()) cur += path[i];
  }
  return isDirectory(path);
}

bool readFile(const std::string& path, std::vector<uint8_t>& out, size_t maxBytes) {
  out.clear();
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  uint8_t buf[4096];
  size_t n;
  bool ok = true;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
    if (out.size() + n > maxBytes) {
      ok = false;
      break;
    }
    out.insert(out.end(), buf, buf + n);
  }
  std::fclose(f);
  return ok;
}

bool readTextFile(const std::string& path, std::string& out) {
  std::vector<uint8_t> data;
  if (!readFile(path, data, 1u << 20)) return false;
  out.assign(reinterpret_cast<const char*>(data.data()), data.size());
  return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  size_t n = std::fwrite(text.data(), 1, text.size(), f);
  bool ok = (n == text.size());
  ok = (std::fclose(f) == 0) && ok;
  return ok;
}

std::vector<std::string> listDirectory(const std::string& path) {
  std::vector<std::string> names;
  DIR* d = ::opendir(path.c_str());
  if (!d) return names;
  while (struct dirent* e = ::readdir(d)) {
    if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
    names.emplace_back(e->d_name);
  }
  ::closedir(d);
  return names;
}

std::string join(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  if (a.back() == '/') return a + b;
  return a + "/" + b;
}

}  // namespace fs
}  // namespace ps5ui
