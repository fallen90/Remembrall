#include "system/version.h"

#include <cstdio>
#include <sstream>

namespace lta {

SemVer SemVer::Current() {
  return {LTA_VERSION_MAJOR, LTA_VERSION_MINOR, LTA_VERSION_PATCH};
}

SemVer SemVer::Parse(const std::string& text) {
  SemVer v;
  std::string s = text;
  if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
    s.erase(s.begin());
  }
  // Trim prerelease / build metadata.
  const auto cut = s.find_first_of("-+");
  if (cut != std::string::npos) {
    s = s.substr(0, cut);
  }
  std::sscanf(s.c_str(), "%d.%d.%d", &v.major, &v.minor, &v.patch);
  return v;
}

bool SemVer::IsNewerThan(const SemVer& other) const {
  if (major != other.major) {
    return major > other.major;
  }
  if (minor != other.minor) {
    return minor > other.minor;
  }
  return patch > other.patch;
}

std::string SemVer::ToString() const {
  std::ostringstream oss;
  oss << major << '.' << minor << '.' << patch;
  return oss.str();
}

const char* CurrentVersionString() {
  return LTA_VERSION_STRING;
}

const char* DefaultGithubRepo() {
  return LTA_GITHUB_REPO;
}

}  // namespace lta
