#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ps5ui {

struct Image {
  int w = 0, h = 0;
  std::vector<uint32_t> px;  // 0xAARRGGBB, tightly packed
  bool valid() const { return w > 0 && h > 0 && px.size() == static_cast<size_t>(w) * static_cast<size_t>(h); }
};

// Small self-contained PNG decoder (no external library). Supports non-interlaced
// 8-bit grey / grey+alpha / RGB / RGBA / palette (1-8 bit) and 16-bit (high byte used).
// Returns false on anything it can't handle; callers fall back to generated artwork.
bool decodePng(const uint8_t* data, size_t size, Image& out, int maxDimension = 2048);

// Box/area downscale to (dw x dh); used to make small thumbnails of big icons.
Image resizeBox(const Image& src, int dw, int dh);

}  // namespace ps5ui
