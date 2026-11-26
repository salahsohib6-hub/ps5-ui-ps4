#include "core/AdaptiveResolution.h"

#include <algorithm>

namespace ps5ui {

AdaptiveResolution::AdaptiveResolution() { configure(false, 100, 100, 100, 16.6667); }

void AdaptiveResolution::configure(bool enabled, int minPercent, int maxPercent, int startPercent,
                                   double targetFrameMs) {
  enabled_ = enabled;
  budgetMs_ = targetFrameMs;
  static const int kSteps[] = {100, 85, 75, 67, 50};
  ladder_.clear();
  if (!enabled) {
    ladder_.push_back(startPercent);
  } else {
    for (int p : kSteps)
      if (p >= minPercent && p <= maxPercent) ladder_.push_back(p);
    if (ladder_.empty()) ladder_.push_back(std::max(minPercent, std::min(maxPercent, startPercent)));
  }
  index_ = 0;
  for (size_t i = 0; i < ladder_.size(); ++i)
    if (ladder_[i] >= startPercent) index_ = i;  // closest not above start (ladder is descending)
  if (!ladder_.empty() && ladder_[index_] > startPercent && index_ + 1 < ladder_.size()) ++index_;
  samples_ = 0;
  sum_ = 0;
  cooldown_ = 0;
  upWait_ = 120;
  framesSinceStepUp_ = 100000;
}

bool AdaptiveResolution::onFrame(double workMs) {
  if (!enabled_ || ladder_.size() < 2) return false;
  ++framesSinceStepUp_;
  if (cooldown_ > 0) {
    --cooldown_;
    return false;
  }
  sum_ += workMs;
  ++samples_;
  double avg = sum_ / samples_;

  // Over budget: step down after a short window.
  if (samples_ >= 20 && avg > budgetMs_ * 0.85 && index_ + 1 < ladder_.size()) {
    if (framesSinceStepUp_ < 600) upWait_ = std::min(upWait_ * 2, 1800);  // we stepped up too eagerly
    ++index_;
    samples_ = 0;
    sum_ = 0;
    cooldown_ = 45;
    return true;
  }
  // Headroom: step up only if the estimated cost at the next scale fits.
  if (samples_ >= upWait_ && index_ > 0) {
    double est = avg * pixelsAt(ladder_[index_ - 1]) / pixelsAt(ladder_[index_]);
    if (est < budgetMs_ * 0.70) {
      --index_;
      samples_ = 0;
      sum_ = 0;
      cooldown_ = 45;
      framesSinceStepUp_ = 0;
      return true;
    }
    samples_ = 0;  // re-evaluate with fresh data
    sum_ = 0;
  }
  if (samples_ > 2000) {
    samples_ = 0;
    sum_ = 0;
  }
  return false;
}

}  // namespace ps5ui
