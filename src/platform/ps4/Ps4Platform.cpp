// PS4 back-end (OpenOrbis). Built only for the console target.
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/Log.h"
#include "platform/Platform.h"
#include "platform/ps4/Ps4Sdk.h"

namespace ps5ui {

namespace {

class Ps4Platform final : public IPlatform {
 public:
  const char* name() const override { return "PS4 (OpenOrbis)"; }

  bool init(int w, int h) override {
    w_ = w;
    h_ = h;

    // Clock: verify it really advances before we ever show an FPS number.
    uint64_t t0 = sceKernelGetProcessTime();
    sceKernelUsleep(3000);
    uint64_t t1 = sceKernelGetProcessTime();
    clockOk_ = (t1 > t0) && (t1 - t0 >= 1500) && (t1 - t0 < 200000);
    log::write("clock check: dt=%llu us ok=%d", static_cast<unsigned long long>(t1 - t0), clockOk_ ? 1 : 0);

    // Video memory: two linear BGRA buffers.
    const size_t pitchBytes = static_cast<size_t>(w) * 4;
    const size_t one = (pitchBytes * static_cast<size_t>(h) + ps4const::kDirectMemAlign - 1) &
                       ~(ps4const::kDirectMemAlign - 1);
    const size_t total = one * 2;
    int64_t phys = 0;
    int rc = sceKernelAllocateDirectMemory(0, static_cast<int64_t>(sceKernelGetDirectMemorySize()), total,
                                           ps4const::kDirectMemAlign, ps4const::kMemTypeWcGarlic, &phys);
    if (rc != 0) {
      log::write("AllocateDirectMemory failed: 0x%x", rc);
      return false;
    }
    void* base = nullptr;
    rc = sceKernelMapDirectMemory(&base, total, ps4const::kProtCpuGpuReadWrite, 0, phys, ps4const::kDirectMemAlign);
    if (rc != 0 || !base) {
      log::write("MapDirectMemory failed: 0x%x", rc);
      return false;
    }
    buf_[0] = static_cast<uint32_t*>(base);
    buf_[1] = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(base) + one);
    for (int i = 0; i < 2; ++i) memset(buf_[i], 0, one);

    video_ = sceVideoOutOpen(ps4const::kUserIdSystem, ps4const::kBusTypeMain, 0, nullptr);
    if (video_ < 0) {
      log::write("sceVideoOutOpen failed: 0x%x", video_);
      return false;
    }
    Ps4VideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, ps4const::kPixelFormatB8G8R8A8Srgb, ps4const::kTilingLinear,
                                  ps4const::kAspect16x9, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                                  static_cast<uint32_t>(w));
    void* addrs[2] = {buf_[0], buf_[1]};
    rc = sceVideoOutRegisterBuffers(video_, 0, addrs, 2, &attr);
    if (rc != 0) {
      log::write("RegisterBuffers failed: 0x%x", rc);
      return false;
    }
    sceVideoOutSetFlipRate(video_, 0);
    sceVideoOutSubmitFlip(video_, 0, ps4const::kFlipModeVsync, 0);
    displayed_ = 0;

    // Pad (DualShock 4).
    sceUserServiceInitialize(nullptr);
    int user = ps4const::kUserIdSystem;
    if (sceUserServiceGetInitialUser(&user) != 0) user = ps4const::kUserIdSystem;
    scePadInit();
    pad_ = scePadOpen(user, 0, 0, nullptr);
    log::write("pad handle: %d", pad_);
    return true;
  }

  void shutdown() override {
    if (video_ >= 0) sceVideoOutClose(video_);
    video_ = -1;
  }

  DisplayBuffer beginFrame() override {
    back_ = displayed_ ^ 1;
    DisplayBuffer d;
    d.pixels = buf_[back_];
    d.width = w_;
    d.height = h_;
    d.pitch = w_;
    return d;
  }

  void endFrame() override {
    sceVideoOutSubmitFlip(video_, back_, ps4const::kFlipModeVsync, static_cast<int64_t>(++flipArg_));
    // Wait until the flip has really been applied (vsync). Time out so we never hang.
    uint64_t start = sceKernelGetProcessTime();
    bool done = false;
    while (sceKernelGetProcessTime() - start < 250000) {
      if (sceVideoOutIsFlipPending(video_) <= 0) {
        done = true;
        break;
      }
      sceKernelUsleep(200);
    }
    flipTimeouts_ = done ? 0 : flipTimeouts_ + 1;
    displayed_ = back_;
  }

  bool pollPad(PadState& out) override {
    out = PadState();
    if (pad_ < 0) return false;
    alignas(8) uint8_t data[256];
    memset(data, 0, sizeof(data));
    int rc = scePadReadState(pad_, data);
    if (rc < 0) return false;
    // Only the first fields of the console pad structure are read (stable layout):
    // u32 buttons, then left stick X/Y, right stick X/Y, L2/R2 analog values.
    uint32_t buttons = static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
                       (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
    out.connected = true;
    out.buttons = buttons;
    out.lx = data[4];
    out.ly = data[5];
    out.rx = data[6];
    out.ry = data[7];
    out.l2 = data[8];
    out.r2 = data[9];
    return true;
  }

  uint64_t nowMicros() override { return sceKernelGetProcessTime(); }
  void sleepMicros(uint64_t us) override { sceKernelUsleep(static_cast<unsigned int>(us)); }
  bool hasReliableFrameTimer() const override { return clockOk_ && flipTimeouts_ < 3 && video_ >= 0; }
  void setTargetFps(int fps) override {
    if (video_ >= 0) sceVideoOutSetFlipRate(video_, fps <= 30 ? 1 : 0);
  }
  std::string dataDirectory() const override { return "/data/ps5ui"; }
  int64_t unixTime() override {
#if defined(PS5UI_ENABLE_LAUNCH)
    return static_cast<int64_t>(time(nullptr));
#else
    return 0;
#endif
  }

  bool canLaunchTitles() const override {
#if defined(PS5UI_ENABLE_LAUNCH)
    return true;
#else
    return false;
#endif
  }
  bool launchTitle(const std::string& titleId, std::string& error) override {
#if defined(PS5UI_ENABLE_LAUNCH)
    int rc = sceSystemServiceLaunchApp(titleId.c_str(), nullptr, nullptr);
    if (rc != 0) {
      char b[48];
      snprintf(b, sizeof(b), "error 0x%08x", static_cast<unsigned>(rc));
      error = b;
      return false;
    }
    return true;
#else
    (void)titleId;
    error = "launch support not compiled in";
    return false;
#endif
  }

 private:
  int w_ = 0, h_ = 0;
  uint32_t* buf_[2] = {nullptr, nullptr};
  int video_ = -1;
  int pad_ = -1;
  int displayed_ = 0, back_ = 1;
  uint64_t flipArg_ = 0;
  bool clockOk_ = false;
  int flipTimeouts_ = 0;
};

}  // namespace

std::unique_ptr<IPlatform> createPlatform() { return std::unique_ptr<IPlatform>(new Ps4Platform()); }

}  // namespace ps5ui
