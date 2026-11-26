# Architecture

```
src/
  core/      portable logic (no console calls)
    Settings        user choices + resolve() -> EffectiveConfig (Low Graphics / Performance Mode rules)
    FpsCounter      real presented-frame statistics (never invents numbers)
    AdaptiveResolution  render-scale controller driven by measured CPU work time
    Upscaler        IUpscaler interface + registry + Nearest/Bilinear
    Sfo, Library    param.sfo reader, installed-title scanner, favourites/hidden/launch data
    Input           DualShock 4 -> navigation events (auto-repeat)
  gfx/       software renderer, PNG decoder, font atlas (generated), cached background
  ui/        UiApp (frame loop, chrome, modals), Screens (Home/Library/Performance/Settings), Widgets
  platform/
    Platform.h      IPlatform: video buffer, flip+vsync, pad, clock, launch
    ps4/            console back-end (self-declared SDK prototypes in Ps4Sdk.h)
    host/           headless desktop harness used by tests and screenshots
```

## Frame pipeline
1. Poll pad -> `NavInput`.
2. Update state, render the UI into a **CPU-side surface** at the current render scale
   (100/85/75/67/50 % of 1920x1080). Rendering never reads video memory.
3. `presentScaled()` copies (scale 100 %) or upscales that surface into the display buffer.
4. `IPlatform::endFrame()` submits the flip and blocks until vsync. The timestamp taken
   right after that call feeds `FpsCounter`; the CPU time before it feeds `AdaptiveResolution`.

## FPS honesty
FPS = number of frames *this app* actually presented per second, from `sceKernelGetProcessTime()`
timestamps taken after each flip completed. `IPlatform::hasReliableFrameTimer()` is false when the
clock self-test fails or flips time out; the UI then shows "N/A". GPU/CPU load and the frame rate of
other titles are not available to homebrew and are not displayed.

## Adding an image upscaler (e.g. a sharpening or edge-directed scaler)
1. In `src/core/Upscaler.cpp` add a class deriving from `IUpscaler` (`id()`, `displayName()`,
   `description()`, `upscale(src, dst)`). Only write to `dst`; it may be write-combined memory.
2. Register it in `registerBuiltinUpscalers()`.
3. It appears in *Performance > Image Upscaler* automatically and is stored by `id()` in `settings.cfg`.
4. Add a test next to the existing ones in `tests/test_main.cpp` (`testUpscalers`).

Techniques that need a GPU (compute shaders) would be a second interface next to `IUpscaler`
fed by a GPU-capable `IPlatform`; that does not exist yet and is intentionally not faked.

## Low Graphics vs Performance Mode (see `resolve()` in `Settings.cpp`)
| | Low Graphics | Performance Mode |
|---|---|---|
| Render scale | capped at 67 % | user value, adaptive 100 -> 50 % |
| Upscaler | nearest (if scaling) | user choice |
| Background | flat gradient | static glow (no drift) |
| Glow / focus effects | off | on |
| Target FPS | user value | forced 60 |
| Cover art sampling | nearest | nearest |
They can be combined.
