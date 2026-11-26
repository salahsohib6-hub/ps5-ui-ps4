#include "gfx/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "gfx/FontData.h"

namespace ps5ui {

namespace {

inline uint32_t blendPx(uint32_t dst, uint32_t src, uint32_t a) {
  if (a == 0) return dst;
  if (a >= 255) return 0xFF000000u | (src & 0x00FFFFFFu);
  uint32_t ia = 255 - a;
  uint32_t rb = (dst & 0x00FF00FFu) * ia + (src & 0x00FF00FFu) * a + 0x00800080u;
  rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
  uint32_t g = (dst & 0x0000FF00u) * ia + (src & 0x0000FF00u) * a + 0x00008000u;
  g = ((g + (g >> 8)) >> 8) & 0x0000FF00u;
  return 0xFF000000u | rb | g;
}

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// Signed distance to a rounded rectangle (negative inside).
inline float sdRoundRect(float px, float py, float cx, float cy, float hx, float hy, float r) {
  float qx = std::fabs(px - cx) - (hx - r);
  float qy = std::fabs(py - cy) - (hy - r);
  float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
  float inside = std::min(std::max(qx, qy), 0.0f);
  // Straight edges (one axis inside) need no square root.
  float len = (ox == 0.0f) ? oy : (oy == 0.0f ? ox : std::sqrt(ox * ox + oy * oy));
  return len + inside - r;
}

}  // namespace

void Renderer::begin(Surface& target, float scale) {
  target_ = &target;
  scale_ = scale;
  clearClip();
}

void Renderer::setClip(const Rect& r) {
  PxRect p = toPx(r);
  clipX0_ = std::max(0, p.x0);
  clipY0_ = std::max(0, p.y0);
  clipX1_ = std::min(target_->width(), p.x1);
  clipY1_ = std::min(target_->height(), p.y1);
}

void Renderer::intersectClip(const Rect& r) {
  PxRect p = toPx(r);
  clipX0_ = std::max(clipX0_, p.x0);
  clipY0_ = std::max(clipY0_, p.y0);
  clipX1_ = std::min(clipX1_, p.x1);
  clipY1_ = std::min(clipY1_, p.y1);
}

void Renderer::clearClip() {
  clipX0_ = clipY0_ = 0;
  clipX1_ = target_ ? target_->width() : 0;
  clipY1_ = target_ ? target_->height() : 0;
}

Renderer::PxRect Renderer::toPx(const Rect& r) const {
  return {static_cast<int>(std::lround(r.x * scale_)), static_cast<int>(std::lround(r.y * scale_)),
          static_cast<int>(std::lround((r.x + r.w) * scale_)), static_cast<int>(std::lround((r.y + r.h) * scale_))};
}

Renderer::PxRect Renderer::clipped(PxRect r) const {
  return {std::max(r.x0, clipX0_), std::max(r.y0, clipY0_), std::min(r.x1, clipX1_), std::min(r.y1, clipY1_)};
}

void Renderer::clear(uint32_t color) {
  uint32_t* d = target_->data();
  size_t n = static_cast<size_t>(target_->width()) * static_cast<size_t>(target_->height());
  for (size_t i = 0; i < n; ++i) d[i] = color | 0xFF000000u;
}

void Renderer::blendRowSpan(int y, int xs, int xe, uint32_t color, uint32_t alpha) {
  if (y < clipY0_ || y >= clipY1_) return;
  xs = std::max(xs, clipX0_);
  xe = std::min(xe, clipX1_);
  if (xs >= xe || alpha == 0) return;
  uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
  if (alpha >= 255) {
    uint32_t c = 0xFF000000u | (color & 0x00FFFFFFu);
    for (int x = xs; x < xe; ++x) row[x] = c;
  } else {
    for (int x = xs; x < xe; ++x) row[x] = blendPx(row[x], color, alpha);
  }
}

void Renderer::fillRect(const Rect& r, uint32_t color) {
  PxRect p = clipped(toPx(r));
  uint32_t a = color >> 24;
  for (int y = p.y0; y < p.y1; ++y) blendRowSpan(y, p.x0, p.x1, color, a);
}

void Renderer::fillVGradient(const Rect& r, uint32_t top, uint32_t bottom) {
  PxRect full = toPx(r);
  PxRect p = clipped(full);
  int hgt = std::max(1, full.y1 - full.y0 - 1);
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t c = lerpColor(top, bottom, static_cast<float>(y - full.y0) / static_cast<float>(hgt));
    blendRowSpan(y, p.x0, p.x1, c, c >> 24);
  }
}

void Renderer::roundRectPx(const PxRect& full, float rad, uint32_t top, uint32_t bottom) {
  PxRect p = clipped(full);
  if (p.x0 >= p.x1 || p.y0 >= p.y1) return;
  float maxR = static_cast<float>(std::min(full.x1 - full.x0, full.y1 - full.y0)) * 0.5f;
  rad = std::min(rad, maxR);
  int r = static_cast<int>(std::ceil(rad));
  float fx0 = static_cast<float>(full.x0), fy0 = static_cast<float>(full.y0);
  float fx1 = static_cast<float>(full.x1), fy1 = static_cast<float>(full.y1);
  int hgt = std::max(1, full.y1 - full.y0 - 1);
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t c = (top == bottom) ? top : lerpColor(top, bottom, static_cast<float>(y - full.y0) / static_cast<float>(hgt));
    uint32_t ca = c >> 24;
    bool corner = (y < full.y0 + r) || (y >= full.y1 - r);
    if (!corner || r == 0) {
      blendRowSpan(y, p.x0, p.x1, c, ca);
      continue;
    }
    float py = static_cast<float>(y) + 0.5f;
    float cyc = (y < full.y0 + r) ? fy0 + rad : fy1 - rad;
    float dy = py - cyc;
    // solid middle part
    int midS = full.x0 + r, midE = full.x1 - r;
    if (midS < midE) blendRowSpan(y, midS, midE, c, ca);
    // left & right corner zones (per-pixel coverage)
    for (int side = 0; side < 2; ++side) {
      int xs = side == 0 ? full.x0 : midE;
      int xe = side == 0 ? midS : full.x1;
      xs = std::max(xs, p.x0);
      xe = std::min(xe, p.x1);
      float ccx = side == 0 ? fx0 + rad : fx1 - rad;
      if (xs >= xe) continue;
      uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
      for (int x = xs; x < xe; ++x) {
        float dx = static_cast<float>(x) + 0.5f - ccx;
        float d = std::sqrt(dx * dx + dy * dy);
        float cov = clamp01(rad - d + 0.5f);
        uint32_t a = static_cast<uint32_t>(cov * static_cast<float>(ca) + 0.5f);
        row[x] = blendPx(row[x], c, a);
      }
    }
  }
}

void Renderer::fillRoundRect(const Rect& r, float radius, uint32_t color) {
  roundRectPx(toPx(r), radius * scale_, color, color);
}

void Renderer::fillRoundRectGradient(const Rect& r, float radius, uint32_t top, uint32_t bottom) {
  roundRectPx(toPx(r), radius * scale_, top, bottom);
}

void Renderer::strokeRoundRectPx(const PxRect& full, float rad, float t, uint32_t color) {
  PxRect p = clipped(full);
  if (p.x0 >= p.x1 || p.y0 >= p.y1) return;
  float maxR = static_cast<float>(std::min(full.x1 - full.x0, full.y1 - full.y0)) * 0.5f;
  rad = std::min(rad, maxR);
  float cx = (static_cast<float>(full.x0) + static_cast<float>(full.x1)) * 0.5f;
  float cy = (static_cast<float>(full.y0) + static_cast<float>(full.y1)) * 0.5f;
  float hx = static_cast<float>(full.x1 - full.x0) * 0.5f, hy = static_cast<float>(full.y1 - full.y0) * 0.5f;
  int band = static_cast<int>(std::ceil(std::max(rad, t))) + 1;
  int xa = full.x0 + band, xb = full.x1 - band;
  if (xa > xb) xa = xb = (full.x0 + full.x1) / 2;
  uint32_t ca = color >> 24;
  auto perPixel = [&](int y, uint32_t* row, int xs, int xe) {
    xs = std::max(xs, p.x0);
    xe = std::min(xe, p.x1);
    float py = static_cast<float>(y) + 0.5f;
    for (int x = xs; x < xe; ++x) {
      float d = sdRoundRect(static_cast<float>(x) + 0.5f, py, cx, cy, hx, hy, rad);
      float cov = clamp01(0.5f - d) * clamp01(d + t + 0.5f);
      if (cov <= 0) continue;
      row[x] = blendPx(row[x], color, static_cast<uint32_t>(cov * static_cast<float>(ca) + 0.5f));
    }
  };
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
    bool fullRow = (y < full.y0 + band) || (y >= full.y1 - band);
    perPixel(y, row, full.x0, xa);          // left corner / left edge
    perPixel(y, row, xb, full.x1);          // right corner / right edge
    if (fullRow && xa < xb) {               // straight top/bottom edge: constant along the row
      float d = sdRoundRect(static_cast<float>(xa) + 0.5f, static_cast<float>(y) + 0.5f, cx, cy, hx, hy, rad);
      float cov = clamp01(0.5f - d) * clamp01(d + t + 0.5f);
      if (cov > 0) blendRowSpan(y, xa, xb, color, static_cast<uint32_t>(cov * static_cast<float>(ca) + 0.5f));
    }
  }
}

void Renderer::strokeRoundRect(const Rect& r, float radius, float thickness, uint32_t color) {
  strokeRoundRectPx(toPx(r), radius * scale_, std::max(1.0f, thickness * scale_), color);
}

void Renderer::glowRing(const Rect& r, float radius, uint32_t color, int layers, float step) {
  uint32_t base = color >> 24;
  for (int i = layers; i >= 1; --i) {
    float f = 1.0f - static_cast<float>(i) / static_cast<float>(layers + 1);
    uint32_t a = static_cast<uint32_t>(static_cast<float>(base) * f * f);
    if (a == 0) continue;
    float e = static_cast<float>(i) * step;
    Rect rr{r.x - e, r.y - e, r.w + 2 * e, r.h + 2 * e};
    strokeRoundRect(rr, radius + e, step + 0.5f, withAlpha(color, a));
  }
}

void Renderer::fillCircle(float cx, float cy, float radius, uint32_t color) {
  float s = scale_;
  float pcx = cx * s, pcy = cy * s, pr = radius * s;
  PxRect p = clipped({static_cast<int>(std::floor(pcx - pr - 1)), static_cast<int>(std::floor(pcy - pr - 1)),
                      static_cast<int>(std::ceil(pcx + pr + 1)), static_cast<int>(std::ceil(pcy + pr + 1))});
  uint32_t ca = color >> 24;
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
    float dy = static_cast<float>(y) + 0.5f - pcy;
    const float inner = std::max(0.0f, pr - 1.0f), outer = pr + 1.0f;
    const float inner2 = inner * inner, outer2 = outer * outer;
    for (int x = p.x0; x < p.x1; ++x) {
      float dx = static_cast<float>(x) + 0.5f - pcx;
      float d2 = dx * dx + dy * dy;
      if (d2 >= outer2) continue;
      if (d2 <= inner2) {
        row[x] = blendPx(row[x], color, ca);
        continue;
      }
      float cov = clamp01(pr - std::sqrt(d2) + 0.5f);
      if (cov > 0) row[x] = blendPx(row[x], color, static_cast<uint32_t>(cov * static_cast<float>(ca) + 0.5f));
    }
  }
}

void Renderer::strokeCircle(float cx, float cy, float radius, float thickness, uint32_t color) {
  float s = scale_;
  float pcx = cx * s, pcy = cy * s, pr = radius * s, pt = std::max(1.0f, thickness * s);
  PxRect p = clipped({static_cast<int>(std::floor(pcx - pr - 1)), static_cast<int>(std::floor(pcy - pr - 1)),
                      static_cast<int>(std::ceil(pcx + pr + 1)), static_cast<int>(std::ceil(pcy + pr + 1))});
  uint32_t ca = color >> 24;
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
    float dy = static_cast<float>(y) + 0.5f - pcy;
    for (int x = p.x0; x < p.x1; ++x) {
      float dx = static_cast<float>(x) + 0.5f - pcx;
      float d = std::sqrt(dx * dx + dy * dy) - pr;
      float cov = clamp01(0.5f - d) * clamp01(d + pt + 0.5f);
      if (cov > 0) row[x] = blendPx(row[x], color, static_cast<uint32_t>(cov * static_cast<float>(ca) + 0.5f));
    }
  }
}

void Renderer::fillPolygon(const std::vector<Point>& pts, uint32_t color) {
  if (pts.size() < 3) return;
  float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
  std::vector<Point> p(pts.size());
  for (size_t i = 0; i < pts.size(); ++i) {
    p[i] = {pts[i].x * scale_, pts[i].y * scale_};
    minX = std::min(minX, p[i].x);
    maxX = std::max(maxX, p[i].x);
    minY = std::min(minY, p[i].y);
    maxY = std::max(maxY, p[i].y);
  }
  PxRect b = clipped({static_cast<int>(std::floor(minX)), static_cast<int>(std::floor(minY)),
                      static_cast<int>(std::ceil(maxX)), static_cast<int>(std::ceil(maxY))});
  uint32_t ca = color >> 24;
  const int ss = 4;
  for (int y = b.y0; y < b.y1; ++y) {
    uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
    for (int x = b.x0; x < b.x1; ++x) {
      int hits = 0;
      for (int sy = 0; sy < ss; ++sy) {
        float py = static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / ss;
        for (int sx = 0; sx < ss; ++sx) {
          float px = static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / ss;
          bool in = false;
          for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
            if (((p[i].y > py) != (p[j].y > py)) &&
                (px < (p[j].x - p[i].x) * (py - p[i].y) / (p[j].y - p[i].y) + p[i].x))
              in = !in;
          }
          hits += in;
        }
      }
      if (hits) row[x] = blendPx(row[x], color, static_cast<uint32_t>(hits * static_cast<int>(ca) / (ss * ss)));
    }
  }
}

void Renderer::drawLine(float x0, float y0, float x1, float y1, float thickness, uint32_t color) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-4f) return;
  float nx = -dy / len * thickness * 0.5f, ny = dx / len * thickness * 0.5f;
  fillPolygon({{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}}, color);
}

void Renderer::drawRadialGlow(float cx, float cy, float radius, uint32_t color, float strength) {
  float s = scale_;
  float pcx = cx * s, pcy = cy * s, pr = radius * s;
  PxRect p = clipped({static_cast<int>(std::floor(pcx - pr)), static_cast<int>(std::floor(pcy - pr)),
                      static_cast<int>(std::ceil(pcx + pr)), static_cast<int>(std::ceil(pcy + pr))});
  static float lut[257];
  static bool init = false;
  if (!init) {
    for (int i = 0; i <= 256; ++i) {
      float t = 1.0f - static_cast<float>(i) / 256.0f;
      lut[i] = t * t * t;  // soft falloff
    }
    init = true;
  }
  float inv = 256.0f / (pr * pr);
  float k = strength * static_cast<float>(color >> 24);
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
    float dy = static_cast<float>(y) + 0.5f - pcy;
    float dy2 = dy * dy;
    for (int x = p.x0; x < p.x1; ++x) {
      float dx = static_cast<float>(x) + 0.5f - pcx;
      int i = static_cast<int>((dx * dx + dy2) * inv);
      if (i >= 256) continue;
      uint32_t a = static_cast<uint32_t>(lut[i] * k);
      if (a) row[x] = blendPx(row[x], color, a);
    }
  }
}

// ---- text ---------------------------------------------------------------------

namespace {
const FontFace& face(FontId f) { return kFontFaces[static_cast<int>(f)]; }
inline const Glyph& glyphFor(const FontFace& ff, unsigned char c) {
  if (c < 32 || c > 126) c = '?';
  return ff.glyphs[c - 32];
}
}  // namespace

float Renderer::measureText(const std::string& text, FontId font) {
  const FontFace& ff = face(font);
  int adv = 0;
  for (unsigned char c : text) adv += glyphFor(ff, c).adv16;
  return static_cast<float>(adv) / 16.0f;
}

float Renderer::lineHeight(FontId font) { return static_cast<float>(face(font).lineHeight); }

std::string Renderer::fitText(const std::string& text, FontId font, float maxWidth) {
  if (measureText(text, font) <= maxWidth) return text;
  std::string t = text;
  while (!t.empty() && measureText(t + "...", font) > maxWidth) t.pop_back();
  return t + "...";
}

float Renderer::drawText(float x, float y, const std::string& text, FontId font, uint32_t color) {
  const FontFace& ff = face(font);
  float penX = x;
  float baseline = y + static_cast<float>(ff.ascent);
  uint32_t ca = color >> 24;
  const bool unscaled = std::fabs(scale_ - 1.0f) < 1e-3f;
  for (unsigned char c : text) {
    const Glyph& g = glyphFor(ff, c);
    if (g.w && g.h) {
      const uint8_t* bmp = ff.data + g.offset;
      float gx = penX + static_cast<float>(g.bx);
      float gy = baseline - static_cast<float>(g.by);
      if (unscaled) {
        int x0 = static_cast<int>(std::lround(gx)), y0 = static_cast<int>(std::lround(gy));
        for (int j = 0; j < g.h; ++j) {
          int yy = y0 + j;
          if (yy < clipY0_ || yy >= clipY1_) continue;
          uint32_t* row = target_->data() + static_cast<size_t>(yy) * static_cast<size_t>(target_->pitch());
          for (int i = 0; i < g.w; ++i) {
            int xx = x0 + i;
            if (xx < clipX0_ || xx >= clipX1_) continue;
            uint32_t a = bmp[j * g.w + i];
            if (a) row[xx] = blendPx(row[xx], color, a * ca / 255);
          }
        }
      } else {
        int x0 = static_cast<int>(std::lround(gx * scale_)), y0 = static_cast<int>(std::lround(gy * scale_));
        int dw = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.w) * scale_)));
        int dh = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.h) * scale_)));
        for (int j = 0; j < dh; ++j) {
          int yy = y0 + j;
          if (yy < clipY0_ || yy >= clipY1_) continue;
          // Area-average over the source rows/cols that map to this pixel (box filter).
          int sy0 = j * g.h / dh, sy1 = std::max(sy0 + 1, (j + 1) * g.h / dh);
          uint32_t* row = target_->data() + static_cast<size_t>(yy) * static_cast<size_t>(target_->pitch());
          for (int i = 0; i < dw; ++i) {
            int xx = x0 + i;
            if (xx < clipX0_ || xx >= clipX1_) continue;
            int sx0 = i * g.w / dw, sx1 = std::max(sx0 + 1, (i + 1) * g.w / dw);
            uint32_t sum = 0, n = 0;
            for (int sy = sy0; sy < sy1 && sy < g.h; ++sy)
              for (int sx = sx0; sx < sx1 && sx < g.w; ++sx) {
                sum += bmp[sy * g.w + sx];
                ++n;
              }
            uint32_t a = n ? sum / n : 0;
            if (a) row[xx] = blendPx(row[xx], color, a * ca / 255);
          }
        }
      }
    }
    penX += static_cast<float>(g.adv16) / 16.0f;
  }
  return penX - x;
}

float Renderer::drawTextRight(float xRight, float y, const std::string& text, FontId font, uint32_t color) {
  float w = measureText(text, font);
  drawText(xRight - w, y, text, font, color);
  return w;
}

float Renderer::drawTextCentered(float cx, float y, const std::string& text, FontId font, uint32_t color) {
  float w = measureText(text, font);
  drawText(cx - w * 0.5f, y, text, font, color);
  return w;
}

// ---- images -------------------------------------------------------------------

void Renderer::drawImage(const Image& img, const Rect& dst, float radius, bool bilinear, uint32_t tint) {
  if (!img.valid()) return;
  PxRect full = toPx(dst);
  PxRect p = clipped(full);
  int dw = full.x1 - full.x0, dh = full.y1 - full.y0;
  if (dw <= 0 || dh <= 0 || p.x0 >= p.x1 || p.y0 >= p.y1) return;
  float rad = std::min(radius * scale_, static_cast<float>(std::min(dw, dh)) * 0.5f);
  float cx = (static_cast<float>(full.x0) + static_cast<float>(full.x1)) * 0.5f;
  float cy = (static_cast<float>(full.y0) + static_cast<float>(full.y1)) * 0.5f;
  float hx = static_cast<float>(dw) * 0.5f, hy = static_cast<float>(dh) * 0.5f;
  uint32_t tintA = tint >> 24;

  // Per-column source mapping, computed once per call (not per pixel).
  const int n = p.x1 - p.x0;
  std::vector<int>& cu0 = imgU0_;
  std::vector<int>& cu1 = imgU1_;
  std::vector<uint32_t>& cw = imgW_;
  cu0.resize(static_cast<size_t>(n));
  cu1.resize(static_cast<size_t>(n));
  cw.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    float u = (static_cast<float>(p.x0 + i - full.x0) + 0.5f) * static_cast<float>(img.w) / static_cast<float>(dw);
    if (bilinear) {
      float fu = u - 0.5f;
      int a0 = static_cast<int>(std::floor(fu));
      cw[static_cast<size_t>(i)] = static_cast<uint32_t>((fu - static_cast<float>(a0)) * 256.0f);
      cu1[static_cast<size_t>(i)] = std::max(0, std::min(a0 + 1, img.w - 1));
      cu0[static_cast<size_t>(i)] = std::max(0, std::min(a0, img.w - 1));
    } else {
      cu0[static_cast<size_t>(i)] = std::min(img.w - 1, static_cast<int>(u));
    }
  }
  auto L = [](uint32_t a, uint32_t b, uint32_t t) {
    uint32_t rb = (((a & 0x00FF00FFu) * (256 - t) + (b & 0x00FF00FFu) * t) >> 8) & 0x00FF00FFu;
    uint32_t ag = ((((a >> 8) & 0x00FF00FFu) * (256 - t) + ((b >> 8) & 0x00FF00FFu) * t)) & 0xFF00FF00u;
    return rb | ag;
  };
  const int cornerBand = rad > 0 ? static_cast<int>(rad) + 1 : 0;
  for (int y = p.y0; y < p.y1; ++y) {
    uint32_t* row = target_->data() + static_cast<size_t>(y) * static_cast<size_t>(target_->pitch());
    float v = (static_cast<float>(y - full.y0) + 0.5f) * static_cast<float>(img.h) / static_cast<float>(dh);
    bool nearCornerRow = cornerBand > 0 && (y < full.y0 + cornerBand || y >= full.y1 - cornerBand);
    const uint32_t* r0;
    const uint32_t* r1 = nullptr;
    uint32_t tv = 0;
    if (bilinear) {
      float fv = v - 0.5f;
      int b0 = static_cast<int>(std::floor(fv));
      tv = static_cast<uint32_t>((fv - static_cast<float>(b0)) * 256.0f);
      int b1 = std::max(0, std::min(b0 + 1, img.h - 1));
      b0 = std::max(0, std::min(b0, img.h - 1));
      r0 = img.px.data() + static_cast<size_t>(b0) * static_cast<size_t>(img.w);
      r1 = img.px.data() + static_cast<size_t>(b1) * static_cast<size_t>(img.w);
    } else {
      int sv = std::min(img.h - 1, static_cast<int>(v));
      r0 = img.px.data() + static_cast<size_t>(sv) * static_cast<size_t>(img.w);
    }
    for (int i = 0; i < n; ++i) {
      int x = p.x0 + i;
      uint32_t c = bilinear ? L(L(r0[cu0[static_cast<size_t>(i)]], r0[cu1[static_cast<size_t>(i)]], cw[static_cast<size_t>(i)]),
                                L(r1[cu0[static_cast<size_t>(i)]], r1[cu1[static_cast<size_t>(i)]], cw[static_cast<size_t>(i)]), tv)
                            : r0[cu0[static_cast<size_t>(i)]];
      uint32_t a = (c >> 24) * tintA / 255;
      if (nearCornerRow || (cornerBand > 0 && (x < full.x0 + cornerBand || x >= full.x1 - cornerBand))) {
        float d = sdRoundRect(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, cx, cy, hx, hy, rad);
        a = static_cast<uint32_t>(clamp01(0.5f - d) * static_cast<float>(a));
      }
      if (a >= 255) row[x] = 0xFF000000u | (c & 0x00FFFFFFu);
      else if (a) row[x] = blendPx(row[x], c, a);
    }
  }
}

}  // namespace ps5ui
