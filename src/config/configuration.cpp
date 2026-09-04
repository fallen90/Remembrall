#include "config/configuration.h"

#include "common/branding.h"
#include "system/logger.h"

#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>

namespace lta {
namespace {

std::filesystem::path AppDataRoot() {
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
    std::filesystem::path root(path);
    CoTaskMemFree(path);
    root /= branding::kAppDataFolder;
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root;
  }
  return std::filesystem::current_path();
}

bool ParseBool(const std::string& v) {
  return v == "true" || v == "1";
}

// Minimal JSON-ish parser for our flat settings (no nested objects).
bool ExtractBool(const std::string& json, const char* key, bool& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const auto colon = json.find(':', pos + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  auto start = colon + 1;
  while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) {
    ++start;
  }
  auto end = start;
  while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\n') {
    ++end;
  }
  std::string token = json.substr(start, end - start);
  while (!token.empty() && (token.back() == ' ' || token.back() == '\r')) {
    token.pop_back();
  }
  out = ParseBool(token);
  return true;
}

bool ExtractString(const std::string& json, const char* key, std::string& out) {
  const std::string needle = std::string("\"") + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) {
    return false;
  }
  ++pos;
  out.clear();
  while (pos < json.size()) {
    const char c = json[pos++];
    if (c == '\\' && pos < json.size()) {
      out.push_back(json[pos++]);
      continue;
    }
    if (c == '"') {
      break;
    }
    out.push_back(c);
  }
  return true;
}

}  // namespace

std::filesystem::path DefaultSettingsPath() {
  return AppDataRoot() / L"settings.json";
}

std::filesystem::path DefaultModelDirectory() {
  // Prefer models next to the executable, then AppData.
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  auto exe_dir = std::filesystem::path(module).parent_path();
  auto beside = exe_dir / L"models" / L"zipformer-en";
  if (std::filesystem::exists(beside)) {
    return beside;
  }
  auto appdata = AppDataRoot() / L"models" / L"zipformer-en";
  return appdata;
}

std::filesystem::path DefaultLogDirectory() {
  return AppDataRoot() / L"logs";
}

Configuration Configuration::LoadDefaults() {
  Configuration c;
  c.model_directory = DefaultModelDirectory();
  c.settings_path = DefaultSettingsPath();
  return c;
}

Configuration Configuration::LoadFromFile(const std::filesystem::path& path) {
  Configuration c = LoadDefaults();
  c.settings_path = path;
  std::ifstream in(path);
  if (!in) {
    return c;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  const auto json = ss.str();
  ExtractBool(json, "always_on_top", c.always_on_top);
  ExtractBool(json, "start_minimized", c.start_minimized);
  ExtractBool(json, "launch_with_windows", c.launch_with_windows);
  ExtractBool(json, "show_partial_results", c.show_partial_results);
  ExtractBool(json, "check_for_updates", c.check_for_updates);
  ExtractBool(json, "auto_download_updates", c.auto_download_updates);
  ExtractString(json, "github_repo", c.github_repo);
  return c;
}

bool Configuration::SaveToFile(const std::filesystem::path& path) const {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << "{\n"
      << "  \"always_on_top\": " << (always_on_top ? "true" : "false") << ",\n"
      << "  \"start_minimized\": " << (start_minimized ? "true" : "false") << ",\n"
      << "  \"launch_with_windows\": " << (launch_with_windows ? "true" : "false") << ",\n"
      << "  \"show_partial_results\": " << (show_partial_results ? "true" : "false") << ",\n"
      << "  \"check_for_updates\": " << (check_for_updates ? "true" : "false") << ",\n"
      << "  \"auto_download_updates\": " << (auto_download_updates ? "true" : "false") << ",\n"
      << "  \"github_repo\": \"" << github_repo << "\",\n"
      << "  \"language\": \"en\"\n"
      << "}\n";
  return true;
}

void Configuration::ApplyLaunchWithWindows() const {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                    0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
    LTA_LOG_WARN("Failed to open Run key for launch-with-Windows setting");
    return;
  }

  if (launch_with_windows) {
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    RegSetValueExW(key, branding::kRunKeyValue, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(module),
                   static_cast<DWORD>((wcslen(module) + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, branding::kRunKeyValue);
  }
  RegCloseKey(key);
}

}  // namespace lta
