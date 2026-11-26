#pragma once
#include <string>
#include <vector>

namespace ps5ui {

enum class FpsOverlay { Off = 0, Simple = 1, Detailed = 2 };

// What the user chose. Low Graphics / Performance Mode are *modes* that
// override some of these at runtime (see resolve()); the raw choices are kept
// so turning a mode off restores the previous configuration.
struct Settings {
  bool lowGraphics = false;
  bool performanceMode = false;
  int renderScalePercent = 100;      // 50, 67, 75 or 100
  bool adaptiveResolution = false;   // dynamic render scale from measured frame work time
  int targetFps = 60;                // 30 or 60 (display flip rate)
  std::string upscaler = "bilinear"; // id from UpscalerRegistry
  FpsOverlay fpsOverlay = FpsOverlay::Off;
  bool backgroundEffects = true;     // animated background glows
  bool uiAnimations = true;
  bool highQualityImages = false;    // bilinear cover art sampling (more CPU than nearest)
  bool showHiddenGames = false;

  void sanitize();
  bool loadFromText(const std::string& text);
  std::string toText() const;
  bool load(const std::string& path);
  bool save(const std::string& path) const;
};

// The final, mode-resolved configuration the renderer actually uses.
struct EffectiveConfig {
  int renderScalePercent = 100;
  int minScalePercent = 100;   // lower bound for adaptive resolution
  int maxScalePercent = 100;   // upper bound for adaptive resolution
  bool adaptive = false;
  int targetFps = 60;
  std::string upscaler = "bilinear";
  bool animatedBackground = true;
  bool staticGlowBackground = true;
  bool glowEffects = true;
  bool uiAnimations = true;
  bool bilinearImages = true;
};

EffectiveConfig resolve(const Settings& s);

// Allowed discrete values (used by the UI and by sanitize()).
const std::vector<int>& renderScaleChoices();

}  // namespace ps5ui
