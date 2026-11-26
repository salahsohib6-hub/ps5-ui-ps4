// Entry point of the PS4 homebrew application.
#include "platform/Platform.h"
#include "ui/UiApp.h"

int main(int /*argc*/, char** /*argv*/) {
  std::unique_ptr<ps5ui::IPlatform> platform = ps5ui::createPlatform();
  ps5ui::UiApp app(*platform);
  if (!app.init()) return 1;
  app.run();
  app.shutdown();
  return 0;
}
