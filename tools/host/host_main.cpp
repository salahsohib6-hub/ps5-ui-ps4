// Desktop development harness: runs the real UI against the headless HostPlatform
// and writes screenshots (PPM). Not part of the PS4 build.
//
//   host_ui --data DIR --out DIR [--demo] [--icon PATH] [--realtime]
#include <cstdio>
#include <cstring>
#include <string>

#include "platform/host/HostPlatform.h"
#include "tests/Fixtures.h"
#include "ui/UiApp.h"

using namespace ps5ui;

static void savePpm(const HostPlatform& hp, const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", hp.width(), hp.height());
  for (uint32_t p : hp.lastFrame()) {
    unsigned char rgb[3] = {static_cast<unsigned char>(p >> 16), static_cast<unsigned char>(p >> 8), static_cast<unsigned char>(p)};
    std::fwrite(rgb, 1, 3, f);
  }
  std::fclose(f);
}

int main(int argc, char** argv) {
  std::string data = "./host_data", out = "./shots", icon;
  bool demo = false, realtime = false;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--data") && i + 1 < argc) data = argv[++i];
    else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    else if (!std::strcmp(argv[i], "--icon") && i + 1 < argc) icon = argv[++i];
    else if (!std::strcmp(argv[i], "--demo")) demo = true;
    else if (!std::strcmp(argv[i], "--realtime")) realtime = true;
  }
  fs::makeDirs(out);
  std::string apps = fs::join(data, "apps");
  if (demo) {
    const char* ids[] = {"CUSA00001", "CUSA00002", "CUSA00003", "CUSA00004", "CUSA00005", "CUSA00006", "CUSA00007", "CUSA00008", "CUSA00009"};
    const char* names[] = {"Neon Drift Racing", "Skyward Chronicles", "Pixel Harbor", "Echoes of Orion", "Mountain Bike Rally",
                           "Dungeon Lantern", "Starlight Tactics", "Ocean Keeper", "Retro Arcade Pack"};
    for (int i = 0; i < 9; ++i) fixtures::makeGame(apps, ids[i], names[i], "gd", "01.0" + std::to_string(i % 4), 0x05050000 + (i % 3) * 0x00500000, (i % 3 == 0) ? icon : "");
    fixtures::makeGame(apps, "CUSA00099", "Patch Only", "gp");  // must be ignored
  }
  fs::makeDirs(data);
  fs::writeTextFile(fs::join(data, "library_paths.txt"), apps + "\n");

  HostPlatform hp(data);
  hp.useSimulatedClock(!realtime);
  UiApp app(hp);
  if (!app.init()) return 1;

  auto step = [&](uint32_t buttons, int frames) {
    PadState p;
    p.connected = true;
    p.buttons = buttons;
    hp.setPad(p);
    for (int i = 0; i < frames; ++i) app.frame();
  };
  auto press = [&](uint32_t b) { step(b, 1); step(0, 3); };
  auto shot = [&](const char* name) {
    step(0, 40);
    savePpm(hp, fs::join(out, name));
    std::printf("saved %s (render %d%%)\n", name, app.renderPercent());
  };

  step(0, 30);
  press(pad::kRight); press(pad::kRight);
  shot("01_home.ppm");
  press(pad::kCross);
  step(0, 5);
  shot("02_details.ppm");
  press(pad::kCircle);
  press(pad::kR1);
  press(pad::kSquare);  // (library) favourite
  shot("03_library.ppm");
  press(pad::kR1);
  press(pad::kOptions);
  press(pad::kOptions);
  shot("04_performance.ppm");
  press(pad::kDown); press(pad::kCross);  // Performance Mode on
  press(pad::kDown); press(pad::kDown); press(pad::kLeft);
  shot("05_performance_modes.ppm");
  press(pad::kR1);
  shot("06_settings.ppm");
  app.shutdown();
  return 0;
}
