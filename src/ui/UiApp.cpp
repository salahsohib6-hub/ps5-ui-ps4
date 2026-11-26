#include "ui/UiApp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/FileSystem.h"
#include "core/Log.h"
#include "ui/Widgets.h"

namespace ps5ui {

using namespace ui;

namespace {
constexpr const char* kAppName = "PS5 Emulator UI for PS4";
constexpr const char* kVersion = "1.0.0";

int evenRound(double v) {
  int i = static_cast<int>(std::lround(v));
  return i & ~1;
}
}  // namespace

UiApp::UiApp(IPlatform& platform) : platform_(platform) {}

// ------------------------------------------------------------------ lifecycle

bool UiApp::init() {
  if (dataDir_.empty()) dataDir_ = platform_.dataDirectory();
  fs::makeDirs(dataDir_);
  log::open(fs::join(dataDir_, "log.txt").c_str());
  log::write("%s %s starting on %s", kAppName, kVersion, platform_.name());

  if (!platform_.init(kLogicalWidth, kLogicalHeight)) {
    log::write("platform init failed");
    return false;
  }
  outW_ = kLogicalWidth;
  outH_ = kLogicalHeight;

  registerBuiltinUpscalers(upscalers_);
  settings_.load(fs::join(dataDir_, "settings.cfg"));

  std::vector<std::string> roots;
  std::string pathsFile = fs::join(dataDir_, "library_paths.txt");
  std::string text;
  if (fs::readTextFile(pathsFile, text)) {
    size_t pos = 0;
    while (pos < text.size()) {
      size_t eol = text.find('\n', pos);
      if (eol == std::string::npos) eol = text.size();
      std::string line = text.substr(pos, eol - pos);
      pos = eol + 1;
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
      if (!line.empty() && line[0] != '#') roots.push_back(line);
    }
  } else {
    roots = {"/user/app", "/mnt/ext0/user/app"};
    fs::writeTextFile(pathsFile,
                      "# One folder per line. Each folder is scanned for <TITLE_ID>/sce_sys/param.sfo\n"
                      "/user/app\n/mnt/ext0/user/app\n");
  }
  library_.setScanRoots(roots);
  library_.loadUserData(fs::join(dataDir_, "library.cfg"));
  library_.scan();

  applyConfig();
  ensureSurfaces();
  lastFrameStartUs_ = platform_.nowMicros();
  return true;
}

void UiApp::saveAll() {
  settings_.save(fs::join(dataDir_, "settings.cfg"));
  library_.saveUserData(fs::join(dataDir_, "library.cfg"));
}

void UiApp::shutdown() {
  saveAll();
  platform_.shutdown();
  log::write("shutdown");
  log::close();
}

void UiApp::run() {
  while (frame()) {
  }
}

void UiApp::applyConfig() {
  settings_.sanitize();
  config_ = resolve(settings_);
  if (!upscalers_.find(config_.upscaler)) config_.upscaler = "nearest";
  adaptive_.configure(config_.adaptive, config_.minScalePercent, config_.maxScalePercent,
                      config_.renderScalePercent, 1000.0 / static_cast<double>(config_.targetFps));
  renderPercent_ = adaptive_.percent();
  platform_.setTargetFps(config_.targetFps);
  fps_.reset();
  frameHistory_.clear();
  lastPresentUs_ = 0;
  workMsEma_ = 0;
}

void UiApp::ensureSurfaces() {
  int w = std::max(320, evenRound(outW_ * renderPercent_ / 100.0));
  int h = std::max(180, evenRound(static_cast<double>(w) * outH_ / outW_));
  if (surface_.width() != w || surface_.height() != h) surface_.allocate(w, h);
  background_.ensure(w, h, config_.staticGlowBackground);
}

float UiApp::smooth(float current, float target, float rate, double dt) const {
  if (!config_.uiAnimations) return target;
  float k = 1.0f - std::exp(-rate * static_cast<float>(dt));
  float v = current + (target - current) * k;
  return std::fabs(target - v) < 0.0005f ? target : v;
}

void UiApp::showToast(const std::string& text) {
  toast_ = text;
  toastTime_ = 3.0;
}

// ---------------------------------------------------------------------- frame

bool UiApp::frame() {
  frameStartUs_ = platform_.nowMicros();
  double dt = static_cast<double>(frameStartUs_ - lastFrameStartUs_) / 1e6;
  lastFrameStartUs_ = frameStartUs_;
  if (dt <= 0 || dt > 0.1) dt = 1.0 / 60.0;

  PadState pad;
  platform_.pollPad(pad);
  controllerConnected_ = pad.connected;
  NavInput in = inputTracker_.update(pad, dt);

  if (viewsDirty_) refreshViews();
  update(dt, in);
  ensureSurfaces();
  pumpIcons();
  render();

  DisplayBuffer out = platform_.beginFrame();
  ImageView src{surface_.data(), surface_.width(), surface_.height(), surface_.pitch()};
  ImageTarget dst{out.pixels, out.width, out.height, out.pitch};
  presentScaled(src, dst, upscalers_, config_.upscaler);
  uint64_t workEnd = platform_.nowMicros();

  platform_.endFrame();  // flip + wait for vsync
  uint64_t presented = platform_.nowMicros();

  double workMs = static_cast<double>(workEnd - frameStartUs_) / 1000.0;
  workMsEma_ = (workMsEma_ <= 0) ? workMs : workMsEma_ * 0.9 + workMs * 0.1;
  if (platform_.hasReliableFrameTimer()) {
    fps_.onFramePresented(presented);
    if (lastPresentUs_ != 0) {
      frameHistory_.push_back(static_cast<float>(presented - lastPresentUs_) / 1000.0f);
      if (frameHistory_.size() > 120) frameHistory_.erase(frameHistory_.begin());
    }
    lastPresentUs_ = presented;
  }
  if (adaptive_.onFrame(workMs)) {
    renderPercent_ = adaptive_.percent();
    log::write("adaptive resolution -> %d%% (work %.2f ms)", renderPercent_, workMs);
  }
  ++frameCounter_;
  return !exitRequested_;
}

void UiApp::update(double dt, const NavInput& in) {
  animTime_ += dt;
  tabPos_ = smooth(tabPos_, static_cast<float>(tab_), 14.0f, dt);
  if (toastTime_ > 0) toastTime_ -= dt;

  if (modal_ != Modal::None) {
    updateModal(in);
    return;
  }
  if (in.options) {
    int o = (static_cast<int>(settings_.fpsOverlay) + 1) % 3;
    settings_.fpsOverlay = static_cast<FpsOverlay>(o);
    saveAll();
  }
  auto switchTab = [&](int d) {
    int t = (static_cast<int>(tab_) + d + 4) % 4;
    tab_ = static_cast<Tab>(t);
  };
  if (in.l1) switchTab(-1);
  if (in.r1) switchTab(+1);

  if (focusTabs_) {
    if (in.left) tab_ = static_cast<Tab>(std::max(0, static_cast<int>(tab_) - 1));
    if (in.right) tab_ = static_cast<Tab>(std::min(3, static_cast<int>(tab_) + 1));
    if (in.down || in.accept) focusTabs_ = false;
    return;
  }
  switch (tab_) {
    case Tab::Home: updateHome(in, dt); break;
    case Tab::Library: updateLibrary(in, dt); break;
    case Tab::Performance: updatePerformance(in, dt); break;
    case Tab::Settings: updateSettingsScreen(in, dt); break;
  }
}

void UiApp::render() {
  float scale = static_cast<float>(surface_.width()) / static_cast<float>(kLogicalWidth);
  r_.begin(surface_, scale);
  float pan = config_.animatedBackground
                  ? 0.5f + 0.5f * std::sin(static_cast<float>(animTime_) * 0.10f)
                  : 0.35f;
  background_.blit(surface_, pan);

  switch (tab_) {
    case Tab::Home: renderHome(); break;
    case Tab::Library: renderLibrary(); break;
    case Tab::Performance: renderPerformance(); break;
    case Tab::Settings: renderSettingsScreen(); break;
  }
  renderTopBar();
  renderHints();
  renderOverlay();
  if (modal_ != Modal::None) renderModal();
}

// -------------------------------------------------------------------- chrome

void UiApp::renderTopBar() {
  // soft scrim so scrolling content never collides with the header
  r_.fillVGradient({0, 0, 1920, 150}, argb(190, 4, 7, 22), argb(0, 4, 7, 22));

  r_.fillRoundRectGradient({96, 36, 58, 58}, 17, theme::kAccent, theme::kAccent2);
  r_.drawTextCentered(125, 50, "UI", FontId::Heading, rgb(255, 255, 255));
  r_.drawText(172, 34, "PS5 Emulator UI", FontId::Heading, theme::kText);
  r_.drawText(172, 72, "for PS4 homebrew", FontId::Small, theme::kTextDim);

  static const char* kTabs[4] = {"Home", "Library", "Performance", "Settings"};
  float x = 560;
  float tx[4], tw[4];
  for (int i = 0; i < 4; ++i) {
    tw[i] = Renderer::measureText(kTabs[i], FontId::Body) + 46;
    tx[i] = x;
    x += tw[i] + 24;
  }
  // animated selection pill
  int a = std::min(3, static_cast<int>(tabPos_));
  int b = std::min(3, a + 1);
  float f = tabPos_ - static_cast<float>(a);
  float px = tx[a] + (tx[b] - tx[a]) * f;
  float pw = tw[a] + (tw[b] - tw[a]) * f;
  if (focusTabs_) {
    r_.fillRoundRect({px - 8, 34, pw + 16, 62}, 31, theme::kRowFocus);
    r_.strokeRoundRect({px - 8, 34, pw + 16, 62}, 31, 2.5f, theme::kText);
  }
  r_.fillRoundRect({px + 14, 90, pw - 28, 5}, 2.5f, theme::kAccent);
  for (int i = 0; i < 4; ++i) {
    bool active = (static_cast<int>(tab_) == i);
    uint32_t c = active ? theme::kText : theme::kTextDim;
    drawTabIcon(r_, i, tx[i] + 22, 65, 10, c);
    r_.drawText(tx[i] + 44, 50, kTabs[i], FontId::Body, c);
  }

  // controller status
  const char* cs = controllerConnected_ ? "DualShock 4 connected" : "No controller";
  float w = Renderer::measureText(cs, FontId::Small);
  r_.drawText(1824 - w, 56, cs, FontId::Small, theme::kTextDim);
  r_.fillCircle(1824 - w - 18, 69, 6, controllerConnected_ ? theme::kGood : theme::kBad);
}

void UiApp::renderHints() {
  r_.fillVGradient({0, 980, 1920, 100}, argb(0, 3, 5, 16), argb(200, 3, 5, 16));
  float x = 96, y = 1012;
  auto gap = [&](float w) { x += w + 44; };
  if (modal_ != Modal::None) {
    gap(drawGlyphHint(r_, x, y, PadGlyph::Cross, "Select"));
    gap(drawGlyphHint(r_, x, y, PadGlyph::Circle, "Close"));
    return;
  }
  if (focusTabs_) {
    gap(drawKeyHint(r_, x, y, "< >", "Change tab"));
    gap(drawKeyHint(r_, x, y, "Down", "Back to content"));
    return;
  }
  switch (tab_) {
    case Tab::Home:
      gap(drawGlyphHint(r_, x, y, PadGlyph::Cross, homeView_.empty() ? "Rescan" : "Details"));
      gap(drawGlyphHint(r_, x, y, PadGlyph::Square, "Favorite"));
      break;
    case Tab::Library:
      gap(drawGlyphHint(r_, x, y, PadGlyph::Cross, "Details"));
      gap(drawGlyphHint(r_, x, y, PadGlyph::Square, "Favorite"));
      gap(drawGlyphHint(r_, x, y, PadGlyph::Triangle, "Sort"));
      gap(drawKeyHint(r_, x, y, "L2 / R2", "Filter"));
      break;
    case Tab::Performance:
      gap(drawKeyHint(r_, x, y, "< >", "Adjust"));
      gap(drawGlyphHint(r_, x, y, PadGlyph::Cross, "Change"));
      break;
    case Tab::Settings:
      gap(drawGlyphHint(r_, x, y, PadGlyph::Cross, "Select"));
      break;
  }
  gap(drawKeyHint(r_, x, y, "L1 / R1", "Tabs"));
  gap(drawKeyHint(r_, x, y, "OPTIONS", "FPS overlay"));
}

void UiApp::renderOverlay() {
  if (settings_.fpsOverlay != FpsOverlay::Off) {
    FpsStats s = fps_.stats();
    char line[96];
    std::vector<std::string> lines;
    if (!platform_.hasReliableFrameTimer()) {
      lines.push_back("FPS: N/A (no reliable frame timer)");
    } else if (!s.valid) {
      lines.push_back("FPS: measuring...");
    } else {
      std::snprintf(line, sizeof(line), "%.1f FPS", s.fps);
      lines.push_back(line);
      if (settings_.fpsOverlay == FpsOverlay::Detailed) {
        std::snprintf(line, sizeof(line), "Frame %.1f ms (min %.1f / max %.1f)", s.avgFrameMs, s.minFrameMs, s.maxFrameMs);
        lines.push_back(line);
        std::snprintf(line, sizeof(line), "CPU work %.1f ms", workMsEma_);
        lines.push_back(line);
        std::snprintf(line, sizeof(line), "Render %dx%d (%d%%)", surface_.width(), surface_.height(), renderPercent_);
        lines.push_back(line);
      }
    }
    float w = 0;
    for (auto& l : lines) w = std::max(w, Renderer::measureText(l, FontId::Small));
    float h = 18 + static_cast<float>(lines.size()) * 30;
    Rect rc{1824 - w - 36, 112, w + 36, h};
    r_.fillRoundRect(rc, 16, argb(200, 8, 12, 34));
    r_.strokeRoundRect(rc, 16, 1.5f, theme::kPanelStroke);
    float y = rc.y + 10;
    for (size_t i = 0; i < lines.size(); ++i) {
      r_.drawText(rc.x + 18, y, lines[i], FontId::Small, i == 0 ? theme::kText : theme::kTextDim);
      y += 30;
    }
  }
  if (toastTime_ > 0 && !toast_.empty()) {
    float a = static_cast<float>(std::min(1.0, toastTime_));
    float w = Renderer::measureText(toast_, FontId::Body) + 64;
    Rect rc{960 - w / 2, 900, w, 62};
    r_.fillRoundRect(rc, 31, argb(static_cast<uint32_t>(225 * a), 14, 20, 52));
    r_.strokeRoundRect(rc, 31, 1.5f, argb(static_cast<uint32_t>(120 * a), 150, 180, 255));
    r_.drawTextCentered(960, rc.y + 16, toast_, FontId::Body, argb(static_cast<uint32_t>(255 * a), 240, 244, 255));
  }
}

// --------------------------------------------------------------------- modals

namespace {
std::string fwString(uint32_t v) {
  if (v == 0) return "Any";
  char b[16];
  std::snprintf(b, sizeof(b), "%X.%02X", (v >> 24) & 0xFF, (v >> 16) & 0xFF);
  return b;
}
}  // namespace

void UiApp::openDetails(int gameIndex) {
  if (gameIndex < 0 || gameIndex >= static_cast<int>(library_.all().size())) return;
  modal_ = Modal::GameDetails;
  modalGame_ = gameIndex;
  modalIndex_ = 0;
}

void UiApp::launchSelected() {
  const GameEntry& g = library_.all()[static_cast<size_t>(modalGame_)];
  if (!platform_.canLaunchTitles()) {
    showToast("Launching other titles is disabled in this build (see README)");
    return;
  }
  std::string err;
  if (platform_.launchTitle(g.titleId, err)) {
    library_.recordLaunch(g.titleId, platform_.unixTime());
    saveAll();
    viewsDirty_ = true;
    showToast("Launching " + g.title + "...");
  } else {
    showToast("Launch failed: " + err);
  }
}

void UiApp::updateModal(const NavInput& in) {
  if (modal_ == Modal::About) {
    if (in.accept || in.back) modal_ = Modal::None;
    return;
  }
  if (modalGame_ < 0 || modalGame_ >= static_cast<int>(library_.all().size())) {
    modal_ = Modal::None;
    return;
  }
  if (in.up) modalIndex_ = std::max(0, modalIndex_ - 1);
  if (in.down) modalIndex_ = std::min(3, modalIndex_ + 1);
  if (in.back) {
    modal_ = Modal::None;
    return;
  }
  if (in.accept) {
    const std::string id = library_.all()[static_cast<size_t>(modalGame_)].titleId;
    switch (modalIndex_) {
      case 0: launchSelected(); break;
      case 1:
        library_.toggleFavorite(id);
        saveAll();
        viewsDirty_ = true;
        break;
      case 2:
        library_.toggleHidden(id);
        saveAll();
        viewsDirty_ = true;
        if (!settings_.showHiddenGames) modal_ = Modal::None;
        break;
      default: modal_ = Modal::None; break;
    }
  }
}

void UiApp::renderModal() {
  r_.fillRect({0, 0, 1920, 1080}, argb(185, 2, 4, 14));
  if (modal_ == Modal::About) {
    Rect p{360, 190, 1200, 700};
    r_.fillRoundRect(p, 28, argb(242, 8, 12, 34));
    drawPanel(r_, p, 28);
    r_.drawText(p.x + 56, p.y + 44, kAppName, FontId::Title, theme::kText);
    std::string v = std::string("Version ") + kVersion + "  |  built with the OpenOrbis PS4 Toolchain";
    r_.drawText(p.x + 56, p.y + 116, v, FontId::Body, theme::kTextDim);
    const char* lines[] = {
        "A PS5-inspired launcher interface for PS4 homebrew.",
        "It is NOT a PS5 emulator and cannot run PS5 games.",
        "No game files, keys or firmware are included or needed.",
        "The library lists titles already installed on this console.",
        "FPS values come from real presented-frame timestamps of this app only.",
        "Font: Inter, SIL Open Font License 1.1.",
    };
    float y = p.y + 190;
    for (const char* l : lines) {
      r_.drawText(p.x + 56, y, l, FontId::Body, theme::kText);
      y += 56;
    }
    return;
  }
  if (modalGame_ < 0 || modalGame_ >= static_cast<int>(library_.all().size())) return;
  const GameEntry& g = library_.all()[static_cast<size_t>(modalGame_)];
  Rect p{280, 170, 1360, 740};
  r_.fillRoundRect(p, 28, argb(242, 8, 12, 34));
  drawPanel(r_, p, 28);
  Rect cover{p.x + 48, p.y + 48, 340, 340};
  drawCover(r_, g, iconFor(g), cover, 24, config_.bilinearImages);
  float x = cover.right() + 52, y = p.y + 44;
  r_.drawText(x, y, Renderer::fitText(g.title, FontId::Heading, p.right() - x - 48), FontId::Heading, theme::kText);
  y += 62;
  auto kv = [&](const char* k, const std::string& val) {
    r_.drawText(x, y, k, FontId::Small, theme::kTextFaint);
    r_.drawText(x + 190, y, Renderer::fitText(val, FontId::Small, p.right() - x - 190 - 48), FontId::Small, theme::kTextDim);
    y += 38;
  };
  kv("Title ID", g.titleId);
  kv("Version", g.version.empty() ? "-" : g.version);
  kv("Min firmware", fwString(g.minFirmware));
  kv("Category", g.category);
  kv("Content ID", g.contentId.empty() ? "-" : g.contentId);
  kv("Launched", std::to_string(g.launchCount) + (g.launchCount == 1 ? " time" : " times"));
  kv("Location", g.path);

  const char* labels[4] = {"Launch", g.favorite ? "Remove from favorites" : "Add to favorites",
                           g.hidden ? "Show in library" : "Hide from library", "Close"};
  float by = p.y + 430;
  for (int i = 0; i < 4; ++i) {
    Rect row{p.x + 48, by + static_cast<float>(i) * 62, 520, 52};
    bool sel = (i == modalIndex_);
    if (sel) {
      r_.fillRoundRect(row, 26, theme::kRowFocus);
      r_.strokeRoundRect(row, 26, 2.5f, theme::kText);
    }
    uint32_t c = (i == 0 && !platform_.canLaunchTitles()) ? theme::kTextFaint : theme::kText;
    r_.drawText(row.x + 28, row.y + 10, labels[i], FontId::Body, sel ? theme::kText : (c == theme::kText ? theme::kTextDim : c));
  }
  if (!platform_.canLaunchTitles())
    r_.drawText(p.x + 620, by + 14, "Launching is disabled in this build (see README).", FontId::Small, theme::kTextFaint);
}

// ---------------------------------------------------------------------- icons

const Image* UiApp::iconFor(const GameEntry& g) {
  auto it = icons_.find(g.titleId);
  if (it == icons_.end()) {
    icons_[g.titleId] = IconEntry();
    iconQueue_.push_back(g.titleId);
    return nullptr;
  }
  it->second.lastUsed = frameCounter_;
  return it->second.loaded ? &it->second.thumb : nullptr;
}

void UiApp::pumpIcons() {
  if (!iconQueue_.empty()) {
    std::string id = iconQueue_.front();
    iconQueue_.erase(iconQueue_.begin());
    IconEntry& e = icons_[id];
    int gi = library_.indexOf(id);
    if (gi >= 0) {
      std::vector<uint8_t> bytes;
      Image full;
      if (fs::readFile(library_.all()[static_cast<size_t>(gi)].iconPath, bytes, 4u << 20) &&
          decodePng(bytes.data(), bytes.size(), full, 2048)) {
        e.thumb = resizeBox(full, 320, 320);
        e.loaded = e.thumb.valid();
      }
    }
    e.failed = !e.loaded;
    e.lastUsed = frameCounter_;
  }
  // Keep memory bounded: at most 24 decoded thumbnails (400 KiB each).
  size_t loaded = 0;
  for (auto& kv : icons_) loaded += kv.second.loaded ? 1 : 0;
  while (loaded > 24) {
    auto victim = icons_.end();
    for (auto it = icons_.begin(); it != icons_.end(); ++it)
      if (it->second.loaded && (victim == icons_.end() || it->second.lastUsed < victim->second.lastUsed)) victim = it;
    if (victim == icons_.end()) break;
    icons_.erase(victim);
    --loaded;
  }
}

}  // namespace ps5ui
