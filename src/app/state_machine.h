#pragma once

#include "common/types.h"
#include "config/configuration.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace lta {

class StateMachine {
 public:
  using Listener = std::function<void(AppState previous, AppState next)>;

  StateMachine();

  [[nodiscard]] AppState State() const { return state_.load(); }
  void Transition(AppState next);
  void SetListener(Listener listener);

  // High-level recovery helpers used by Application.
  void OnDiscordFound(uint32_t pid);
  void OnDiscordLost();
  void OnCaptureStarted();
  void OnCaptureFailed(const std::string& reason);
  void OnTranscribing();
  void OnDeviceChange();
  void OnRecovering();
  void OnAsrFailed(const std::string& reason);

 private:
  std::atomic<AppState> state_{AppState::Starting};
  std::mutex listener_mutex_;
  Listener listener_;
};

}  // namespace lta
