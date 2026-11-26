// Self-contained test runner (no framework). Usage: tests <png-fixture-dir> <pkg-sce_sys-dir>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <string>

#include "core/AdaptiveResolution.h"
#include "core/FileSystem.h"
#include "core/FpsCounter.h"
#include "core/Library.h"
#include "core/Settings.h"
#include "core/Sfo.h"
#include "core/Upscaler.h"
#include "gfx/Png.h"
#include "gfx/Renderer.h"
#include "platform/host/HostPlatform.h"
#include "tests/Fixtures.h"
#include "ui/UiApp.h"

using namespace ps5ui;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    ++g_checks;                                                                  \
    if (!(cond)) {                                                               \
      ++g_fail;                                                                  \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);              \
    }                                                                            \
  } while (0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= (eps))

static std::string g_pngDir, g_pkgDir;
static std::string tmpDir(const char* name) {
  std::string d = std::string("build/host/tmp_") + name;
  std::string cmd = "rm -rf " + d;
  if (std::system(cmd.c_str()) != 0) {}
  fs::makeDirs(d);
  return d;
}

static void testSettings() {
  std::printf("settings\n");
  Settings s;
  s.renderScalePercent = 70;  // not an allowed value -> snaps
  s.targetFps = 45;
  s.upscaler = "nearest";
  s.fpsOverlay = FpsOverlay::Detailed;
  s.lowGraphics = true;
  s.sanitize();
  CHECK(s.renderScalePercent == 67);
  CHECK(s.targetFps == 60);
  Settings t;
  t.loadFromText(s.toText());
  CHECK(t.renderScalePercent == 67 && t.lowGraphics && t.upscaler == "nearest" && t.fpsOverlay == FpsOverlay::Detailed);
  t.loadFromText("garbage line\nrenderScalePercent=abc\nfpsOverlay=99\n");
  CHECK(t.renderScalePercent == 50);  // atoi("abc")=0 -> nearest allowed
  CHECK(t.fpsOverlay == FpsOverlay::Off);

  Settings d;  // defaults
  EffectiveConfig c = resolve(d);
  CHECK(c.renderScalePercent == 100 && !c.adaptive && c.animatedBackground && c.glowEffects);
  Settings low;
  low.lowGraphics = true;
  low.renderScalePercent = 100;
  c = resolve(low);
  CHECK(c.renderScalePercent == 67 && !c.animatedBackground && !c.glowEffects && !c.staticGlowBackground);
  CHECK(c.upscaler == "nearest");
  Settings perf;
  perf.performanceMode = true;
  perf.targetFps = 30;
  c = resolve(perf);
  CHECK(c.targetFps == 60 && c.adaptive && !c.animatedBackground);
  CHECK(c.maxScalePercent == 100 && c.minScalePercent == 50);
  Settings both;
  both.lowGraphics = both.performanceMode = true;
  c = resolve(both);
  CHECK(c.adaptive && c.maxScalePercent <= 67 && c.renderScalePercent <= 67);
}

static void testFps() {
  std::printf("fps counter\n");
  FpsCounter f;
  CHECK(!f.stats().valid);
  uint64_t t = 5000000;
  for (int i = 0; i < 4; ++i) { f.onFramePresented(t); t += 16667; }
  CHECK(!f.stats().valid);  // not enough real samples yet: never invent a number
  for (int i = 0; i < 120; ++i) { f.onFramePresented(t); t += 16667; }
  FpsStats s = f.stats();
  CHECK(s.valid);
  CHECK_NEAR(s.fps, 60.0, 0.5);
  CHECK_NEAR(s.avgFrameMs, 16.667, 0.05);
  CHECK_NEAR(s.minFrameMs, 16.667, 0.05);
  CHECK_NEAR(s.maxFrameMs, 16.667, 0.05);

  FpsCounter g;
  t = 1000;
  for (int i = 0; i < 100; ++i) { g.onFramePresented(t); t += 33333; }
  CHECK_NEAR(g.stats().fps, 30.0, 0.3);

  FpsCounter h;  // one slow frame shows up in max / p99
  t = 1000;
  for (int i = 0; i < 100; ++i) { h.onFramePresented(t); t += (i == 90) ? 80000 : 16667; }
  s = h.stats();
  CHECK(s.valid && s.maxFrameMs > 79 && s.minFrameMs < 17 && s.fps < 60);

  FpsCounter k;  // clock going backwards must not crash or produce garbage
  for (int i = 0; i < 20; ++i) k.onFramePresented(1000000 + i * 16667);
  k.onFramePresented(10);
  for (int i = 0; i < 20; ++i) k.onFramePresented(100 + i * 16667);
  CHECK(k.stats().valid && std::isfinite(k.stats().fps));
}

static void testAdaptive() {
  std::printf("adaptive resolution\n");
  AdaptiveResolution a;
  a.configure(false, 50, 100, 75, 16.67);
  CHECK(a.percent() == 75);
  for (int i = 0; i < 500; ++i) CHECK(!a.onFrame(100.0));
  CHECK(a.percent() == 75);

  a.configure(true, 50, 100, 100, 16.67);
  CHECK(a.percent() == 100);
  bool changed = false;
  for (int i = 0; i < 40; ++i) changed |= a.onFrame(30.0);  // way over budget
  CHECK(changed && a.percent() < 100);
  for (int i = 0; i < 3000; ++i) a.onFrame(60.0);
  CHECK(a.percent() == 50);  // bottoms out at the minimum
  for (int i = 0; i < 4000; ++i) a.onFrame(1.0);  // lots of headroom
  CHECK(a.percent() == 100);

  a.configure(true, 50, 67, 67, 16.67);  // low-graphics style cap
  for (int i = 0; i < 4000; ++i) a.onFrame(0.5);
  CHECK(a.percent() == 67);
}

static void testUpscalers() {
  std::printf("upscalers\n");
  UpscalerRegistry reg;
  registerBuiltinUpscalers(reg);
  CHECK(reg.count() >= 2 && reg.find("nearest") && reg.find("bilinear") && !reg.find("nope"));
  CHECK(reg.fallback() == reg.find("nearest"));

  uint32_t src[4] = {0xFF000000u, 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu};
  std::vector<uint32_t> dst(16, 0);
  presentScaled({src, 2, 2, 2}, {dst.data(), 4, 4, 4}, reg, "nearest");
  CHECK(dst[0] == src[0] && dst[1] == src[0] && dst[2] == src[1] && dst[3] == src[1]);
  CHECK(dst[12] == src[2] && dst[15] == src[3] && dst[5] == src[0] && dst[10] == src[3]);

  std::vector<uint32_t> flat(9 * 9, 0xFF336699u), big(27 * 27, 0);
  presentScaled({flat.data(), 9, 9, 9}, {big.data(), 27, 27, 27}, reg, "bilinear");
  bool allSame = true;
  for (uint32_t p : big) allSame &= (p == 0xFF336699u);
  CHECK(allSame);  // interpolating a flat image must not change any value

  // horizontal ramp stays monotonic
  std::vector<uint32_t> ramp(8), up(32);
  for (int i = 0; i < 8; ++i) ramp[static_cast<size_t>(i)] = 0xFF000000u | (static_cast<uint32_t>(i * 36) << 16);
  presentScaled({ramp.data(), 8, 1, 8}, {up.data(), 32, 1, 32}, reg, "bilinear");
  bool mono = true;
  for (size_t i = 1; i < 32; ++i) mono &= ((up[i] >> 16) & 255) >= ((up[i - 1] >> 16) & 255);
  CHECK(mono && (up[0] & 0xFF0000u) == 0 && ((up[31] >> 16) & 255) == 252);

  // same size = straight copy; unknown id falls back instead of crashing; pitch respected
  std::vector<uint32_t> a(6 * 4), b(8 * 4, 0xDEADBEEFu);
  for (size_t i = 0; i < a.size(); ++i) a[i] = static_cast<uint32_t>(i);
  presentScaled({a.data(), 6, 4, 6}, {b.data(), 6, 4, 8}, reg, "whatever");
  CHECK(b[0] == 0 && b[5] == 5 && b[6] == 0xDEADBEEFu && b[8] == 6 && b[8 * 3 + 5] == 23);
  std::vector<uint32_t> c(12 * 8, 0);
  presentScaled({a.data(), 6, 4, 6}, {c.data(), 12, 8, 12}, reg, "unknown-id");
  CHECK(c[0] == 0 && c[12 * 8 - 1] == 23);
}

static void testSfo() {
  std::printf("sfo\n");
  std::vector<uint8_t> bytes;
  CHECK(fs::readFile(g_pkgDir + "/param.sfo", bytes, 1 << 20));
  SfoData d;
  CHECK(parseSfo(bytes.data(), bytes.size(), d));
  std::string s;
  uint32_t v = 99;
  CHECK(d.getString("TITLE", s) && s == "PS5 Emulator UI for PS4");
  CHECK(d.getString("TITLE_ID", s) && s == "PSUI00001");
  CHECK(d.getString("CATEGORY", s) && s == "gd");
  CHECK(d.getString("CONTENT_ID", s) && s.size() == 36);
  CHECK(d.getInt("APP_TYPE", v) && v == 0);
  CHECK(d.getInt("PARENTAL_LEVEL", v) && v == 1);
  CHECK(!d.getString("NOPE", s));

  // truncation / corruption must never crash or read out of bounds
  for (size_t n = 0; n < bytes.size(); n += 7) {
    SfoData t;
    parseSfo(bytes.data(), n, t);
  }
  for (size_t i = 0; i < bytes.size(); i += 3) {
    std::vector<uint8_t> m = bytes;
    m[i] ^= 0xFF;
    SfoData t;
    parseSfo(m.data(), m.size(), t);
  }
  uint8_t bad[24] = {0, 'P', 'S', 'F'};
  bad[16] = 0xFF;  // absurd entry count
  SfoData t;
  CHECK(!parseSfo(bad, sizeof(bad), t));
  CHECK(!parseSfo(nullptr, 0, t));
}

static void testPng() {
  std::printf("png\n");
  int files = 0;
  for (const std::string& n : fs::listDirectory(g_pngDir)) {
    if (n.size() < 5 || n.substr(n.size() - 4) != ".png") continue;
    std::vector<uint8_t> png, raw;
    CHECK(fs::readFile(g_pngDir + "/" + n, png, 8u << 20));
    CHECK(fs::readFile(g_pngDir + "/" + n + ".rgba", raw, 8u << 20));
    Image img;
    bool ok = decodePng(png.data(), png.size(), img);
    CHECK(ok);
    if (!ok) { std::printf("   (%s)\n", n.c_str()); continue; }
    int w = raw[0] | (raw[1] << 8), h = raw[4] | (raw[5] << 8);
    CHECK(img.w == w && img.h == h);
    bool same = true;
    for (int i = 0; i < w * h && same; ++i) {
      const uint8_t* p = &raw[8 + static_cast<size_t>(i) * 4];
      uint32_t exp = argb(p[3], p[0], p[1], p[2]);
      same = img.px[static_cast<size_t>(i)] == exp;
    }
    CHECK(same);
    if (!same) std::printf("   mismatch in %s\n", n.c_str());
    ++files;
  }
  CHECK(files >= 27);
  // the shipped app icon decodes
  std::vector<uint8_t> icon;
  Image img;
  CHECK(fs::readFile(g_pkgDir + "/icon0.png", icon, 8u << 20) && decodePng(icon.data(), icon.size(), img) && img.w == 512 && img.h == 512);
  Image small = resizeBox(img, 64, 64);
  CHECK(small.valid() && small.w == 64);
  // garbage / truncated PNG must fail cleanly
  for (size_t n : {size_t(0), size_t(7), size_t(40), icon.size() / 2, icon.size() - 600}) {
    Image t;
    CHECK(!decodePng(icon.data(), n, t) || n == icon.size());
  }
  Image t;
  std::vector<uint8_t> junk(500, 0x41);
  CHECK(!decodePng(junk.data(), junk.size(), t));
}

static void testLibrary() {
  std::printf("library\n");
  std::string root = tmpDir("lib") + "/apps", root2 = tmpDir("lib2") + "/apps";
  CHECK(fixtures::makeGame(root, "CUSA00010", "zeta Quest"));
  CHECK(fixtures::makeGame(root, "CUSA00011", "Alpha Blaster", "gd", "01.05", 0x07500000));
  CHECK(fixtures::makeGame(root, "CUSA00012", "Middle Earth"));
  CHECK(fixtures::makeGame(root, "CUSA00013", "A Patch", "gp"));
  CHECK(fixtures::makeGame(root, "CUSA00014", "Some DLC", "ac"));
  CHECK(fixtures::makeGame(root2, "CUSA00010", "Duplicate of zeta"));
  fs::makeDirs(root + "/NOTATITLE/sce_sys");
  fs::makeDirs(root + "/CUSA00015/sce_sys");  // no param.sfo
  fs::writeTextFile(root + "/CUSA00016", "a file, not a dir");

  Library lib;
  lib.setScanRoots({root, root2, "/definitely/not/here"});
  CHECK(lib.scan() == 3);
  auto v = lib.view(LibrarySort::TitleAZ, LibraryFilter::All, false);
  CHECK(v.size() == 3 && lib.all()[static_cast<size_t>(v[0])].title == "Alpha Blaster" && lib.all()[static_cast<size_t>(v[2])].title == "zeta Quest");
  v = lib.view(LibrarySort::TitleZA, LibraryFilter::All, false);
  CHECK(lib.all()[static_cast<size_t>(v[0])].title == "zeta Quest");
  v = lib.view(LibrarySort::TitleId, LibraryFilter::All, false);
  CHECK(lib.all()[static_cast<size_t>(v[0])].titleId == "CUSA00010");
  CHECK(lib.all()[static_cast<size_t>(lib.indexOf("CUSA00010"))].title == "zeta Quest");  // first root wins
  CHECK(lib.all()[static_cast<size_t>(lib.indexOf("CUSA00011"))].minFirmware == 0x07500000);
  CHECK(lib.all()[static_cast<size_t>(lib.indexOf("CUSA00011"))].version == "01.05");

  CHECK(lib.toggleFavorite("CUSA00012"));
  lib.recordLaunch("CUSA00011", 1000);
  lib.recordLaunch("CUSA00011", 2000);
  lib.recordLaunch("CUSA00010", 3000);
  CHECK(lib.toggleHidden("CUSA00010"));
  v = lib.view(LibrarySort::TitleAZ, LibraryFilter::All, false);
  CHECK(v.size() == 2);
  v = lib.view(LibrarySort::TitleAZ, LibraryFilter::All, true);
  CHECK(v.size() == 3);
  v = lib.view(LibrarySort::TitleAZ, LibraryFilter::Favorites, true);
  CHECK(v.size() == 1 && lib.all()[static_cast<size_t>(v[0])].titleId == "CUSA00012");
  v = lib.view(LibrarySort::MostLaunched, LibraryFilter::All, true);
  CHECK(lib.all()[static_cast<size_t>(v[0])].titleId == "CUSA00011");
  v = lib.view(LibrarySort::RecentlyLaunched, LibraryFilter::All, true);
  CHECK(lib.all()[static_cast<size_t>(v[0])].titleId == "CUSA00010");

  std::string ud = root + "/../library.cfg";
  CHECK(lib.saveUserData(ud));
  Library lib2;
  lib2.setScanRoots({root});
  CHECK(lib2.loadUserData(ud));
  lib2.scan();
  const GameEntry& g = lib2.all()[static_cast<size_t>(lib2.indexOf("CUSA00012"))];
  CHECK(g.favorite);
  const GameEntry& z = lib2.all()[static_cast<size_t>(lib2.indexOf("CUSA00010"))];
  CHECK(z.hidden && z.launchCount == 1 && z.lastLaunch == 3000);
  CHECK(lib2.all()[static_cast<size_t>(lib2.indexOf("CUSA00011"))].launchCount == 2);
  CHECK(!lib2.toggleFavorite("NOPE00000"));
}

static void testRenderer() {
  std::printf("renderer\n");
  Surface s;
  s.allocate(192, 108);
  Renderer r;
  r.begin(s, 0.1f);
  r.clear(0xFF000000u);
  r.fillRoundRect({100, 100, 400, 300}, 60, rgb(255, 255, 255));
  uint32_t center = s.data()[static_cast<size_t>(25) * 192 + 30];
  uint32_t corner = s.data()[static_cast<size_t>(10) * 192 + 10];
  CHECK((center & 255) == 255);          // inside: solid
  CHECK((corner & 255) < 255);           // rounded corner: not fully covered
  r.fillRect({-500, -500, 5000, 5000}, argb(128, 255, 0, 0));  // clipping far outside
  CHECK(((s.data()[0] >> 16) & 255) > 100);
  // out-of-range shapes/text/images must not write outside the surface (ASan would catch it)
  r.drawText(-300, -50, "Clipped text", FontId::Title, rgb(255, 255, 255));
  r.drawText(1800, 1060, "Clipped text", FontId::Body, rgb(255, 255, 255));
  r.glowRing({-100, -100, 5000, 5000}, 20, argb(200, 0, 255, 0), 6, 6);
  r.fillCircle(-1000, 50, 30, rgb(1, 2, 3));
  r.drawRadialGlow(1900, 1070, 800, rgb(0, 0, 255), 1.0f);
  Image img;
  img.w = img.h = 4;
  img.px.assign(16, 0xFF123456u);
  r.drawImage(img, {1800, 1000, 400, 400}, 30, true);
  r.drawImage(img, {-100, -100, 300, 300}, 30, false);
  CHECK(Renderer::measureText("Hello", FontId::Body) > 40);
  CHECK(Renderer::measureText("Hello", FontId::Title) > Renderer::measureText("Hello", FontId::Small));
  CHECK(Renderer::fitText("A very long title that cannot fit", FontId::Body, 120).size() < 33);
  CHECK(Renderer::fitText("Hi", FontId::Body, 500) == "Hi");
  r.drawText(0, 0, std::string("non-ascii \xC3\xA9\x01"), FontId::Body, rgb(255, 255, 255));  // falls back to '?'
}

static uint64_t hashSurface(const Surface& s) {
  uint64_t h = 1469598103934665603ull;
  for (int y = 0; y < s.height(); y += 3)
    for (int x = 0; x < s.width(); x += 3) {
      h ^= s.data()[static_cast<size_t>(y) * static_cast<size_t>(s.pitch()) + static_cast<size_t>(x)];
      h *= 1099511628211ull;
    }
  return h;
}

static void testUi() {
  std::printf("ui (headless host platform, simulated clock)\n");
  std::string data = tmpDir("ui");
  std::string apps = data + "/apps";
  for (int i = 0; i < 14; ++i)
    fixtures::makeGame(apps, "CUSA001" + std::to_string(i / 10) + std::to_string(i % 10), "Game number " + std::to_string(i), "gd", "01.00", 0x05050000, i % 2 ? g_pkgDir + "/icon0.png" : "");
  fs::writeTextFile(data + "/library_paths.txt", apps + "\n");

  HostPlatform hp(data);
  hp.useSimulatedClock(true);
  UiApp app(hp);
  CHECK(app.init());
  CHECK(app.library().all().size() == 14);

  auto step = [&](uint32_t b, int n) {
    PadState p;
    p.connected = true;
    p.buttons = b;
    hp.setPad(p);
    for (int i = 0; i < n; ++i) CHECK(app.frame());
  };
  auto press = [&](uint32_t b) { step(b, 1); step(0, 2); };

  step(0, 20);
  uint64_t homeHash = hashSurface(app.frameSurface());
  for (int i = 0; i < 5; ++i) press(pad::kRight);
  step(0, 30);
  CHECK(hashSurface(app.frameSurface()) != homeHash);  // carousel really moved

  press(pad::kCross);  // details modal
  CHECK(app.modal() == Modal::GameDetails);
  press(pad::kDown); press(pad::kCross);  // favourite
  step(0, 3);
  press(pad::kCircle);
  CHECK(app.modal() == Modal::None);
  int favs = 0;
  for (const auto& g : app.library().all()) favs += g.favorite;
  CHECK(favs == 1);

  // launching is disabled on host: must toast, not crash
  press(pad::kCross); press(pad::kCross); step(0, 3); press(pad::kCircle);

  press(pad::kR1);
  CHECK(app.tab() == Tab::Library);
  for (int i = 0; i < 9; ++i) press(pad::kDown);
  press(pad::kTriangle); press(pad::kL2); press(pad::kR2); press(pad::kSquare);
  step(0, 10);
  press(pad::kUp); press(pad::kUp); press(pad::kUp); press(pad::kUp);  // reach the tab bar
  press(pad::kRight);
  CHECK(app.tab() == Tab::Performance);
  press(pad::kDown);

  // Every mode/scale/upscaler combination: no crashes (ASan/UBSan) and the internal resolution is exactly right.
  int presets = 0;
  for (int low = 0; low < 2; ++low)
    for (int perf = 0; perf < 2; ++perf)
      for (int scale : {50, 67, 75, 100})
        for (const char* up : {"nearest", "bilinear"}) {
          app.settings().lowGraphics = low;
          app.settings().performanceMode = perf;
          app.settings().renderScalePercent = scale;
          app.settings().upscaler = up;
          app.settings().fpsOverlay = FpsOverlay::Detailed;
          app.applySettings();
          step(0, 4);
          int expect = low ? std::min(scale, 67) : scale;
          CHECK(app.renderPercent() == expect);
          int expW = static_cast<int>(std::lround(1920.0 * expect / 100.0)) & ~1;
          CHECK(app.frameSurface().width() == expW);
          CHECK(app.config().targetFps == (perf ? 60 : app.settings().targetFps));
          CHECK(app.config().glowEffects == !low);
          ++presets;
        }
  CHECK(presets == 32);

  // FPS plumbing with the simulated clock: exactly 60 presented frames per simulated second.
  app.settings().fpsOverlay = FpsOverlay::Simple;
  press(pad::kRight);
  step(0, 200);
  FpsStats fs = app.fpsStats();
  CHECK(fs.valid);
  CHECK_NEAR(fs.fps, 60.0, 1.0);

  press(pad::kR1);
  CHECK(app.tab() == Tab::Settings);
  press(pad::kCross);  // rescan
  press(pad::kDown); press(pad::kCross);  // show hidden toggle
  press(pad::kDown); press(pad::kCross);  // About modal
  CHECK(app.modal() == Modal::About);
  step(0, 3);
  press(pad::kCircle);
  press(pad::kDown);
  {
    PadState p;
    p.connected = true;
    p.buttons = pad::kCross;  // Exit Application
    hp.setPad(p);
    CHECK(!app.frame());      // exit requested -> frame() returns false
  }
  app.shutdown();

  // settings and library data were persisted
  Settings loaded;
  CHECK(loaded.load(data + "/settings.cfg"));
  std::string lib;
  CHECK(fs::readTextFile(data + "/library.cfg", lib) && lib.find("|1|") != std::string::npos);

  // An empty library renders the empty state (no crash), also at low scale.
  std::string data2 = tmpDir("ui_empty");
  fs::writeTextFile(data2 + "/library_paths.txt", data2 + "/nothing\n");
  HostPlatform hp2(data2);
  hp2.useSimulatedClock(true);
  UiApp app2(hp2);
  CHECK(app2.init());
  app2.settings().lowGraphics = true;
  PadState p;
  p.connected = true;
  hp2.setPad(p);
  for (int i = 0; i < 20; ++i) app2.frame();
  p.buttons = pad::kCross;
  hp2.setPad(p);
  app2.frame();
  p.buttons = pad::kR1;
  hp2.setPad(p);
  for (int i = 0; i < 4; ++i) { app2.frame(); p.buttons = 0; hp2.setPad(p); app2.frame(); p.buttons = pad::kR1; hp2.setPad(p); }
  app2.shutdown();
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::printf("usage: tests <png-fixture-dir> <pkg-sce_sys-dir>\n");
    return 2;
  }
  g_pngDir = argv[1];
  g_pkgDir = argv[2];
  testSettings();
  testFps();
  testAdaptive();
  testUpscalers();
  testSfo();
  testPng();
  testLibrary();
  testRenderer();
  testUi();
  std::printf("\n%d checks, %d failed\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
