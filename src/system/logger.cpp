#include "system/logger.h"

#include "common/branding.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace lta::log {
namespace {

std::mutex g_mutex;
std::ofstream g_file;
Level g_level = Level::Info;
bool g_initialized = false;

const char* LevelName(Level level) {
  switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info:  return "INFO";
    case Level::Warn:  return "WARN";
    case Level::Error: return "ERROR";
  }
  return "?";
}

std::string NowString() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto tt = system_clock::to_time_t(now);
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::tm tm{};
  localtime_s(&tm, &tt);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
      << std::setw(3) << std::setfill('0') << ms.count();
  return oss.str();
}

}  // namespace

void Initialize(const std::wstring& log_directory) {
  std::lock_guard lock(g_mutex);
  if (g_initialized) {
    return;
  }
  CreateDirectoryW(log_directory.c_str(), nullptr);
  const std::wstring path = log_directory + L"\\" + ::lta::branding::kLogFileName;
  g_file.open(path, std::ios::out | std::ios::app);
  g_initialized = true;
}

void Shutdown() {
  std::lock_guard lock(g_mutex);
  if (g_file.is_open()) {
    g_file.flush();
    g_file.close();
  }
  g_initialized = false;
}

void SetLevel(Level level) {
  std::lock_guard lock(g_mutex);
  g_level = level;
}

void Write(Level level, const char* file, int line, const std::string& message) {
  std::lock_guard lock(g_mutex);
  if (static_cast<int>(level) < static_cast<int>(g_level)) {
    return;
  }

  // Never log raw audio. Transcript content logging is off by default (callers decide).
  const char* base = file;
  if (const char* slash = strrchr(file, '\\')) {
    base = slash + 1;
  } else if (const char* slash2 = strrchr(file, '/')) {
    base = slash2 + 1;
  }

  std::ostringstream line_out;
  line_out << NowString() << " [" << LevelName(level) << "] "
           << base << ':' << line << " " << message;

  const auto text = line_out.str();
  OutputDebugStringA((text + "\n").c_str());
  if (g_file.is_open()) {
    g_file << text << '\n';
    g_file.flush();
  }
}

}  // namespace lta::log
