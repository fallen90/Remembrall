#include "app/state_machine.h"

#include "system/diagnostics.h"
#include "system/logger.h"

namespace lta {

StateMachine::StateMachine() = default;

void StateMachine::SetListener(Listener listener) {
  std::lock_guard lock(listener_mutex_);
  listener_ = std::move(listener);
}

void StateMachine::Transition(AppState next) {
  const AppState prev = state_.exchange(next);
  if (prev == next) {
    return;
  }
  Diagnostics::Instance().SetAppState(next);
  LTA_LOG_INFO(std::string("State ") + ToString(prev) + " -> " + ToString(next));

  Listener copy;
  {
    std::lock_guard lock(listener_mutex_);
    copy = listener_;
  }
  if (copy) {
    copy(prev, next);
  }
}

void StateMachine::OnDiscordFound(uint32_t pid) {
  Diagnostics::Instance().SetDiscordPid(pid);
  Transition(AppState::DiscordFound);
}

void StateMachine::OnDiscordLost() {
  Diagnostics::Instance().SetDiscordPid(0);
  Transition(AppState::DiscordExited);
}

void StateMachine::OnCaptureStarted() {
  Transition(AppState::Capturing);
}

void StateMachine::OnCaptureFailed(const std::string& reason) {
  Diagnostics::Instance().SetLastError(reason);
  Transition(AppState::CaptureFailed);
}

void StateMachine::OnTranscribing() {
  Transition(AppState::Transcribing);
}

void StateMachine::OnDeviceChange() {
  Transition(AppState::DeviceChange);
}

void StateMachine::OnRecovering() {
  Transition(AppState::Recovering);
}

void StateMachine::OnAsrFailed(const std::string& reason) {
  Diagnostics::Instance().SetLastError(reason);
  Transition(AppState::AsrFailed);
}

}  // namespace lta
