#pragma once
#include "gfx/Renderer.h"

namespace ps5ui {

// Pre-rendered, slightly oversized background. Animating it means sliding a
// window across the cached image, which costs a plain memory copy per frame
// instead of re-blending large radial glows.
class BackgroundCache {
 public:
  void ensure(int outW, int outH, bool glows);
  // Copies the background into dst (must be outW x outH). pan01 in [0,1] selects the window.
  void blit(Surface& dst, float pan01) const;
  bool ready() const { return outW_ > 0; }

 private:
  Surface wide_;
  int outW_ = 0, outH_ = 0;
  bool glows_ = false;
};

}  // namespace ps5ui
