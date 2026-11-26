#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/AdaptiveResolution.h"
#include "core/FpsCounter.h"
#include "core/Input.h"
#include "core/Library.h"
#include "core/Settings.h"
#include "core/Upscaler.h"
#include "gfx/Background.h"
#include "gfx/Renderer.h"
#include "platform/Platform.h"

namespace ps5ui {

enum class Tab { Home = 0, Library = 1, Performance = 2, Settings = 3 };
enum class Modal { None, GameDetails, About };

class UiApp {
 public:
  explicit UiApp(IPlatform& platform);

  // Optional: override the directory used for settings/library data (tests).
  void setDataDirectory(const std::string& dir) { dataDir_ = dir; }

  bool init();
  void shutdown();
  // Runs one frame (input -> update -> render -> present). Returns false once exit was requested.
  bool frame();
  void run();

  // Inspection hooks (tests / host tools).
  Settings& settings() { return settings_; }
  Library& library() { return library_; }
  const Surface& frameSurface() const { return surface_; }
  FpsStats fpsStats() const { return fps_.stats(); }
  int renderPercent() const { return renderPercent_; }
  Tab tab() const { return tab_; }
  Modal modal() const { return modal_; }
  const EffectiveConfig& config() const { return config_; }
  void requestExit() { exitRequested_ = true; }
  // Re-resolves modes/settings after they were changed programmatically (the UI does this itself).
  void applySettings() {
    applyConfig();
    ensureSurfaces();
  }

 private:
  // --- flow (UiApp.cpp)
  void update(double dt, const NavInput& in);
  void render();
  void applyConfig();
  void ensureSurfaces();
  void saveAll();
  void showToast(const std::string& text);
  float smooth(float current, float target, float rate, double dt) const;

  void renderTopBar();
  void renderHints();
  void renderOverlay();     // FPS overlay + toast
  void renderModal();
  void updateModal(const NavInput& in);
  void openDetails(int gameIndex);
  void launchSelected();

  // --- icons
  const Image* iconFor(const GameEntry& g);
  void pumpIcons();

  // --- screens (Screens.cpp)
  void refreshViews();
  void updateHome(const NavInput& in, double dt);
  void renderHome();
  void updateLibrary(const NavInput& in, double dt);
  void renderLibrary();
  void updatePerformance(const NavInput& in, double dt);
  void renderPerformance();
  void updateSettingsScreen(const NavInput& in, double dt);
  void renderSettingsScreen();

  std::string perfRowLabel(int row) const;
  std::string perfRowValue(int row) const;
  std::string perfRowHelp(int row) const;
  void perfRowAdjust(int row, int dir);
  int perfRowCount() const;
  void renderListRows(float x, float y, float w, float rowH, int count, int focus,
                      std::string (UiApp::*label)(int) const, std::string (UiApp::*value)(int) const);

  IPlatform& platform_;
  std::string dataDir_;
  Settings settings_;
  EffectiveConfig config_;
  Library library_;
  UpscalerRegistry upscalers_;
  InputTracker inputTracker_;
  FpsCounter fps_;
  AdaptiveResolution adaptive_;
  BackgroundCache background_;
  Surface surface_;
  Renderer r_;

  int outW_ = 1920, outH_ = 1080;
  int renderPercent_ = 100;
  bool exitRequested_ = false;
  bool controllerConnected_ = false;
  uint64_t frameStartUs_ = 0, lastFrameStartUs_ = 0, frameCounter_ = 0;
  double animTime_ = 0;
  double workMsEma_ = 0;
  std::vector<float> frameHistory_;  // measured present-to-present intervals (ms)
  uint64_t lastPresentUs_ = 0;

  // navigation state
  Tab tab_ = Tab::Home;
  bool focusTabs_ = false;
  float tabPos_ = 0;
  Modal modal_ = Modal::None;
  int modalGame_ = -1;
  int modalIndex_ = 0;

  std::vector<int> homeView_, libView_;
  int homeIndex_ = 0, libIndex_ = 0, perfIndex_ = 0, setIndex_ = 0;
  float homeScroll_ = 0, libScrollRow_ = 0;
  LibrarySort librarySort_ = LibrarySort::TitleAZ;
  LibraryFilter libraryFilter_ = LibraryFilter::All;
  bool viewsDirty_ = true;

  std::string toast_;
  double toastTime_ = 0;

  struct IconEntry {
    Image thumb;
    bool loaded = false;
    bool failed = false;
    uint64_t lastUsed = 0;
  };
  std::map<std::string, IconEntry> icons_;
  std::vector<std::string> iconQueue_;
};

}  // namespace ps5ui
