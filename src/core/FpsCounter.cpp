#include "core/FpsCounter.h"

#include <algorithm>

namespace ps5ui {

FpsCounter::FpsCounter(uint64_t windowUs, int warmupFrames)
    : windowUs_(windowUs), warmupFrames_(warmupFrames) {
  stamps_.reserve(512);
}

void FpsCounter::reset() {
  total_ = 0;
  stamps_.clear();
}

void FpsCounter::onFramePresented(uint64_t nowUs) {
  ++total_;
  if (total_ <= static_cast<uint64_t>(warmupFrames_)) return;  // skip start-up hitches
  if (!stamps_.empty() && nowUs < stamps_.back()) {            // clock went backwards: restart
    stamps_.clear();
  }
  stamps_.push_back(nowUs);
  size_t drop = 0;
  while (drop + 1 < stamps_.size() && nowUs - stamps_[drop] > windowUs_) ++drop;
  if (drop) stamps_.erase(stamps_.begin(), stamps_.begin() + static_cast<long>(drop));
  if (stamps_.size() > 480) stamps_.erase(stamps_.begin(), stamps_.begin() + 120);
}

FpsStats FpsCounter::stats() const {
  FpsStats s;
  s.totalFrames = total_;
  if (stamps_.size() < static_cast<size_t>(kMinSamples) + 1) return s;
  std::vector<double> dt;
  dt.reserve(stamps_.size() - 1);
  for (size_t i = 1; i < stamps_.size(); ++i)
    dt.push_back(static_cast<double>(stamps_[i] - stamps_[i - 1]) / 1000.0);
  double span = static_cast<double>(stamps_.back() - stamps_.front()) / 1e6;
  if (span <= 0) return s;
  double sum = 0;
  for (double d : dt) sum += d;
  s.fps = static_cast<double>(dt.size()) / span;
  s.avgFrameMs = sum / static_cast<double>(dt.size());
  s.minFrameMs = *std::min_element(dt.begin(), dt.end());
  s.maxFrameMs = *std::max_element(dt.begin(), dt.end());
  std::sort(dt.begin(), dt.end());
  size_t idx = static_cast<size_t>(static_cast<double>(dt.size() - 1) * 0.99);
  s.p99FrameMs = dt[idx];
  s.valid = true;
  return s;
}

}  // namespace ps5ui
