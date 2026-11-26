#pragma once
#include "core/Types.h"

namespace ps5ui {

// One frame of navigation events (edge-triggered, with auto-repeat for directions).
struct NavInput {
  bool up = false, down = false, left = false, right = false;
  bool accept = false;    // Cross
  bool back = false;      // Circle
  bool square = false, triangle = false;
  bool l1 = false, r1 = false, l2 = false, r2 = false;
  bool options = false;
  bool any() const {
    return up || down || left || right || accept || back || square || triangle || l1 || r1 || l2 || r2 || options;
  }
};

class InputTracker {
 public:
  NavInput update(const PadState& pad, double dtSeconds);

 private:
  struct Repeat {
    bool held = false;
    double time = 0;
    double next = 0;
  };
  bool step(Repeat& r, bool down, double dt);
  uint32_t prevButtons_ = 0;
  Repeat dir_[4];
};

}  // namespace ps5ui
