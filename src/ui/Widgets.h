#pragma once
#include <string>

#include "core/Library.h"
#include "gfx/Renderer.h"

namespace ps5ui {
namespace ui {

namespace theme {
constexpr uint32_t kText = rgb(240, 244, 255);
constexpr uint32_t kTextDim = rgb(148, 162, 200);
constexpr uint32_t kTextFaint = rgb(96, 110, 150);
constexpr uint32_t kAccent = rgb(86, 156, 255);
constexpr uint32_t kAccent2 = rgb(150, 112, 255);
constexpr uint32_t kGood = rgb(92, 214, 150);
constexpr uint32_t kWarn = rgb(255, 196, 80);
constexpr uint32_t kBad = rgb(255, 110, 110);
constexpr uint32_t kPanel = argb(150, 16, 24, 58);
constexpr uint32_t kPanelStroke = argb(70, 150, 180, 255);
constexpr uint32_t kRowFocus = argb(70, 90, 150, 255);
}  // namespace theme

enum class PadGlyph { Cross, Circle, Square, Triangle };

uint32_t hsv(float h, float s, float v);  // h in [0,360)
uint32_t hashString(const std::string& s);

void drawPadGlyph(Renderer& r, PadGlyph g, float cx, float cy, float radius);
// Draws "<glyph> label" at x,y (y = text top). Returns total width.
float drawGlyphHint(Renderer& r, float x, float y, PadGlyph g, const std::string& label);
// Draws "[KEY] label" for buttons without a glyph (L1/R1/OPTIONS...). Returns total width.
float drawKeyHint(Renderer& r, float x, float y, const std::string& key, const std::string& label);

void drawStar(Renderer& r, float cx, float cy, float radius, uint32_t color);
void drawTabIcon(Renderer& r, int tab, float cx, float cy, float size, uint32_t color);
void drawPanel(Renderer& r, const Rect& rc, float radius = 24.0f);
void drawSwitch(Renderer& r, const Rect& rc, bool on, uint32_t accent = theme::kAccent);
// Rounded cover art: decoded icon if available, otherwise generated artwork.
void drawCover(Renderer& r, const GameEntry& g, const Image* icon, const Rect& rc, float radius, bool bilinear);

}  // namespace ui
}  // namespace ps5ui
