#include "core/Upscaler.h"

#include <algorithm>
#include <cstring>

namespace ps5ui {

namespace {

class NearestUpscaler final : public IUpscaler {
 public:
  const char* id() const override { return "nearest"; }
  const char* displayName() const override { return "Nearest"; }
  const char* description() const override {
    return "Fastest. Hard pixel edges; lowest CPU cost when rendering below native resolution.";
  }
  void upscale(const ImageView& src, const ImageTarget& dst) override {
    if (src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return;
    xmap_.resize(static_cast<size_t>(dst.w));
    for (int x = 0; x < dst.w; ++x)
      xmap_[static_cast<size_t>(x)] = static_cast<int>((static_cast<int64_t>(x) * 2 + 1) * src.w / (static_cast<int64_t>(dst.w) * 2));
    for (int y = 0; y < dst.h; ++y) {
      int sy = static_cast<int>((static_cast<int64_t>(y) * 2 + 1) * src.h / (static_cast<int64_t>(dst.h) * 2));
      const uint32_t* s = src.px + static_cast<size_t>(sy) * static_cast<size_t>(src.pitch);
      uint32_t* d = dst.px + static_cast<size_t>(y) * static_cast<size_t>(dst.pitch);
      for (int x = 0; x < dst.w; ++x) d[x] = s[xmap_[static_cast<size_t>(x)]];
    }
  }

 private:
  std::vector<int> xmap_;
};

inline uint32_t lerp8(uint32_t a, uint32_t b, uint32_t t /*0..256*/) {
  uint32_t rb = (((a & 0x00FF00FFu) * (256 - t) + (b & 0x00FF00FFu) * t) >> 8) & 0x00FF00FFu;
  uint32_t ag = ((((a >> 8) & 0x00FF00FFu) * (256 - t) + ((b >> 8) & 0x00FF00FFu) * t)) & 0xFF00FF00u;
  return rb | ag;
}

class BilinearUpscaler final : public IUpscaler {
 public:
  const char* id() const override { return "bilinear"; }
  const char* displayName() const override { return "Bilinear"; }
  const char* description() const override {
    return "Smooth interpolation between source pixels. Softer image, moderate CPU cost.";
  }
  void upscale(const ImageView& src, const ImageTarget& dst) override {
    if (src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return;
    const size_t n = static_cast<size_t>(dst.w);
    x0_.resize(n);
    x1_.resize(n);
    fx_.resize(n);
    for (int x = 0; x < dst.w; ++x) {
      // Pixel-centre mapping in 8.8 fixed point.
      int64_t p = ((static_cast<int64_t>(x) * 2 + 1) * src.w * 256) / (static_cast<int64_t>(dst.w) * 2) - 128;
      if (p < 0) p = 0;
      int64_t maxp = static_cast<int64_t>(src.w - 1) * 256;
      if (p > maxp) p = maxp;
      int xi = static_cast<int>(p >> 8);
      x0_[static_cast<size_t>(x)] = xi;
      x1_[static_cast<size_t>(x)] = std::min(xi + 1, src.w - 1);
      fx_[static_cast<size_t>(x)] = static_cast<uint32_t>(p & 255);
    }
    rowA_.resize(n);
    rowB_.resize(n);
    int cachedA = -1, cachedB = -1;
    for (int y = 0; y < dst.h; ++y) {
      int64_t p = ((static_cast<int64_t>(y) * 2 + 1) * src.h * 256) / (static_cast<int64_t>(dst.h) * 2) - 128;
      if (p < 0) p = 0;
      int64_t maxp = static_cast<int64_t>(src.h - 1) * 256;
      if (p > maxp) p = maxp;
      int y0 = static_cast<int>(p >> 8);
      int y1 = std::min(y0 + 1, src.h - 1);
      uint32_t fy = static_cast<uint32_t>(p & 255);
      if (y0 != cachedA) {
        if (y0 == cachedB) {
          rowA_.swap(rowB_);
          std::swap(cachedA, cachedB);
        } else {
          hrow(src, y0, rowA_);
          cachedA = y0;
        }
      }
      if (y1 != cachedB) {
        hrow(src, y1, rowB_);
        cachedB = y1;
      }
      uint32_t* d = dst.px + static_cast<size_t>(y) * static_cast<size_t>(dst.pitch);
      if (y0 == y1 || fy == 0) {
        std::memcpy(d, rowA_.data(), n * sizeof(uint32_t));
      } else {
        for (size_t x = 0; x < n; ++x) d[x] = lerp8(rowA_[x], rowB_[x], fy);
      }
    }
  }

 private:
  void hrow(const ImageView& src, int sy, std::vector<uint32_t>& out) {
    const uint32_t* s = src.px + static_cast<size_t>(sy) * static_cast<size_t>(src.pitch);
    for (size_t x = 0; x < out.size(); ++x) out[x] = lerp8(s[x0_[x]], s[x1_[x]], fx_[x]);
  }
  std::vector<int> x0_, x1_;
  std::vector<uint32_t> fx_;
  std::vector<uint32_t> rowA_, rowB_;
};

}  // namespace

void UpscalerRegistry::add(std::unique_ptr<IUpscaler> u) { items_.push_back(std::move(u)); }

IUpscaler* UpscalerRegistry::find(const std::string& id) const {
  for (const auto& u : items_)
    if (id == u->id()) return u.get();
  return nullptr;
}

IUpscaler* UpscalerRegistry::fallback() const {
  IUpscaler* n = find("nearest");
  if (n) return n;
  return items_.empty() ? nullptr : items_.front().get();
}

std::vector<IUpscaler*> UpscalerRegistry::list() const {
  std::vector<IUpscaler*> out;
  for (const auto& u : items_) out.push_back(u.get());
  return out;
}

void registerBuiltinUpscalers(UpscalerRegistry& r) {
  r.add(std::unique_ptr<IUpscaler>(new NearestUpscaler()));
  r.add(std::unique_ptr<IUpscaler>(new BilinearUpscaler()));
  // Future techniques are registered here, e.g.:
  // r.add(std::unique_ptr<IUpscaler>(new MyEdgeDirectedUpscaler()));
}

void presentScaled(const ImageView& src, const ImageTarget& dst, const UpscalerRegistry& reg,
                   const std::string& upscalerId) {
  if (src.px == nullptr || dst.px == nullptr) return;
  if (src.w == dst.w && src.h == dst.h) {
    for (int y = 0; y < dst.h; ++y)
      std::memcpy(dst.px + static_cast<size_t>(y) * static_cast<size_t>(dst.pitch),
                  src.px + static_cast<size_t>(y) * static_cast<size_t>(src.pitch),
                  static_cast<size_t>(dst.w) * sizeof(uint32_t));
    return;
  }
  IUpscaler* u = reg.find(upscalerId);
  if (!u) u = reg.fallback();
  if (u) u->upscale(src, dst);
}

}  // namespace ps5ui
