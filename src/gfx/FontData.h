#pragma once
#include <cstddef>
#include <cstdint>

namespace ps5ui {

// One printable ASCII glyph (32..126) rasterised as 8-bit alpha.
struct Glyph {
  uint16_t adv16;   // advance in 1/16 pixel
  int8_t bx;        // left bearing (pixels)
  int8_t by;        // distance from baseline up to the top row (pixels)
  uint8_t w, h;     // bitmap size
  uint32_t offset;  // byte offset into FontFace::data
};

struct FontFace {
  int px;           // nominal pixel size in logical (1080p) units
  int ascent;
  int lineHeight;
  const Glyph* glyphs;  // indexed by (codepoint - 32)
  const uint8_t* data;
};

constexpr int kFontFaceCount = 4;
extern const FontFace kFontFaces[kFontFaceCount];

}  // namespace ps5ui
