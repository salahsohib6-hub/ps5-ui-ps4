#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/FileSystem.h"
#include "ui/UiApp.h"
#include "ui/Widgets.h"

namespace ps5ui {

using namespace ui;

namespace {
constexpr int kLibCols = 6;
std::string fwString(uint32_t v) {
  if (v == 0) return "Any";
  char b[16];
  std::snprintf(b, sizeof(b), "%X.%02X", (v >> 24) & 0xFF, (v >> 16) & 0xFF);
  return b;
}
}  // namespace

void UiApp::refreshViews() {
  viewsDirty_ = false;
  homeView_ = library_.view(LibrarySort::RecentlyLaunched, LibraryFilter::All, settings_.showHiddenGames);
  libView_ = library_.view(librarySort_, libraryFilter_, settings_.showHiddenGames);
  homeIndex_ = std::max(0, std::min(homeIndex_, static_cast<int>(homeView_.size()) - 1));
  libIndex_ = std::max(0, std::min(libIndex_, static_cast<int>(libView_.size()) - 1));
}

// ----------------------------------------------------------------------- Home

void UiApp::updateHome(const NavInput& in, double dt) {
  int n = static_cast<int>(homeView_.size());
  if (in.up) focusTabs_ = true;
  if (n == 0) {
    if (in.accept) {
      library_.scan();
      viewsDirty_ = true;
      showToast("Library rescanned: " + std::to_string(library_.all().size()) + " title(s)");
    }
    return;
  }
  if (in.left) homeIndex_ = std::max(0, homeIndex_ - 1);
  if (in.right) homeIndex_ = std::min(n - 1, homeIndex_ + 1);
  if (in.accept) openDetails(homeView_[static_cast<size_t>(homeIndex_)]);
  if (in.square) {
    library_.toggleFavorite(library_.all()[static_cast<size_t>(homeView_[static_cast<size_t>(homeIndex_)])].titleId);
    saveAll();
    viewsDirty_ = true;
  }
  homeScroll_ = smooth(homeScroll_, static_cast<float>(homeIndex_), 12.0f, dt);
}

void UiApp::renderHome() {
  int n = static_cast<int>(homeView_.size());
  r_.drawText(150, 160, "Your Games", FontId::Heading, theme::kText);
  if (n == 0) {
    Rect p{360, 330, 1200, 420};
    drawPanel(r_, p, 28);
    r_.drawTextCentered(960, p.y + 50, "No PS4 titles found", FontId::Heading, theme::kText);
    r_.drawTextCentered(960, p.y + 130, "Looked in:", FontId::Body, theme::kTextDim);
    float y = p.y + 176;
    for (const auto& root : library_.scanRoots()) {
      r_.drawTextCentered(960, y, Renderer::fitText(root, FontId::Body, 1000), FontId::Body, theme::kText);
      y += 40;
    }
    r_.drawTextCentered(960, p.bottom() - 120, "Edit library_paths.txt in the app data folder to add folders.", FontId::Small, theme::kTextDim);
    r_.drawTextCentered(960, p.bottom() - 78, "Press X to rescan.", FontId::Small, theme::kTextDim);
    return;
  }
  r_.drawTextRight(1770, 168, std::to_string(n) + (n == 1 ? " title" : " titles"), FontId::Small, theme::kTextDim);

  const float baseX = 150, normal = 300, focusSz = 400, gap = 36, yc = 480;
  const float pitch = normal + gap;
  float scroll = homeScroll_;
  int focusIdx = homeIndex_;
  auto rectFor = [&](int i, float& f) {
  f = std::max(0.0f, std::min<float>(1.0f, 1.0f - std::fabs(static_cast<float>(i) - scroll)));
    float size = normal + (focusSz - normal) * f;
    float g = std::max(0.0f, std::min(1.0f, static_cast<float>(i) - scroll));
    float x = baseX + (static_cast<float>(i) - scroll) * pitch + (focusSz - normal) * g;
    return Rect{x, yc - size / 2, size, size};
  };
  r_.setClip({0, 200, 1920, 640});
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < n; ++i) {
      if ((i == focusIdx) != (pass == 1)) continue;
      float f;
      Rect rc = rectFor(i, f);
      if (rc.right() < 0 || rc.x > 1920) continue;
      const GameEntry& g = library_.all()[static_cast<size_t>(homeView_[static_cast<size_t>(i)])];
      drawCover(r_, g, iconFor(g), rc, 24, config_.bilinearImages);
      if (f < 1.0f) r_.fillRoundRect(rc, 24, argb(static_cast<uint32_t>(120 * (1 - f)), 0, 0, 0));
      if (g.favorite) drawStar(r_, rc.right() - 34, rc.y + 34, 15, rgb(255, 210, 90));
      if (i == focusIdx) {
        if (config_.glowEffects) r_.glowRing(rc, 24, argb(170, 90, 160, 255), 5, 7);
        float pulse = config_.uiAnimations ? 0.5f + 0.5f * std::sin(static_cast<float>(animTime_) * 3.0f) : 1.0f;
        r_.strokeRoundRect(rc, 24, 4.0f, argb(static_cast<uint32_t>(190 + 65 * pulse), 240, 246, 255));
      }
    }
  }
  r_.clearClip();

  const GameEntry& g = library_.all()[static_cast<size_t>(homeView_[static_cast<size_t>(homeIndex_)])];
  r_.drawText(150, 740, Renderer::fitText(g.title, FontId::Title, 1500), FontId::Title, theme::kText);
  std::string meta = g.titleId + "   |   v" + (g.version.empty() ? "?" : g.version) + "   |   Min FW " + fwString(g.minFirmware);
  if (g.launchCount) meta += "   |   Launched " + std::to_string(g.launchCount) + "x";
  r_.drawText(150, 818, meta, FontId::Body, theme::kTextDim);
  r_.drawText(150, 864, Renderer::fitText(g.path, FontId::Small, 1500), FontId::Small, theme::kTextFaint);
}

// -------------------------------------------------------------------- Library

void UiApp::updateLibrary(const NavInput& in, double dt) {
  int n = static_cast<int>(libView_.size());
  if (in.triangle) {
    librarySort_ = static_cast<LibrarySort>((static_cast<int>(librarySort_) + 1) % 5);
    viewsDirty_ = true;
    showToast(std::string("Sort: ") + sortName(librarySort_));
  }
  if (in.l2 || in.r2) {
    libraryFilter_ = libraryFilter_ == LibraryFilter::All ? LibraryFilter::Favorites : LibraryFilter::All;
    libIndex_ = 0;
    viewsDirty_ = true;
    showToast(std::string("Filter: ") + filterName(libraryFilter_));
  }
  if (n == 0) {
    if (in.up) focusTabs_ = true;
    return;
  }
  if (in.left) libIndex_ = std::max(0, libIndex_ - 1);
  if (in.right) libIndex_ = std::min(n - 1, libIndex_ + 1);
  if (in.down) libIndex_ = std::min(n - 1, libIndex_ + kLibCols);
  if (in.up) {
    if (libIndex_ < kLibCols) focusTabs_ = true;
    else libIndex_ -= kLibCols;
  }
  const GameEntry& g = library_.all()[static_cast<size_t>(libView_[static_cast<size_t>(libIndex_)])];
  if (in.accept) openDetails(libView_[static_cast<size_t>(libIndex_)]);
  if (in.square) {
    library_.toggleFavorite(g.titleId);
    saveAll();
    viewsDirty_ = true;
  }
  float target = static_cast<float>(std::max(0, libIndex_ / kLibCols - 1));
  libScrollRow_ = smooth(libScrollRow_, target, 12.0f, dt);
}

void UiApp::renderLibrary() {
  int n = static_cast<int>(libView_.size());
  r_.drawText(150, 160, "Library", FontId::Heading, theme::kText);
  std::string info = std::string("Sort: ") + sortName(librarySort_) + "    Filter: " + filterName(libraryFilter_) + "    " +
                     std::to_string(n) + (n == 1 ? " title" : " titles");
  r_.drawTextRight(1770, 168, info, FontId::Small, theme::kTextDim);
  if (n == 0) {
    r_.drawTextCentered(960, 480, libraryFilter_ == LibraryFilter::Favorites ? "No favorites yet" : "No titles to show",
                        FontId::Heading, theme::kTextDim);
    return;
  }
  const float tile = 220, gapX = 40, rowPitch = 300, x0 = (1920 - (kLibCols * tile + (kLibCols - 1) * gapX)) / 2, y0 = 240;
  r_.setClip({0, 215, 1920, 750});
  for (int i = 0; i < n; ++i) {
    int row = i / kLibCols, col = i % kLibCols;
    float y = y0 + (static_cast<float>(row) - libScrollRow_) * rowPitch;
    if (y > 980 || y + rowPitch < 215) continue;
    Rect rc{x0 + static_cast<float>(col) * (tile + gapX), y, tile, tile};
    const GameEntry& g = library_.all()[static_cast<size_t>(libView_[static_cast<size_t>(i)])];
    drawCover(r_, g, iconFor(g), rc, 20, config_.bilinearImages);
    if (g.favorite) drawStar(r_, rc.right() - 28, rc.y + 28, 12, rgb(255, 210, 90));
    if (g.hidden) r_.drawText(rc.x + 12, rc.y + 10, "HIDDEN", FontId::Small, theme::kWarn);
    bool sel = (i == libIndex_);
    if (sel) {
      if (config_.glowEffects) r_.glowRing(rc, 20, argb(160, 90, 160, 255), 5, 5);
      r_.strokeRoundRect(rc, 20, 4.0f, theme::kText);
    }
    r_.drawText(rc.x + 4, rc.bottom() + 12, Renderer::fitText(g.title, FontId::Small, tile - 8), FontId::Small,
                sel ? theme::kText : theme::kTextDim);
    r_.drawText(rc.x + 4, rc.bottom() + 40, g.titleId, FontId::Small, theme::kTextFaint);
  }
  r_.clearClip();
}

// ---------------------------------------------------------------- list helper

void UiApp::renderListRows(float x, float y, float w, float rowH, int count, int focus,
                           std::string (UiApp::*label)(int) const, std::string (UiApp::*value)(int) const) {
  for (int i = 0; i < count; ++i) {
    Rect row{x, y + static_cast<float>(i) * rowH, w, rowH - 6};
    bool sel = (i == focus) && !focusTabs_;
    if (sel) {
      r_.fillRoundRect(row, 16, theme::kRowFocus);
      r_.strokeRoundRect(row, 16, 2.5f, theme::kText);
    }
    r_.drawText(row.x + 24, row.y + (row.h - 30) / 2, (this->*label)(i), FontId::Body, sel ? theme::kText : theme::kTextDim);
    std::string v = (this->*value)(i);
    if (!v.empty()) {
      uint32_t c = v.rfind("On", 0) == 0 ? theme::kGood : (v.rfind("Off", 0) == 0 ? theme::kTextFaint : theme::kAccent);
      std::string shown = sel ? "<  " + v + "  >" : v;
      r_.drawTextRight(row.right() - 24, row.y + (row.h - 30) / 2, shown, FontId::Body, c);
    }
  }
}

// ---------------------------------------------------------------- Performance

int UiApp::perfRowCount() const { return 11; }

std::string UiApp::perfRowLabel(int row) const {
  static const char* kL[11] = {"Low Graphics Mode", "Performance Mode", "Render Scale", "Adaptive Resolution",
                               "Target Frame Rate", "Image Upscaler", "Background Effects", "UI Animations",
                               "High Quality Images", "FPS Overlay", "Reset to Defaults"};
  return kL[row];
}

std::string UiApp::perfRowValue(int row) const {
  auto onoff = [](bool b) { return std::string(b ? "On" : "Off"); };
  switch (row) {
    case 0: return onoff(settings_.lowGraphics);
    case 1: return onoff(settings_.performanceMode);
    case 2: {
      std::string s = std::to_string(settings_.renderScalePercent) + "%";
      if (config_.renderScalePercent != settings_.renderScalePercent && !config_.adaptive)
        s += " (using " + std::to_string(config_.renderScalePercent) + "%)";
      return s;
    }
    case 3: return onoff(settings_.adaptiveResolution) + (config_.adaptive && !settings_.adaptiveResolution ? " (by mode)" : "");
    case 4: return std::to_string(settings_.targetFps) + " FPS" + (config_.targetFps != settings_.targetFps ? " (60 by mode)" : "");
    case 5: {
      IUpscaler* u = upscalers_.find(settings_.upscaler);
      std::string s = u ? u->displayName() : settings_.upscaler;
      if (config_.upscaler != settings_.upscaler) s += " (nearest by mode)";
      return s;
    }
    case 6: return onoff(settings_.backgroundEffects);
    case 7: return onoff(settings_.uiAnimations);
    case 8: return onoff(settings_.highQualityImages);
    case 9: {
      static const char* k[3] = {"Off", "Simple", "Detailed"};
      return k[static_cast<int>(settings_.fpsOverlay)];
    }
    default: return "";
  }
}

std::string UiApp::perfRowHelp(int row) const {
  switch (row) {
    case 0: return "Caps render scale at 67%, uses a flat background, removes glow effects and uses nearest scaling.";
    case 1: return "Prioritises smooth frame pacing: 60 Hz target, adaptive resolution, no animated background.";
    case 2: return "Internal resolution of the UI. Below 100% the frame is upscaled to 1080p output.";
    case 3: return "Lowers/raises the render scale automatically from the measured CPU time per frame.";
    case 4: return "Display flip rate. 30 FPS halves CPU/memory work per second.";
    case 5: return upscalers_.find(settings_.upscaler) ? upscalers_.find(settings_.upscaler)->description() : "";
    case 6: return "Slowly drifting background glow. Costs a memory copy only; off in Performance/Low modes.";
    case 7: return "Smooth scrolling and focus transitions. Off = instant movement.";
    case 8: return "Bilinear sampling for game artwork instead of nearest.";
    case 9: return "Shows frame statistics measured from this app's real presented frames. OPTIONS cycles it anywhere.";
    case 10: return "Restore every option on this screen to its default value.";
  }
  return "";
}

void UiApp::perfRowAdjust(int row, int dir) {
  auto& s = settings_;
  switch (row) {
    case 0: s.lowGraphics = !s.lowGraphics; break;
    case 1: s.performanceMode = !s.performanceMode; break;
    case 2: {
      const auto& c = renderScaleChoices();
      int idx = 0;
      for (size_t i = 0; i < c.size(); ++i)
        if (c[i] == s.renderScalePercent) idx = static_cast<int>(i);
      idx = (idx + dir + static_cast<int>(c.size())) % static_cast<int>(c.size());
      s.renderScalePercent = c[static_cast<size_t>(idx)];
      break;
    }
    case 3: s.adaptiveResolution = !s.adaptiveResolution; break;
    case 4: s.targetFps = s.targetFps == 60 ? 30 : 60; break;
    case 5: {
      auto list = upscalers_.list();
      int idx = 0;
      for (size_t i = 0; i < list.size(); ++i)
        if (s.upscaler == list[i]->id()) idx = static_cast<int>(i);
      idx = (idx + dir + static_cast<int>(list.size())) % static_cast<int>(list.size());
      s.upscaler = list[static_cast<size_t>(idx)]->id();
      break;
    }
    case 6: s.backgroundEffects = !s.backgroundEffects; break;
    case 7: s.uiAnimations = !s.uiAnimations; break;
    case 8: s.highQualityImages = !s.highQualityImages; break;
    case 9: s.fpsOverlay = static_cast<FpsOverlay>((static_cast<int>(s.fpsOverlay) + dir + 3) % 3); break;
    case 10: {
      bool hidden = s.showHiddenGames;
      s = Settings();
      s.showHiddenGames = hidden;
      showToast("Performance settings reset");
      break;
    }
  }
  applyConfig();
  saveAll();
}

void UiApp::updatePerformance(const NavInput& in, double) {
  int n = perfRowCount();
  if (in.up) {
    if (perfIndex_ == 0) focusTabs_ = true;
    else --perfIndex_;
  }
  if (in.down) perfIndex_ = std::min(n - 1, perfIndex_ + 1);
  if (in.left && perfIndex_ != 10) perfRowAdjust(perfIndex_, -1);
  if (in.right && perfIndex_ != 10) perfRowAdjust(perfIndex_, +1);
  if (in.accept) perfRowAdjust(perfIndex_, +1);
}

void UiApp::renderPerformance() {
  r_.drawText(150, 160, "Performance & Image", FontId::Heading, theme::kText);
  Rect left{96, 215, 1010, 745};
  drawPanel(r_, left, 26);
  renderListRows(left.x + 20, left.y + 20, left.w - 40, 56, perfRowCount(), perfIndex_, &UiApp::perfRowLabel,
                 &UiApp::perfRowValue);
  std::string help = perfRowHelp(perfIndex_);
  r_.fillRect({left.x + 40, left.bottom() - 98, left.w - 80, 1.5f}, argb(60, 160, 190, 255));
  // wrap help text on two lines
  std::string l1 = help, l2;
  while (Renderer::measureText(l1, FontId::Small) > left.w - 80 && l1.size() > 4) {
    size_t sp = l1.rfind(' ');
    if (sp == std::string::npos) break;
    l2 = l1.substr(sp + 1) + (l2.empty() ? "" : " " + l2);
    l1 = l1.substr(0, sp);
  }
  r_.drawText(left.x + 40, left.bottom() - 84, l1, FontId::Small, theme::kTextDim);
  r_.drawText(left.x + 40, left.bottom() - 52, l2, FontId::Small, theme::kTextDim);

  Rect right{1140, 215, 684, 745};
  drawPanel(r_, right, 26);
  float x = right.x + 36, y = right.y + 28;
  r_.drawText(x, y, "LIVE MEASUREMENTS", FontId::Small, theme::kTextFaint);
  FpsStats s = fps_.stats();
  char buf[96];
  if (!platform_.hasReliableFrameTimer()) {
    r_.drawText(x, y + 40, "--", FontId::Title, theme::kTextFaint);
    r_.drawText(x, y + 112, "No reliable frame timer on this platform.", FontId::Small, theme::kWarn);
  } else if (!s.valid) {
    r_.drawText(x, y + 40, "--", FontId::Title, theme::kTextFaint);
    r_.drawText(x, y + 112, "Collecting samples...", FontId::Small, theme::kTextDim);
  } else {
    std::snprintf(buf, sizeof(buf), "%.1f", s.fps);
    float w = r_.drawText(x, y + 40, buf, FontId::Title, theme::kText);
    r_.drawText(x + w + 12, y + 70, "FPS", FontId::Body, theme::kTextDim);
    std::snprintf(buf, sizeof(buf), "Frame time  avg %.1f   min %.1f   max %.1f   p99 %.1f ms", s.avgFrameMs, s.minFrameMs, s.maxFrameMs, s.p99FrameMs);
    r_.drawText(x, y + 112, Renderer::fitText(buf, FontId::Small, right.w - 72), FontId::Small, theme::kTextDim);
  }
  std::snprintf(buf, sizeof(buf), "%.1f ms", workMsEma_);
  r_.drawText(x, y + 160, "CPU work (render + scale + copy)", FontId::Small, theme::kTextFaint);
  r_.drawTextRight(right.right() - 36, y + 160, buf, FontId::Small, theme::kText);
  std::snprintf(buf, sizeof(buf), "%dx%d  (%d%%)", surface_.width(), surface_.height(), renderPercent_);
  r_.drawText(x, y + 196, "Internal resolution", FontId::Small, theme::kTextFaint);
  r_.drawTextRight(right.right() - 36, y + 196, buf, FontId::Small, theme::kText);
  r_.drawText(x, y + 232, "Output", FontId::Small, theme::kTextFaint);
  r_.drawTextRight(right.right() - 36, y + 232, "1920x1080", FontId::Small, theme::kText);
  r_.drawText(x, y + 268, "Upscaler in use", FontId::Small, theme::kTextFaint);
  IUpscaler* u = upscalers_.find(config_.upscaler);
  r_.drawTextRight(right.right() - 36, y + 268, renderPercent_ >= 100 ? "none (native)" : (u ? u->displayName() : "?"), FontId::Small, theme::kText);
  r_.drawText(x, y + 304, "Target", FontId::Small, theme::kTextFaint);
  std::snprintf(buf, sizeof(buf), "%d FPS%s", config_.targetFps, config_.adaptive ? "  (adaptive)" : "");
  r_.drawTextRight(right.right() - 36, y + 304, buf, FontId::Small, theme::kText);

  // frame-time history (real measured intervals)
  Rect g{x, y + 360, right.w - 72, 170};
  r_.fillRoundRect(g, 12, argb(90, 6, 10, 30));
  if (!frameHistory_.empty()) {
    float budget = 1000.0f / static_cast<float>(config_.targetFps);
    float maxMs = 50.0f;
    float bw = g.w / 120.0f;
    for (size_t i = 0; i < frameHistory_.size(); ++i) {
      float ms = std::min(frameHistory_[i], maxMs);
      float h = ms / maxMs * (g.h - 12);
      uint32_t c = frameHistory_[i] <= budget * 1.2f ? theme::kGood : (frameHistory_[i] <= budget * 2.2f ? theme::kWarn : theme::kBad);
      float bx = g.x + g.w - static_cast<float>(frameHistory_.size() - i) * bw;
      r_.fillRect({bx, g.bottom() - 6 - h, std::max(1.0f, bw - 1), h}, c);
    }
    float ly = g.bottom() - 6 - budget / maxMs * (g.h - 12);
    r_.fillRect({g.x, ly, g.w, 1.5f}, argb(160, 255, 255, 255));
  }
  r_.drawText(x, g.bottom() + 12, "Frame time history (white line = target)", FontId::Small, theme::kTextFaint);
  r_.drawText(x, right.bottom() - 92, "Measured from this app's presented frames only.", FontId::Small, theme::kTextFaint);
  r_.drawText(x, right.bottom() - 60, "GPU/CPU load of other titles is not available.", FontId::Small, theme::kTextFaint);
}

// ------------------------------------------------------------------- Settings

void UiApp::updateSettingsScreen(const NavInput& in, double) {
  if (in.up) {
    if (setIndex_ == 0) focusTabs_ = true;
    else --setIndex_;
  }
  if (in.down) setIndex_ = std::min(3, setIndex_ + 1);
  bool act = in.accept || in.left || in.right;
  if (!act) return;
  switch (setIndex_) {
    case 0:
      if (in.accept) {
        library_.scan();
        viewsDirty_ = true;
        showToast("Library rescanned: " + std::to_string(library_.all().size()) + " title(s)");
      }
      break;
    case 1:
      settings_.showHiddenGames = !settings_.showHiddenGames;
      saveAll();
      viewsDirty_ = true;
      break;
    case 2:
      if (in.accept) modal_ = Modal::About;
      break;
    case 3:
      if (in.accept) requestExit();
      break;
  }
}

void UiApp::renderSettingsScreen() {
  r_.drawText(150, 160, "Settings", FontId::Heading, theme::kText);
  Rect left{96, 215, 1010, 330};
  drawPanel(r_, left, 26);
  for (int i = 0; i < 4; ++i) {
    static const char* k[4] = {"Rescan Library", "Show Hidden Games", "About", "Exit Application"};
    Rect row{left.x + 20, left.y + 20 + static_cast<float>(i) * 72, left.w - 40, 66};
    bool sel = (i == setIndex_) && !focusTabs_;
    if (sel) {
      r_.fillRoundRect(row, 16, theme::kRowFocus);
      r_.strokeRoundRect(row, 16, 2.5f, theme::kText);
    }
    r_.drawText(row.x + 24, row.y + 17, k[i], FontId::Body, sel ? theme::kText : theme::kTextDim);
    if (i == 0) r_.drawTextRight(row.right() - 24, row.y + 17, std::to_string(library_.all().size()) + " titles", FontId::Body, theme::kAccent);
    if (i == 1) drawSwitch(r_, {row.right() - 100, row.y + 13, 76, 40}, settings_.showHiddenGames);
  }
  Rect right{1140, 215, 684, 330};
  drawPanel(r_, right, 26);
  float x = right.x + 36, y = right.y + 28;
  r_.drawText(x, y, "LIBRARY FOLDERS", FontId::Small, theme::kTextFaint);
  y += 44;
  for (const auto& root : library_.scanRoots()) {
    bool ok = fs::isDirectory(root);
    r_.fillCircle(x + 8, y + 15, 6, ok ? theme::kGood : theme::kBad);
    r_.drawText(x + 28, y, Renderer::fitText(root, FontId::Small, right.w - 110), FontId::Small, theme::kText);
    y += 36;
  }
  r_.drawText(x, right.bottom() - 70, "Data folder:", FontId::Small, theme::kTextFaint);
  r_.drawText(x + 130, right.bottom() - 70, Renderer::fitText(dataDir_, FontId::Small, right.w - 190), FontId::Small, theme::kTextDim);

  Rect note{96, 575, 1728, 190};
  drawPanel(r_, note, 26);
  r_.drawText(note.x + 36, note.y + 28, "What this app is", FontId::Body, theme::kText);
  r_.drawText(note.x + 36, note.y + 76, "A launcher UI for PS4 homebrew with a PS5-inspired look. It is not a PS5 emulator and does not run PS5 games.", FontId::Small, theme::kTextDim);
  r_.drawText(note.x + 36, note.y + 112, "It only reads metadata (param.sfo / icon0.png) of titles already installed on this console.", FontId::Small, theme::kTextDim);
}

}  // namespace ps5ui
