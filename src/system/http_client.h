#pragma once

#include "common/branding.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lta {

struct HttpResponse {
  int status_code = 0;
  std::string body;
  std::wstring error;
};

class HttpClient {
 public:
  static HttpResponse Get(const std::wstring& url,
                          const std::wstring& user_agent = branding::kUpdaterUserAgent);

  static bool DownloadToFile(const std::wstring& url,
                             const std::filesystem::path& destination,
                             const std::wstring& user_agent = branding::kUpdaterUserAgent);
};

}  // namespace lta
