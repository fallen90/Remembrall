#include "system/http_client.h"

#include "system/logger.h"

#include <fstream>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace lta {
namespace {

struct ParsedUrl {
  bool https = true;
  std::wstring host;
  INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
  std::wstring path;
};

bool ParseUrl(const std::wstring& url, ParsedUrl& out) {
  URL_COMPONENTS uc{};
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256]{};
  wchar_t path[4096]{};
  wchar_t extra[2048]{};
  uc.lpszHostName = host;
  uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path;
  uc.dwUrlPathLength = 4096;
  uc.lpszExtraInfo = extra;
  uc.dwExtraInfoLength = 2048;
  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
    return false;
  }
  out.https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
  out.host.assign(host, uc.dwHostNameLength);
  out.port = uc.nPort;
  out.path.assign(path, uc.dwUrlPathLength);
  if (uc.dwExtraInfoLength > 0) {
    out.path.append(extra, uc.dwExtraInfoLength);
  }
  if (out.path.empty()) {
    out.path = L"/";
  }
  return true;
}

struct WinHttpHandles {
  HINTERNET session = nullptr;
  HINTERNET connect = nullptr;
  HINTERNET request = nullptr;
  ~WinHttpHandles() {
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
  }
};

bool OpenGet(const std::wstring& url,
             const std::wstring& user_agent,
             WinHttpHandles& h,
             DWORD& status,
             std::wstring& error) {
  ParsedUrl parsed;
  if (!ParseUrl(url, parsed)) {
    error = L"Invalid URL";
    return false;
  }
  h.session = WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!h.session) {
    error = L"WinHttpOpen failed";
    return false;
  }
  h.connect = WinHttpConnect(h.session, parsed.host.c_str(), parsed.port, 0);
  if (!h.connect) {
    error = L"WinHttpConnect failed";
    return false;
  }
  const DWORD flags = parsed.https ? WINHTTP_FLAG_SECURE : 0;
  h.request = WinHttpOpenRequest(h.connect, L"GET", parsed.path.c_str(), nullptr,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!h.request) {
    error = L"WinHttpOpenRequest failed";
    return false;
  }
  DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(h.request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
  const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
  if (!WinHttpSendRequest(h.request, headers, static_cast<DWORD>(-1),
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(h.request, nullptr)) {
    error = L"HTTP request failed";
    return false;
  }
  status = 0;
  DWORD size = sizeof(status);
  WinHttpQueryHeaders(h.request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
  return true;
}

}  // namespace

HttpResponse HttpClient::Get(const std::wstring& url, const std::wstring& user_agent) {
  HttpResponse response;
  WinHttpHandles h;
  DWORD status = 0;
  if (!OpenGet(url, user_agent, h, status, response.error)) {
    return response;
  }
  response.status_code = static_cast<int>(status);
  DWORD avail = 0;
  while (WinHttpQueryDataAvailable(h.request, &avail) && avail > 0) {
    std::vector<char> buf(avail);
    DWORD read = 0;
    if (!WinHttpReadData(h.request, buf.data(), avail, &read) || read == 0) {
      break;
    }
    response.body.append(buf.data(), read);
  }
  return response;
}

bool HttpClient::DownloadToFile(const std::wstring& url,
                                const std::filesystem::path& destination,
                                const std::wstring& user_agent) {
  WinHttpHandles h;
  DWORD status = 0;
  std::wstring error;
  if (!OpenGet(url, user_agent, h, status, error)) {
    LTA_LOG_ERROR("Download open failed");
    return false;
  }
  if (status != 200) {
    LTA_LOG_ERROR("Download failed status=" + std::to_string(status));
    return false;
  }

  std::error_code ec;
  std::filesystem::create_directories(destination.parent_path(), ec);
  std::ofstream out(destination, std::ios::binary);
  if (!out) {
    return false;
  }

  DWORD avail = 0;
  while (WinHttpQueryDataAvailable(h.request, &avail) && avail > 0) {
    std::vector<char> buf(avail);
    DWORD read = 0;
    if (!WinHttpReadData(h.request, buf.data(), avail, &read) || read == 0) {
      break;
    }
    out.write(buf.data(), static_cast<std::streamsize>(read));
  }
  return static_cast<bool>(out);
}

}  // namespace lta
