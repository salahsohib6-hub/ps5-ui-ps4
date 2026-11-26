#include "platform/host/HostPlatform.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <thread>

namespace ps5ui {

bool HostPlatform::init(int w, int h) {
  w_ = w;
  h_ = h;
  frame_.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0xFF000000u);
  frameStart_ = nowMicros();
  return true;
}

DisplayBuffer HostPlatform::beginFrame() {
  DisplayBuffer b;
  b.pixels = frame_.data();
  b.width = w_;
  b.height = h_;
  b.pitch = w_;
  return b;
}

void HostPlatform::endFrame() {
  ++presented_;
  uint64_t period = 1000000ull / static_cast<uint64_t>(targetFps_);
  if (simulated_) {
    simNow_ += period;
    return;
  }
  uint64_t deadline = frameStart_ + period;
  uint64_t now = nowMicros();
  if (now < deadline) sleepMicros(deadline - now);
  frameStart_ = std::max(deadline, now < deadline ? deadline : now);
}

bool HostPlatform::pollPad(PadState& out) {
  out = pad_;
  return true;
}

uint64_t HostPlatform::nowMicros() {
  if (simulated_) return simNow_;
  using namespace std::chrono;
  return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

void HostPlatform::sleepMicros(uint64_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

int64_t HostPlatform::unixTime() { return static_cast<int64_t>(std::time(nullptr)); }

}  // namespace ps5ui
