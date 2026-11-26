#include "core/Settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "core/FileSystem.h"

namespace ps5ui {

const std::vector<int>& renderScaleChoices() {
  static const std::vector<int> v = {50, 67, 75, 100};
  return v;
}

namespace {

int nearestChoice(int v, const std::vector<int>& choices) {
  int best = choices.front();
  for (int c : choices)
    if (std::abs(c - v) < std::abs(best - v)) best = c;
  return best;
}

bool parseBool(const std::string& v, bool def) {
  if (v == "1" || v == "true" || v == "on") return true;
  if (v == "0" || v == "false" || v == "off") return false;
  return def;
}

}  // namespace

void Settings::sanitize() {
  renderScalePercent = nearestChoice(renderScalePercent, renderScaleChoices());
  targetFps = (targetFps <= 30) ? 30 : 60;
  int o = static_cast<int>(fpsOverlay);
  if (o < 0 || o > 2) fpsOverlay = FpsOverlay::Off;
  if (upscaler.empty() || upscaler.size() > 32) upscaler = "bilinear";
}

bool Settings::loadFromText(const std::string& text) {
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    std::string line = text.substr(pos, eol - pos);
    pos = eol + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "lowGraphics") lowGraphics = parseBool(v, lowGraphics);
    else if (k == "performanceMode") performanceMode = parseBool(v, performanceMode);
    else if (k == "renderScalePercent") renderScalePercent = std::atoi(v.c_str());
    else if (k == "adaptiveResolution") adaptiveResolution = parseBool(v, adaptiveResolution);
    else if (k == "targetFps") targetFps = std::atoi(v.c_str());
    else if (k == "upscaler") upscaler = v;
    else if (k == "fpsOverlay") fpsOverlay = static_cast<FpsOverlay>(std::atoi(v.c_str()));
    else if (k == "backgroundEffects") backgroundEffects = parseBool(v, backgroundEffects);
    else if (k == "uiAnimations") uiAnimations = parseBool(v, uiAnimations);
    else if (k == "highQualityImages") highQualityImages = parseBool(v, highQualityImages);
    else if (k == "showHiddenGames") showHiddenGames = parseBool(v, showHiddenGames);
  }
  sanitize();
  return true;
}

std::string Settings::toText() const {
  auto b = [](bool v) { return v ? "1" : "0"; };
  std::string t = "# PS5 Emulator UI for PS4 - settings (edit while the app is closed)\n";
  t += std::string("lowGraphics=") + b(lowGraphics) + "\n";
  t += std::string("performanceMode=") + b(performanceMode) + "\n";
  t += "renderScalePercent=" + std::to_string(renderScalePercent) + "\n";
  t += std::string("adaptiveResolution=") + b(adaptiveResolution) + "\n";
  t += "targetFps=" + std::to_string(targetFps) + "\n";
  t += "upscaler=" + upscaler + "\n";
  t += "fpsOverlay=" + std::to_string(static_cast<int>(fpsOverlay)) + "\n";
  t += std::string("backgroundEffects=") + b(backgroundEffects) + "\n";
  t += std::string("uiAnimations=") + b(uiAnimations) + "\n";
  t += std::string("highQualityImages=") + b(highQualityImages) + "\n";
  t += std::string("showHiddenGames=") + b(showHiddenGames) + "\n";
  return t;
}

bool Settings::load(const std::string& path) {
  std::string text;
  if (!fs::readTextFile(path, text)) return false;
  return loadFromText(text);
}

bool Settings::save(const std::string& path) const { return fs::writeTextFile(path, toText()); }

EffectiveConfig resolve(const Settings& in) {
  Settings s = in;
  s.sanitize();
  EffectiveConfig c;
  c.renderScalePercent = s.renderScalePercent;
  c.targetFps = s.targetFps;
  c.upscaler = s.upscaler;
  c.adaptive = s.adaptiveResolution;
  c.animatedBackground = s.backgroundEffects;
  c.staticGlowBackground = true;
  c.glowEffects = true;
  c.uiAnimations = s.uiAnimations;
  c.bilinearImages = s.highQualityImages;
  c.minScalePercent = 50;
  c.maxScalePercent = 100;

  if (s.performanceMode) {
    // Responsiveness first: 60 Hz target, adaptive resolution, no animated background.
    c.targetFps = 60;
    c.adaptive = true;
    c.animatedBackground = false;
    c.bilinearImages = false;
  }
  if (s.lowGraphics) {
    // Cheapest possible rendering: capped scale, flat background, no glow.
    c.renderScalePercent = std::min(c.renderScalePercent, 67);
    c.maxScalePercent = 67;
    c.animatedBackground = false;
    c.staticGlowBackground = false;
    c.glowEffects = false;
    c.bilinearImages = false;
    if (c.renderScalePercent < 100) c.upscaler = "nearest";
  }
  if (c.adaptive) {
    // Adaptive resolution starts from (and never exceeds) the chosen scale.
    c.maxScalePercent = std::min(c.maxScalePercent, std::max(c.renderScalePercent, 50));
    c.minScalePercent = 50;
  } else {
    c.minScalePercent = c.maxScalePercent = c.renderScalePercent;
  }
  return c;
}

}  // namespace ps5ui
