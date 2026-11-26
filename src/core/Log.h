#pragma once

namespace ps5ui {
namespace log {

// Opens (truncates) the log file. Safe to skip; logging then only goes to stdout.
void open(const char* path);
void close();
void write(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace log
}  // namespace ps5ui
