#pragma once
#include <cstddef>
#include <vector>

namespace ps5ui {

// Chooses the UI render scale from the *measured* CPU work time per frame
// (render + upscale + copy), not from the vsync-quantised frame interval.
// Steps down quickly when over budget, steps up slowly and only when the
// estimated cost at the next scale still fits (with a back-off to avoid
// oscillation).
class AdaptiveResolution {
 public:
  AdaptiveResolution();
  void configure(bool enabled, int minPercent, int maxPercent, int startPercent, double targetFrameMs);
  int percent() const { return ladder_.empty() ? 100 : ladder_[index_]; }
  bool enabled() const { return enabled_; }
  // Report CPU work time for the frame just rendered. Returns true if the scale changed.
  bool onFrame(double workMs);

 private:
  static double pixelsAt(int percent) { return (percent / 100.0) * (percent / 100.0); }
  bool enabled_ = false;
  double budgetMs_ = 16.6667;
  std::vector<int> ladder_;  // descending percentages
  std::size_t index_ = 0;
  int samples_ = 0;
  double sum_ = 0;
  int cooldown_ = 0;
  int upWait_ = 120;       // frames of headroom required before stepping up
  int framesSinceStepUp_ = 100000;
};

}  // namespace ps5ui
