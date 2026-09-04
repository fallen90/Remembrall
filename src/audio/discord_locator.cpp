#include "audio/discord_locator.h"

#include "system/logger.h"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <wrl/client.h>

#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace lta {
namespace {

ProcessMonitor::ProcessNames DefaultDiscordNames() {
  return {L"Discord.exe", L"DiscordCanary.exe", L"DiscordPTB.exe"};
}

}  // namespace

DiscordLocator::DiscordLocator() : monitor_(DefaultDiscordNames()) {}

void DiscordLocator::AddProcessName(std::wstring exe_name) {
  extra_names_.push_back(std::move(exe_name));
}

bool DiscordLocator::HasActiveRenderSession(uint32_t pid) const {
  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), &enumerator);
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<IMMDevice> device;
  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<IAudioSessionManager2> session_manager;
  hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &session_manager);
  if (FAILED(hr)) {
    return false;
  }

  ComPtr<IAudioSessionEnumerator> session_enum;
  hr = session_manager->GetSessionEnumerator(&session_enum);
  if (FAILED(hr)) {
    return false;
  }

  int count = 0;
  session_enum->GetCount(&count);
  for (int i = 0; i < count; ++i) {
    ComPtr<IAudioSessionControl> control;
    if (FAILED(session_enum->GetSession(i, &control))) {
      continue;
    }
    ComPtr<IAudioSessionControl2> control2;
    if (FAILED(control.As(&control2))) {
      continue;
    }
    DWORD session_pid = 0;
    if (FAILED(control2->GetProcessId(&session_pid))) {
      continue;
    }
    if (session_pid != pid) {
      continue;
    }
    AudioSessionState state = AudioSessionStateInactive;
    control->GetState(&state);
    if (state == AudioSessionStateActive) {
      return true;
    }
  }
  return false;
}

std::vector<DiscordProcess> DiscordLocator::ListCandidates() const {
  std::vector<DiscordProcess> out;
  auto matches = monitor_.FindMatchingProcesses();

  if (!extra_names_.empty()) {
    ProcessMonitor extra(extra_names_);
    auto more = extra.FindMatchingProcesses();
    matches.insert(matches.end(), more.begin(), more.end());
  }

  // Deduplicate by PID.
  std::sort(matches.begin(), matches.end(),
            [](const ProcessInfo& a, const ProcessInfo& b) { return a.pid < b.pid; });
  matches.erase(std::unique(matches.begin(), matches.end(),
                            [](const ProcessInfo& a, const ProcessInfo& b) {
                              return a.pid == b.pid;
                            }),
                matches.end());

  for (const auto& m : matches) {
    DiscordProcess dp;
    dp.pid = m.pid;
    dp.exe_name = m.name;
    dp.has_active_audio_session = HasActiveRenderSession(m.pid);
    out.push_back(std::move(dp));
  }
  return out;
}

std::optional<DiscordProcess> DiscordLocator::Locate() const {
  auto candidates = ListCandidates();
  if (candidates.empty()) {
    return std::nullopt;
  }

  auto it = std::find_if(candidates.begin(), candidates.end(),
                         [](const DiscordProcess& p) { return p.has_active_audio_session; });
  if (it != candidates.end()) {
    LTA_LOG_INFO("Located Discord with active audio session pid=" + std::to_string(it->pid));
    return *it;
  }

  LTA_LOG_INFO("Located Discord without active session yet pid=" +
               std::to_string(candidates.front().pid));
  return candidates.front();
}

}  // namespace lta
