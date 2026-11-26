#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace ps5ui {

// Minimal reader for the PSF/param.sfo container (plain key/value data).
struct SfoData {
  std::map<std::string, std::string> strings;
  std::map<std::string, uint32_t> ints;

  bool getString(const std::string& key, std::string& out) const;
  bool getInt(const std::string& key, uint32_t& out) const;
};

// Returns false for malformed / truncated data. Never reads out of bounds.
bool parseSfo(const uint8_t* data, size_t size, SfoData& out);

}  // namespace ps5ui
