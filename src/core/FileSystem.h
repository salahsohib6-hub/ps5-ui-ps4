#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ps5ui {
namespace fs {

bool exists(const std::string& path);
bool isDirectory(const std::string& path);
bool makeDirs(const std::string& path);  // like mkdir -p
bool readFile(const std::string& path, std::vector<uint8_t>& out, size_t maxBytes);
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
std::vector<std::string> listDirectory(const std::string& path);  // names only, no "." / ".."
std::string join(const std::string& a, const std::string& b);

}  // namespace fs
}  // namespace ps5ui
