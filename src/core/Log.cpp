#include "core/Log.h"

#include <cstdarg>
#include <cstdio>

namespace ps5ui {
namespace log {
namespace {
FILE* g_file = nullptr;
}

void open(const char* path) {
  close();
  g_file = std::fopen(path, "w");
}

void close() {
  if (g_file) {
    std::fclose(g_file);
    g_file = nullptr;
  }
}

void write(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  std::printf("[ps5ui] %s\n", buf);
  if (g_file) {
    std::fprintf(g_file, "%s\n", buf);
    std::fflush(g_file);
  }
}

}  // namespace log
}  // namespace ps5ui
