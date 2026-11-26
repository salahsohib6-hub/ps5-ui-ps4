#include "core/Input.h"

namespace ps5ui {

bool InputTracker::step(Repeat& r, bool down, double dt) {
  if (!down) {
    r.held = false;
    return false;
  }
  if (!r.held) {
    r.held = true;
    r.time = 0;
    r.next = 0.35;  // initial delay
    return true;
  }
  r.time += dt;
  if (r.time >= r.next) {
    r.next += 0.09;  // repeat interval
    return true;
  }
  return false;
}

NavInput InputTracker::update(const PadState& pad, double dt) {
  NavInput n;
  if (!pad.connected) {
    prevButtons_ = 0;
    for (auto& d : dir_) d.held = false;
    return n;
  }
  uint32_t b = pad.buttons;
  uint32_t pressed = b & ~prevButtons_;
  prevButtons_ = b;

  const int dead = 60;  // of 128
  bool up = (b & pad::kUp) || pad.ly < 128 - dead;
  bool down = (b & pad::kDown) || pad.ly > 128 + dead;
  bool left = (b & pad::kLeft) || pad.lx < 128 - dead;
  bool right = (b & pad::kRight) || pad.lx > 128 + dead;
  n.up = step(dir_[0], up, dt);
  n.down = step(dir_[1], down, dt);
  n.left = step(dir_[2], left, dt);
  n.right = step(dir_[3], right, dt);

  n.accept = pressed & pad::kCross;
  n.back = pressed & pad::kCircle;
  n.square = pressed & pad::kSquare;
  n.triangle = pressed & pad::kTriangle;
  n.l1 = pressed & pad::kL1;
  n.r1 = pressed & pad::kR1;
  n.l2 = pressed & pad::kL2;
  n.r2 = pressed & pad::kR2;
  n.options = pressed & pad::kOptions;
  return n;
}

}  // namespace ps5ui
