#include "ui/Widgets.h"

#include <cctype>
#include <cmath>

namespace ps5ui {
namespace ui {

uint32_t hsv(float h, float s, float v) {
  float c = v * s;
  float hp = std::fmod(h, 360.0f) / 60.0f;
  float x = c * (1 - std::fabs(std::fmod(hp, 2.0f) - 1));
  float r = 0, g = 0, b = 0;
  if (hp < 1) { r = c; g = x; }
  else if (hp < 2) { r = x; g = c; }
  else if (hp < 3) { g = c; b = x; }
  else if (hp < 4) { g = x; b = c; }
  else if (hp < 5) { r = x; b = c; }
  else { r = c; b = x; }
  float m = v - c;
  auto to8 = [](float f) { return static_cast<uint32_t>(f * 255.0f + 0.5f); };
  return rgb(to8(r + m), to8(g + m), to8(b + m));
}

uint32_t hashString(const std::string& s) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (unsigned char c : s) {
    h ^= c;
    h *= 16777619u;
  }
  return h;
}

void drawPadGlyph(Renderer& r, PadGlyph g, float cx, float cy, float rad) {
  r.fillCircle(cx, cy, rad, rgb(34, 42, 78));
  r.strokeCircle(cx, cy, rad, 1.5f, argb(120, 170, 190, 255));
  float s = rad * 0.5f;
  switch (g) {
    case PadGlyph::Cross: {
      uint32_t c = rgb(110, 170, 255);
      r.drawLine(cx - s, cy - s, cx + s, cy + s, 2.6f, c);
      r.drawLine(cx - s, cy + s, cx + s, cy - s, 2.6f, c);
      break;
    }
    case PadGlyph::Circle:
      r.strokeCircle(cx, cy, s * 1.05f, 2.6f, rgb(255, 118, 130));
      break;
    case PadGlyph::Square:
      r.strokeRoundRect({cx - s, cy - s, 2 * s, 2 * s}, 2.0f, 2.6f, rgb(240, 140, 210));
      break;
    case PadGlyph::Triangle: {
      uint32_t c = rgb(110, 224, 170);
      float a = s * 1.15f;
      r.drawLine(cx, cy - a, cx + a, cy + a * 0.8f, 2.6f, c);
      r.drawLine(cx + a, cy + a * 0.8f, cx - a, cy + a * 0.8f, 2.6f, c);
      r.drawLine(cx - a, cy + a * 0.8f, cx, cy - a, 2.6f, c);
      break;
    }
  }
}

float drawGlyphHint(Renderer& r, float x, float y, PadGlyph g, const std::string& label) {
  const float rad = 17.0f;
  drawPadGlyph(r, g, x + rad, y + 13, rad);
  float w = r.drawText(x + 2 * rad + 10, y, label, FontId::Small, theme::kTextDim);
  return 2 * rad + 10 + w;
}

float drawKeyHint(Renderer& r, float x, float y, const std::string& key, const std::string& label) {
  float kw = Renderer::measureText(key, FontId::Small) + 22;
  r.fillRoundRect({x, y - 2, kw, 30}, 15, rgb(34, 42, 78));
  r.strokeRoundRect({x, y - 2, kw, 30}, 15, 1.5f, argb(120, 170, 190, 255));
  r.drawText(x + 11, y, key, FontId::Small, rgb(200, 212, 245));
  float w = r.drawText(x + kw + 10, y, label, FontId::Small, theme::kTextDim);
  return kw + 10 + w;
}

void drawStar(Renderer& r, float cx, float cy, float rad, uint32_t color) {
  std::vector<Point> pts;
  for (int i = 0; i < 10; ++i) {
    float a = -3.14159265f / 2 + static_cast<float>(i) * 3.14159265f / 5;
    float rr = (i % 2 == 0) ? rad : rad * 0.45f;
    pts.push_back({cx + std::cos(a) * rr, cy + std::sin(a) * rr});
  }
  r.fillPolygon(pts, color);
}

void drawTabIcon(Renderer& r, int tab, float cx, float cy, float s, uint32_t c) {
  switch (tab) {
    case 0:  // Home: house
      r.fillPolygon({{cx - s, cy - s * 0.1f}, {cx, cy - s}, {cx + s, cy - s * 0.1f}}, c);
      r.fillRoundRect({cx - s * 0.7f, cy - s * 0.15f, s * 1.4f, s * 1.05f}, 3, c);
      break;
    case 1:  // Library: 2x2 grid
      for (int i = 0; i < 4; ++i)
        r.fillRoundRect({cx - s + static_cast<float>(i % 2) * s * 1.1f, cy - s + static_cast<float>(i / 2) * s * 1.1f, s * 0.9f, s * 0.9f}, 3, c);
      break;
    case 2:  // Performance: bars
      r.fillRoundRect({cx - s, cy + s * 0.1f, s * 0.5f, s * 0.9f}, 2, c);
      r.fillRoundRect({cx - s * 0.25f, cy - s * 0.4f, s * 0.5f, s * 1.4f}, 2, c);
      r.fillRoundRect({cx + s * 0.5f, cy - s, s * 0.5f, s * 2.0f}, 2, c);
      break;
    default:  // Settings: sliders
      for (int i = 0; i < 3; ++i) {
        float yy = cy - s * 0.8f + static_cast<float>(i) * s * 0.8f;
        r.fillRoundRect({cx - s, yy - 1.5f, s * 2, 3}, 1.5f, c);
        r.fillCircle(cx - s * 0.4f + static_cast<float>((i * 2) % 3) * s * 0.5f, yy, s * 0.3f, c);
      }
      break;
  }
}

void drawPanel(Renderer& r, const Rect& rc, float radius) {
  r.fillRoundRect(rc, radius, theme::kPanel);
  r.strokeRoundRect(rc, radius, 1.5f, theme::kPanelStroke);
}

void drawSwitch(Renderer& r, const Rect& rc, bool on, uint32_t accent) {
  r.fillRoundRect(rc, rc.h / 2, on ? accent : rgb(52, 62, 98));
  float k = rc.h - 8;
  float x = on ? rc.x + rc.w - k - 4 : rc.x + 4;
  r.fillCircle(x + k / 2, rc.y + rc.h / 2, k / 2, rgb(245, 248, 255));
}

void drawCover(Renderer& r, const GameEntry& g, const Image* icon, const Rect& rc, float radius, bool bilinear) {
  if (icon && icon->valid()) {
    r.drawImage(*icon, rc, radius, bilinear);
    return;
  }
  // Generated artwork derived from the title ID: stable colours, no copyrighted assets.
  uint32_t h = hashString(g.titleId);
  float hue = static_cast<float>(h % 360);
  uint32_t top = hsv(hue, 0.55f, 0.62f);
  uint32_t bot = hsv(std::fmod(hue + 40.0f, 360.0f), 0.70f, 0.28f);
  r.fillRoundRectGradient(rc, radius, top, bot);
  Renderer::ClipState saved = r.saveClip();
  r.intersectClip(rc);
  r.fillCircle(rc.x + rc.w * 0.74f, rc.y + rc.h * 0.26f, rc.w * 0.22f, argb(36, 255, 255, 255));
  r.fillCircle(rc.x + rc.w * 0.22f, rc.y + rc.h * 0.78f, rc.w * 0.24f, argb(26, 255, 255, 255));
  r.restoreClip(saved);
  std::string initials;
  bool startWord = true;
  for (char c : g.title) {
    if (c == ' ') { startWord = true; continue; }
    if (startWord && std::isalnum(static_cast<unsigned char>(c)) && initials.size() < 2) {
      initials += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    startWord = false;
  }
  if (initials.empty()) initials = "?";
  r.drawTextCentered(rc.cx(), rc.cy() - Renderer::lineHeight(FontId::Title) * 0.5f, initials, FontId::Title,
                     argb(235, 255, 255, 255));
}

}  // namespace ui
}  // namespace ps5ui
