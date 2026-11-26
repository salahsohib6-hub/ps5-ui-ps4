#include "gfx/Background.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ps5ui {

namespace {
constexpr float kWideFactor = 1.25f;
}

void BackgroundCache::ensure(int outW, int outH, bool glows) {
  if (outW == outW_ && outH == outH_ && glows == glows_ && wide_.width() > 0) return;
  outW_ = outW;
  outH_ = outH;
  glows_ = glows;
  int wideW = static_cast<int>(std::lround(static_cast<float>(outW) * kWideFactor));
  wide_.allocate(wideW, outH);
  float scale = static_cast<float>(outW) / static_cast<float>(kLogicalWidth);
  Renderer r;
  r.begin(wide_, scale);
  const float lw = static_cast<float>(kLogicalWidth) * kWideFactor;
  const float lh = static_cast<float>(kLogicalHeight);
  r.fillVGradient({0, 0, lw, lh}, rgb(9, 14, 40), rgb(3, 5, 16));
  if (glows) {
    r.drawRadialGlow(520, 980, 980, rgb(28, 84, 214), 0.55f);
    r.drawRadialGlow(1950, 90, 860, rgb(104, 58, 196), 0.45f);
    r.drawRadialGlow(1250, 560, 760, rgb(20, 120, 170), 0.14f);
  }
}

void BackgroundCache::blit(Surface& dst, float pan01) const {
  if (!ready() || dst.width() != outW_ || dst.height() != outH_) return;
  pan01 = std::max(0.0f, std::min(1.0f, pan01));
  int maxOff = wide_.width() - outW_;
  int off = static_cast<int>(std::lround(pan01 * static_cast<float>(maxOff)));
  for (int y = 0; y < outH_; ++y)
    std::memcpy(dst.data() + static_cast<size_t>(y) * static_cast<size_t>(dst.pitch()),
                wide_.data() + static_cast<size_t>(y) * static_cast<size_t>(wide_.pitch()) + off,
                static_cast<size_t>(outW_) * sizeof(uint32_t));
}

}  // namespace ps5ui
