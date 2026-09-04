#include "system/auto_updater.h"

#include "common/branding.h"
#include "system/http_client.h"
#include "system/logger.h"

#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shellapi.h>
#include <ShlObj.h>

namespace lta {
namespace {

std::filesystem::path UpdateCacheDir() {
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
    std::filesystem::path root(path);
    CoTaskMemFree(path);
    root /= branding::kAppDataFolder;
    root /= L"updates";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root;
  }
  return std::filesystem::temp_directory_path() / L"RemembrallUpdates";
}

std::filesystem::path InstallDirectory() {
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  return std::filesystem::path(module).parent_path();
}

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

std::string ExtractJsonString(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return {};
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return {};
  }
  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) {
    return {};
  }
  ++pos;
  std::string out;
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
  return out;
}

// Find first asset whose name starts with prefix and ends with .zip
bool FindZipAsset(const std::string& json,
                  const std::string& prefix_utf8,
                  std::string& name_out,
                  std::string& url_out) {
  const std::string assets_key = "\"assets\"";
  auto assets_pos = json.find(assets_key);
  if (assets_pos == std::string::npos) {
    return false;
  }
  assets_pos = json.find('[', assets_pos);
  if (assets_pos == std::string::npos) {
    return false;
  }

  size_t pos = assets_pos;
  while (true) {
    const auto name_key = json.find("\"name\"", pos);
    if (name_key == std::string::npos) {
      break;
    }
    // Bound search to assets array roughly by looking ahead.
    const auto name = ExtractJsonString(json.substr(name_key), "name");
    const auto url = ExtractJsonString(json.substr(name_key), "browser_download_url");
    if (name.size() >= prefix_utf8.size() &&
        name.compare(0, prefix_utf8.size(), prefix_utf8) == 0 &&
        name.size() >= 4 &&
        name.compare(name.size() - 4, 4, ".zip") == 0 &&
        !url.empty()) {
      name_out = name;
      url_out = url;
      return true;
    }
    pos = name_key + 6;
  }
  return false;
}

}  // namespace

AutoUpdater::AutoUpdater() : repo_(DefaultGithubRepo()) {}

AutoUpdater::~AutoUpdater() {
  stop_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
}

void AutoUpdater::SetGithubRepo(std::string owner_slash_repo) {
  std::lock_guard lock(mutex_);
  if (!owner_slash_repo.empty()) {
    repo_ = std::move(owner_slash_repo);
  }
}

void AutoUpdater::SetEnabled(bool enabled) {
  enabled_.store(enabled);
}

void AutoUpdater::SetStateCallback(StateCallback cb) {
  std::lock_guard lock(mutex_);
  callback_ = std::move(cb);
}

UpdateInfo AutoUpdater::Info() const {
  std::lock_guard lock(mutex_);
  return info_;
}

std::string AutoUpdater::LastMessage() const {
  std::lock_guard lock(mutex_);
  return last_message_;
}

void AutoUpdater::SetState(UpdateState state, const std::string& message) {
  state_.store(state);
  {
    std::lock_guard lock(mutex_);
    if (!message.empty()) {
      last_message_ = message;
    }
  }
  Notify();
}

void AutoUpdater::Notify() {
  StateCallback cb;
  UpdateInfo info;
  UpdateState state;
  std::string message;
  {
    std::lock_guard lock(mutex_);
    cb = callback_;
    info = info_;
    message = last_message_;
  }
  state = state_.load();
  if (cb) {
    cb(state, info, message);
  }
}

void AutoUpdater::CheckForUpdatesAsync(bool interactive_prompt) {
  if (!enabled_.load()) {
    SetState(UpdateState::Idle, "Auto-update disabled");
    return;
  }
  if (worker_.joinable()) {
    worker_.join();
  }
  interactive_ = interactive_prompt;
  stop_.store(false);
  worker_ = std::thread([this]() {
    SetState(UpdateState::Checking, "Checking GitHub Releases…");
    UpdateInfo info;
    std::string error;
    if (!CheckForUpdates(info, error)) {
      SetState(UpdateState::Failed, error.empty() ? "Update check failed" : error);
      return;
    }
    {
      std::lock_guard lock(mutex_);
      info_ = info;
    }
    if (!info.remote_version.IsNewerThan(SemVer::Current())) {
      SetState(UpdateState::UpToDate, "Already on latest (" + SemVer::Current().ToString() + ")");
      return;
    }
    SetState(UpdateState::Available,
             "Update " + info.remote_version.ToString() + " available");
    (void)interactive_;
  });
}

void AutoUpdater::DownloadAndStageAsync() {
  if (worker_.joinable()) {
    // Don't join if checking — queue on same thread only when idle/available.
    if (state_.load() == UpdateState::Checking || state_.load() == UpdateState::Downloading) {
      return;
    }
    worker_.join();
  }
  worker_ = std::thread([this]() {
    UpdateInfo info;
    {
      std::lock_guard lock(mutex_);
      info = info_;
    }
    if (info.download_url.empty()) {
      SetState(UpdateState::Failed, "No download URL");
      return;
    }
    SetState(UpdateState::Downloading, "Downloading update…");
    std::string error;
    if (!DownloadAndStage(info, error)) {
      SetState(UpdateState::Failed, error.empty() ? "Download failed" : error);
    }
  });
}

bool AutoUpdater::CheckForUpdates(UpdateInfo& out, std::string& error) {
  std::string repo;
  {
    std::lock_guard lock(mutex_);
    repo = repo_;
  }
  if (repo.empty() || repo.find('/') == std::string::npos) {
    error = "GitHub repo not configured (owner/name)";
    return false;
  }

  const std::wstring url =
      L"https://api.github.com/repos/" + Utf8ToWide(repo) + L"/releases/latest";
  LTA_LOG_INFO("Checking updates: " + repo);

  auto response = HttpClient::Get(url);
  if (response.status_code == 404) {
    error = "No releases published yet";
    return false;
  }
  if (response.status_code != 200) {
    error = "GitHub API HTTP " + std::to_string(response.status_code);
    return false;
  }

  const auto tag = ExtractJsonString(response.body, "tag_name");
  if (tag.empty()) {
    error = "Release tag_name missing";
    return false;
  }
  out.tag_name = tag;
  out.remote_version = SemVer::Parse(tag);
  out.release_notes = ExtractJsonString(response.body, "body");

  std::string asset_name;
  std::string asset_url;
  if (!FindZipAsset(response.body, branding::kReleaseAssetPrefix, asset_name, asset_url)) {
    error = std::string("Release has no ") + branding::kReleaseAssetPrefix + "*.zip asset";
    return false;
  }
  out.asset_name = Utf8ToWide(asset_name);
  out.download_url = Utf8ToWide(asset_url);
  return true;
}

bool AutoUpdater::DownloadAndStage(const UpdateInfo& info, std::string& error) {
  SetState(UpdateState::Downloading, "Downloading " + std::string(info.asset_name.begin(),
                                                                   info.asset_name.end()));

  const auto cache = UpdateCacheDir();
  const auto zip_path = cache / info.asset_name;
  const auto staged = cache / L"pending";

  std::error_code ec;
  std::filesystem::remove_all(staged, ec);
  std::filesystem::create_directories(staged, ec);

  if (!HttpClient::DownloadToFile(info.download_url, zip_path)) {
    error = "Failed to download release asset";
    return false;
  }

  // Expand with PowerShell (built into Windows 10+).
  std::wostringstream cmd;
  cmd << L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
      << L"\"Expand-Archive -LiteralPath '" << zip_path.wstring()
      << L"' -DestinationPath '" << staged.wstring() << L"' -Force\"";

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring cmdline = cmd.str();
  if (!CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    error = "Failed to extract update archive";
    return false;
  }
  WaitForSingleObject(pi.hProcess, 120000);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (code != 0) {
    error = "Expand-Archive failed";
    return false;
  }

  // Staged layout may be flat or nested one folder.
  auto exe = staged / branding::kExeFileName;
  if (!std::filesystem::exists(exe)) {
    for (const auto& entry : std::filesystem::directory_iterator(staged)) {
      if (entry.is_directory()) {
        const auto nested = entry.path() / branding::kExeFileName;
        if (std::filesystem::exists(nested)) {
          // Flatten: use nested as staged root marker via junction copy — store nested path.
          {
            std::lock_guard lock(mutex_);
            staged_package_ = entry.path();
          }
          if (!WriteApplyScript(entry.path(), InstallDirectory(), GetCurrentProcessId())) {
            error = "Failed to write apply script";
            return false;
          }
          SetState(UpdateState::ReadyToApply, "Update ready — restart to apply");
          return true;
        }
      }
    }
    error = std::string(branding::kExeFileNameUtf8) + " missing from package";
    return false;
  }

  {
    std::lock_guard lock(mutex_);
    staged_package_ = staged;
  }
  if (!WriteApplyScript(staged, InstallDirectory(), GetCurrentProcessId())) {
    error = "Failed to write apply script";
    return false;
  }
  SetState(UpdateState::ReadyToApply, "Update ready — restart to apply");
  return true;
}

bool AutoUpdater::WriteApplyScript(const std::filesystem::path& staged_dir,
                                   const std::filesystem::path& install_dir,
                                   uint32_t current_pid) {
  const auto script = UpdateCacheDir() / L"apply-update.cmd";
  std::ofstream out(script);
  if (!out) {
    return false;
  }
  // Wait for app exit, copy files, relaunch.
  out << "@echo off\r\n"
      << "setlocal\r\n"
      << "echo Applying Remembrall update...\r\n"
      << ":wait\r\n"
      << "tasklist /FI \"PID eq " << current_pid << "\" | find \"" << current_pid << "\" >nul\r\n"
      << "if not errorlevel 1 (\r\n"
      << "  timeout /t 1 /nobreak >nul\r\n"
      << "  goto wait\r\n"
      << ")\r\n"
      << "timeout /t 1 /nobreak >nul\r\n"
      << "xcopy /E /Y /I /Q \"" << staged_dir.string() << "\\*\" \"" << install_dir.string()
      << "\\\"\r\n"
      << "start \"\" \"" << (install_dir / branding::kExeFileNameUtf8).string() << "\"\r\n"
      << "endlocal\r\n";
  return true;
}

bool AutoUpdater::ApplyAndRelaunch() {
  if (state_.load() != UpdateState::ReadyToApply) {
    return false;
  }
  SetState(UpdateState::Applying, "Applying update…");
  const auto script = UpdateCacheDir() / L"apply-update.cmd";
  if (!std::filesystem::exists(script)) {
    SetState(UpdateState::Failed, "Apply script missing");
    return false;
  }

  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"open";
  sei.lpFile = script.c_str();
  sei.nShow = SW_HIDE;
  if (!ShellExecuteExW(&sei)) {
    SetState(UpdateState::Failed, "Failed to start apply script");
    return false;
  }
  if (sei.hProcess) {
    CloseHandle(sei.hProcess);
  }
  // Caller should exit the app so the script can replace binaries.
  return true;
}

}  // namespace lta
