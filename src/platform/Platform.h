#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "core/Types.h"

namespace ps5ui {

struct DisplayBuffer {
  uint32_t* pixels = nullptr;  // write-only (may be write-combined video memory)
  int width = 0, height = 0;
  int pitch = 0;               // pixels per row
};

// Everything the app needs from the machine it runs on. The PS4 implementation
// talks to the console (video out, pad, kernel clock); the host implementation
// is a headless stand-in used for development and automated tests.
class IPlatform {
 public:
  virtual ~IPlatform() = default;

  virtual const char* name() const = 0;
  virtual bool init(int outputWidth, int outputHeight) = 0;
  virtual void shutdown() = 0;

  // Returns the buffer the next frame must be written into.
  virtual DisplayBuffer beginFrame() = 0;
  // Submits that buffer and blocks until it is actually on screen (vsync).
  virtual void endFrame() = 0;

  virtual bool pollPad(PadState& out) = 0;

  virtual uint64_t nowMicros() = 0;  // monotonic
  virtual void sleepMicros(uint64_t us) = 0;

  // True only if frame timestamps taken after endFrame() really reflect presented
  // frames. The UI shows "FPS: --" whenever this is false.
  virtual bool hasReliableFrameTimer() const = 0;
  virtual void setTargetFps(int fps) = 0;  // 30 or 60 (display flip rate)

  virtual std::string dataDirectory() const = 0;
  virtual int64_t unixTime() = 0;

  // Launching other titles is privileged and firmware dependent, so it is opt-in.
  virtual bool canLaunchTitles() const = 0;
  virtual bool launchTitle(const std::string& titleId, std::string& error) = 0;
};

// Defined by exactly one of the platform back-ends (ps4 or host).
std::unique_ptr<IPlatform> createPlatform();

}  // namespace ps5ui
