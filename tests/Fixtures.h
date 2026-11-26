#pragma once
// Test/dev helpers: build fake installed-title folders (no real game data involved).
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/FileSystem.h"

namespace fixtures {

struct SfoEntry {
  std::string key;
  bool isInt;
  std::string str;
  uint32_t num;
  uint32_t maxLen;
};

inline void put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(static_cast<uint8_t>(x & 255)); v.push_back(static_cast<uint8_t>(x >> 8)); }
inline void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>((x >> (8 * i)) & 255)); }

// Builds a PSF container; entries must already be sorted by key.
inline std::vector<uint8_t> buildSfo(const std::vector<SfoEntry>& e) {
  std::vector<uint8_t> keys, data;
  std::vector<uint8_t> index;
  for (const auto& en : e) {
    uint32_t koff = static_cast<uint32_t>(keys.size());
    keys.insert(keys.end(), en.key.begin(), en.key.end());
    keys.push_back(0);
    uint32_t doff = static_cast<uint32_t>(data.size());
    put16(index, static_cast<uint16_t>(koff));
    if (en.isInt) {
      put16(index, 0x0404); put32(index, 4); put32(index, 4); put32(index, doff);
      put32(data, en.num);
    } else {
      put16(index, 0x0204); put32(index, static_cast<uint32_t>(en.str.size() + 1)); put32(index, en.maxLen); put32(index, doff);
      data.insert(data.end(), en.str.begin(), en.str.end());
      for (uint32_t i = static_cast<uint32_t>(en.str.size()); i < en.maxLen; ++i) data.push_back(0);
    }
  }
  while (keys.size() % 4) keys.push_back(0);
  std::vector<uint8_t> out = {0, 'P', 'S', 'F'};
  put32(out, 0x101);
  uint32_t keyStart = 20 + static_cast<uint32_t>(e.size()) * 16;
  put32(out, keyStart);
  put32(out, keyStart + static_cast<uint32_t>(keys.size()));
  put32(out, static_cast<uint32_t>(e.size()));
  out.insert(out.end(), index.begin(), index.end());
  out.insert(out.end(), keys.begin(), keys.end());
  out.insert(out.end(), data.begin(), data.end());
  return out;
}

inline bool writeBytes(const std::string& path, const std::vector<uint8_t>& b) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  size_t n = std::fwrite(b.data(), 1, b.size(), f);
  std::fclose(f);
  return n == b.size();
}

// Creates <root>/<id>/sce_sys/param.sfo (and optionally copies an icon).
inline bool makeGame(const std::string& root, const std::string& id, const std::string& title,
                     const std::string& category = "gd", const std::string& ver = "01.00",
                     uint32_t sysVer = 0x05050000, const std::string& iconSrc = "") {
  std::string dir = ps5ui::fs::join(root, id);
  if (!ps5ui::fs::makeDirs(ps5ui::fs::join(dir, "sce_sys"))) return false;
  std::vector<SfoEntry> e = {
      {"APP_VER", false, ver, 0, 8},
      {"CATEGORY", false, category, 0, 4},
      {"CONTENT_ID", false, "IV0000-" + id + "_00-0000000000000000", 0, 48},
      {"SYSTEM_VER", true, "", sysVer, 4},
      {"TITLE", false, title, 0, 128},
      {"TITLE_ID", false, id, 0, 12},
  };
  if (!writeBytes(ps5ui::fs::join(dir, "sce_sys/param.sfo"), buildSfo(e))) return false;
  if (!iconSrc.empty()) {
    std::vector<uint8_t> icon;
    if (ps5ui::fs::readFile(iconSrc, icon, 8u << 20)) writeBytes(ps5ui::fs::join(dir, "sce_sys/icon0.png"), icon);
  }
  return true;
}

}  // namespace fixtures
