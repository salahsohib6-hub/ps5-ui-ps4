#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ps5ui {

struct ImageView {
  const uint32_t* px = nullptr;
  int w = 0, h = 0, pitch = 0;  // pitch in pixels
};

struct ImageTarget {
  uint32_t* px = nullptr;
  int w = 0, h = 0, pitch = 0;  // pitch in pixels
};

// Extension point for image upscaling.
//
// To add a technique later (a sharpening upscaler, an edge-directed scaler,
// a GPU-backed one, ...): derive from IUpscaler, implement upscale(), and
// register an instance in registerBuiltinUpscalers() (Upscaler.cpp). It shows
// up in the settings UI automatically. Implementations must only write the
// destination (it may be write-combined video memory, so reading from it is
// slow) and must handle any src/dst size.
class IUpscaler {
 public:
  virtual ~IUpscaler() = default;
  virtual const char* id() const = 0;           // stable key stored in settings
  virtual const char* displayName() const = 0;  // shown in the UI
  virtual const char* description() const = 0;
  virtual void upscale(const ImageView& src, const ImageTarget& dst) = 0;
};

class UpscalerRegistry {
 public:
  void add(std::unique_ptr<IUpscaler> u);
  IUpscaler* find(const std::string& id) const;
  IUpscaler* fallback() const;                 // "nearest"
  std::vector<IUpscaler*> list() const;
  size_t count() const { return items_.size(); }

 private:
  std::vector<std::unique_ptr<IUpscaler>> items_;
};

void registerBuiltinUpscalers(UpscalerRegistry& r);

// Copies when sizes match, otherwise scales with the chosen upscaler.
void presentScaled(const ImageView& src, const ImageTarget& dst, const UpscalerRegistry& reg,
                   const std::string& upscalerId);

}  // namespace ps5ui
