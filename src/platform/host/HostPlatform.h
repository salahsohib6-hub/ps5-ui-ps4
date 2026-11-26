#pragma once
#include <vector>

#include "platform/Platform.h"

namespace ps5ui {

// Headless desktop stand-in used for development and automated tests.
// It is NOT shipped in the PS4 build.
class HostPlatform : public IPlatform {
 public:
  explicit HostPlatform(std::string dataDir) : dataDir_(std::move(dataDir)) {}

  const char* name() const override { return "host (headless dev harness)"; }
  bool init(int w, int h) override;
  void shutdown() override {}
  DisplayBuffer beginFrame() override;
  void endFrame() override;
  bool pollPad(PadState& out) override;
  uint64_t nowMicros() override;
  void sleepMicros(uint64_t us) override;
  bool hasReliableFrameTimer() const override { return true; }
  void setTargetFps(int fps) override { targetFps_ = fps; }
  std::string dataDirectory() const override { return dataDir_; }
  int64_t unixTime() override;
  bool canLaunchTitles() const override { return false; }
  bool launchTitle(const std::string&, std::string& error) override {
    error = "not supported on host";
    return false;
  }

  // --- test controls
  void setPad(const PadState& p) { pad_ = p; }
  // Tests may advance a simulated clock instead of sleeping (never used for the screenshot tool).
  void useSimulatedClock(bool on) { simulated_ = on; }
  const std::vector<uint32_t>& lastFrame() const { return frame_; }
  int width() const { return w_; }
  int height() const { return h_; }
  uint64_t framesPresented() const { return presented_; }

 private:
  std::string dataDir_;
  int w_ = 0, h_ = 0;
  std::vector<uint32_t> frame_;
  PadState pad_;
  int targetFps_ = 60;
  bool simulated_ = false;
  uint64_t simNow_ = 1000000;
  uint64_t frameStart_ = 0;
  uint64_t presented_ = 0;
};

}  // namespace ps5ui
