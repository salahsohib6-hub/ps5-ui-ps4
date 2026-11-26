#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/Types.h"
#include "gfx/Png.h"

namespace ps5ui {

// CPU-side pixel buffer (0xAARRGGBB). Rendering always happens in normal cached
// memory; only the finished frame is copied into video memory.
class Surface {
 public:
  void allocate(int w, int h) {
    w_ = w;
    h_ = h;
    buf_.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0xFF000000u);
  }
  int width() const { return w_; }
  int height() const { return h_; }
  int pitch() const { return w_; }
  uint32_t* data() { return buf_.data(); }
  const uint32_t* data() const { return buf_.data(); }

 private:
  int w_ = 0, h_ = 0;
  std::vector<uint32_t> buf_;
};

enum class FontId { Small = 0, Body = 1, Heading = 2, Title = 3 };

struct Point {
  float x, y;
};

// Immediate-mode 2D software renderer. All coordinates are logical 1920x1080
// units; they are scaled to the target surface (the "render scale").
class Renderer {
 public:
  // `scale` = pixels per logical unit (surface.width / 1920 for a full-screen target).
  void begin(Surface& target, float scale);
  float scale() const { return scale_; }
  Surface* target() const { return target_; }

  void setClip(const Rect& r);
  void clearClip();
  struct ClipState {
    int x0, y0, x1, y1;
  };
  ClipState saveClip() const { return {clipX0_, clipY0_, clipX1_, clipY1_}; }
  void restoreClip(const ClipState& c) { clipX0_ = c.x0; clipY0_ = c.y0; clipX1_ = c.x1; clipY1_ = c.y1; }
  void intersectClip(const Rect& r);  // narrows the current clip (never widens it)

  void clear(uint32_t color);
  void fillRect(const Rect& r, uint32_t color);
  void fillVGradient(const Rect& r, uint32_t top, uint32_t bottom);
  void fillRoundRect(const Rect& r, float radius, uint32_t color);
  void fillRoundRectGradient(const Rect& r, float radius, uint32_t top, uint32_t bottom);
  void strokeRoundRect(const Rect& r, float radius, float thickness, uint32_t color);
  void glowRing(const Rect& r, float radius, uint32_t color, int layers, float step);
  void fillCircle(float cx, float cy, float radius, uint32_t color);
  void strokeCircle(float cx, float cy, float radius, float thickness, uint32_t color);
  void fillPolygon(const std::vector<Point>& pts, uint32_t color);
  void drawLine(float x0, float y0, float x1, float y1, float thickness, uint32_t color);
  void drawRadialGlow(float cx, float cy, float radius, uint32_t color, float strength);

  // y is the top of the text line. Returns the advance width in logical units.
  float drawText(float x, float y, const std::string& text, FontId font, uint32_t color);
  float drawTextRight(float xRight, float y, const std::string& text, FontId font, uint32_t color);
  float drawTextCentered(float cx, float y, const std::string& text, FontId font, uint32_t color);
  static float measureText(const std::string& text, FontId font);
  static float lineHeight(FontId font);
  static std::string fitText(const std::string& text, FontId font, float maxWidth);

  void drawImage(const Image& img, const Rect& dst, float radius, bool bilinear, uint32_t tint = 0xFFFFFFFFu);

 private:
  struct PxRect {
    int x0, y0, x1, y1;
  };
  PxRect toPx(const Rect& r) const;
  PxRect clipped(PxRect r) const;
  void blendRowSpan(int y, int xs, int xe, uint32_t color, uint32_t alpha);
  void roundRectPx(const PxRect& r, float radiusPx, uint32_t top, uint32_t bottom);
  void strokeRoundRectPx(const PxRect& r, float radiusPx, float thicknessPx, uint32_t color);

  std::vector<int> imgU0_, imgU1_;      // scratch for drawImage (avoids TLS / per-call allocation)
  std::vector<uint32_t> imgW_;
  Surface* target_ = nullptr;
  float scale_ = 1.0f;
  int clipX0_ = 0, clipY0_ = 0, clipX1_ = 0, clipY1_ = 0;
};

inline uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  auto ch = [&](int sh) {
    float v = static_cast<float>((a >> sh) & 255) * (1 - t) + static_cast<float>((b >> sh) & 255) * t;
    return static_cast<uint32_t>(v + 0.5f);
  };
  return (ch(24) << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

}  // namespace ps5ui
