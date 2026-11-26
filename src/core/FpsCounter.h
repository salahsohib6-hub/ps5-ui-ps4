#pragma once
#include <cstdint>
#include <vector>

namespace ps5ui {

struct FpsStats {
  bool valid = false;        // false until enough real samples exist
  double fps = 0;            // presented frames per second over the window
  double avgFrameMs = 0;
  double minFrameMs = 0;
  double maxFrameMs = 0;
  double p99FrameMs = 0;     // 99th percentile frame time ("1% low" companion)
  uint64_t totalFrames = 0;
};

// Measures *presented* frames of this application from real timestamps.
// Feed it the time right after each frame has been flipped to the display.
// It never invents numbers: stats().valid stays false until at least
// kMinSamples intervals have been observed.
class FpsCounter {
 public:
  static constexpr int kMinSamples = 4;
  explicit FpsCounter(uint64_t windowUs = 1000000, int warmupFrames = 3);
  void reset();
  void onFramePresented(uint64_t nowUs);
  FpsStats stats() const;

 private:
  uint64_t windowUs_;
  int warmupFrames_;
  uint64_t total_ = 0;
  std::vector<uint64_t> stamps_;  // timestamps inside the window (oldest first)
};

}  // namespace ps5ui
