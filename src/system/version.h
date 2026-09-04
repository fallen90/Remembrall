#pragma once

#include <string>

// Populated by CMake via compile definitions.
#ifndef LTA_VERSION_MAJOR
#define LTA_VERSION_MAJOR 1
#endif
#ifndef LTA_VERSION_MINOR
#define LTA_VERSION_MINOR 0
#endif
#ifndef LTA_VERSION_PATCH
#define LTA_VERSION_PATCH 0
#endif

#ifndef LTA_VERSION_STRING
#define LTA_VERSION_STRING "1.0.0"
#endif

#ifndef LTA_GITHUB_REPO
#define LTA_GITHUB_REPO "fallen90/Remembrall"
#endif

namespace lta {

struct SemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;

  [[nodiscard]] static SemVer Current();
  [[nodiscard]] static SemVer Parse(const std::string& text);  // accepts "v1.2.3" or "1.2.3"
  [[nodiscard]] bool IsNewerThan(const SemVer& other) const;
  [[nodiscard]] std::string ToString() const;
};

const char* CurrentVersionString();
const char* DefaultGithubRepo();

}  // namespace lta
