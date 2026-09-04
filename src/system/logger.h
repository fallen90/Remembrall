#pragma once

#include <string>

namespace lta::log {

enum class Level {
  Debug,
  Info,
  Warn,
  Error,
};

void Initialize(const std::wstring& log_directory);
void Shutdown();
void SetLevel(Level level);
void Write(Level level, const char* file, int line, const std::string& message);

}  // namespace lta::log

#define LTA_LOG_DEBUG(msg) ::lta::log::Write(::lta::log::Level::Debug, __FILE__, __LINE__, (msg))
#define LTA_LOG_INFO(msg)  ::lta::log::Write(::lta::log::Level::Info,  __FILE__, __LINE__, (msg))
#define LTA_LOG_WARN(msg)  ::lta::log::Write(::lta::log::Level::Warn,  __FILE__, __LINE__, (msg))
#define LTA_LOG_ERROR(msg) ::lta::log::Write(::lta::log::Level::Error, __FILE__, __LINE__, (msg))
