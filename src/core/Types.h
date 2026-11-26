#pragma once
#include <cstdint>

namespace ps5ui {

// Logical canvas. All UI layout is authored for 1920x1080 and scaled to the
// internal render resolution by the Renderer.
constexpr int kLogicalWidth = 1920;
constexpr int kLogicalHeight = 1080;

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  float cx() const { return x + w * 0.5f; }
  float cy() const { return y + h * 0.5f; }
};

// 0xAARRGGBB (matches B8G8R8A8 little-endian memory layout).
constexpr uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}
constexpr uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) { return argb(255, r, g, b); }
constexpr uint32_t withAlpha(uint32_t c, uint32_t a) { return (c & 0x00FFFFFFu) | (a << 24); }

// DualShock 4 buttons (bit values match the console pad API).
namespace pad {
constexpr uint32_t kL3 = 0x00000002;
constexpr uint32_t kR3 = 0x00000004;
constexpr uint32_t kOptions = 0x00000008;
constexpr uint32_t kUp = 0x00000010;
constexpr uint32_t kRight = 0x00000020;
constexpr uint32_t kDown = 0x00000040;
constexpr uint32_t kLeft = 0x00000080;
constexpr uint32_t kL2 = 0x00000100;
constexpr uint32_t kR2 = 0x00000200;
constexpr uint32_t kL1 = 0x00000400;
constexpr uint32_t kR1 = 0x00000800;
constexpr uint32_t kTriangle = 0x00001000;
constexpr uint32_t kCircle = 0x00002000;
constexpr uint32_t kCross = 0x00004000;
constexpr uint32_t kSquare = 0x00008000;
constexpr uint32_t kTouchPad = 0x00100000;
}  // namespace pad

struct PadState {
  bool connected = false;
  uint32_t buttons = 0;
  uint8_t lx = 128, ly = 128, rx = 128, ry = 128;  // 0..255, 128 = centre
  uint8_t l2 = 0, r2 = 0;                           // analog triggers
};

}  // namespace ps5ui
