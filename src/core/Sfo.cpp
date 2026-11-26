#include "core/Sfo.h"

namespace ps5ui {

namespace {
uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

bool SfoData::getString(const std::string& key, std::string& out) const {
  auto it = strings.find(key);
  if (it == strings.end()) return false;
  out = it->second;
  return true;
}

bool SfoData::getInt(const std::string& key, uint32_t& out) const {
  auto it = ints.find(key);
  if (it == ints.end()) return false;
  out = it->second;
  return true;
}

bool parseSfo(const uint8_t* data, size_t size, SfoData& out) {
  out.strings.clear();
  out.ints.clear();
  if (!data || size < 20) return false;
  if (data[0] != 0x00 || data[1] != 'P' || data[2] != 'S' || data[3] != 'F') return false;
  uint32_t keyStart = rd32(data + 8);
  uint32_t dataStart = rd32(data + 12);
  uint32_t count = rd32(data + 16);
  if (count > 512) return false;
  if (20u + count * 16u > size) return false;
  if (keyStart > size || dataStart > size) return false;

  for (uint32_t i = 0; i < count; ++i) {
    const uint8_t* e = data + 20 + i * 16;
    uint32_t keyOff = rd16(e);
    uint16_t fmt = rd16(e + 2);
    uint32_t len = rd32(e + 4);
    uint32_t maxLen = rd32(e + 8);
    uint32_t dOff = rd32(e + 12);
    if (len > maxLen) return false;

    size_t kpos = static_cast<size_t>(keyStart) + keyOff;
    if (kpos >= size) return false;
    std::string key;
    while (kpos < size && data[kpos] != 0 && key.size() < 64) key.push_back(static_cast<char>(data[kpos++]));
    if (kpos >= size || data[kpos] != 0) return false;  // unterminated key

    size_t dpos = static_cast<size_t>(dataStart) + dOff;
    if (dpos > size || len > size - dpos) return false;
    const uint8_t* d = data + dpos;

    if (fmt == 0x0404) {
      if (len != 4) return false;
      out.ints[key] = rd32(d);
    } else if (fmt == 0x0204 || fmt == 0x0004) {
      size_t n = len;
      while (n > 0 && d[n - 1] == 0) --n;
      out.strings[key] = std::string(reinterpret_cast<const char*>(d), n);
    }
    // Unknown formats are ignored.
  }
  return true;
}

}  // namespace ps5ui
