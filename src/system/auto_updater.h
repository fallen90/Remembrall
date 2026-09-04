#pragma once

#include "system/version.h"

#include "common/branding.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace lta {

enum class UpdateState {
  Idle,
  Checking,
  UpToDate,
  Available,
  Downloading,
  ReadyToApply,
  Applying,
  Failed,
};

struct UpdateInfo {
  SemVer remote_version{};
  std::string tag_name;
  std::string release_notes;
  std::wstring download_url;
  std::wstring asset_name;
};

class AutoUpdater {
 public:
  using StateCallback = std::function<void(UpdateState state, const UpdateInfo& info, const std::string& message)>;

  AutoUpdater();
  ~AutoUpdater();

  void SetGithubRepo(std::string owner_slash_repo);  // e.g. "fallen90/Remembrall"
  void SetEnabled(bool enabled);
  void SetStateCallback(StateCallback cb);

  // Background check against GitHub Releases /latest.
  void CheckForUpdatesAsync(bool interactive_prompt);

  // Download staged package if Available.
  void DownloadAndStageAsync();

  // Launch apply script and request app exit. Returns false if nothing staged.
  bool ApplyAndRelaunch();

  [[nodiscard]] UpdateState State() const { return state_.load(); }
  [[nodiscard]] UpdateInfo Info() const;
  [[nodiscard]] std::string LastMessage() const;

  // Preferred release asset name prefix.
  static constexpr const wchar_t* kAssetPrefix = branding::kReleaseAssetPrefixW;

 private:
  bool CheckForUpdates(UpdateInfo& out, std::string& error);
  bool DownloadAndStage(const UpdateInfo& info, std::string& error);
  bool WriteApplyScript(const std::filesystem::path& staged_dir,
                        const std::filesystem::path& install_dir,
                        uint32_t current_pid);

  void SetState(UpdateState state, const std::string& message = {});
  void Notify();

  std::string repo_;
  std::atomic<bool> enabled_{true};
  std::atomic<UpdateState> state_{UpdateState::Idle};
  mutable std::mutex mutex_;
  UpdateInfo info_{};
  std::string last_message_;
  StateCallback callback_;
  std::thread worker_;
  std::atomic<bool> stop_{false};
  bool interactive_ = false;
  std::filesystem::path staged_package_;
};

}  // namespace lta
