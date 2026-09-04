#pragma once

#include "system/process_monitor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lta {

struct DiscordProcess {
  uint32_t pid = 0;
  std::wstring exe_name;
  bool has_active_audio_session = false;
};

class DiscordLocator {
 public:
  DiscordLocator();

  // Extensible process name list.
  void AddProcessName(std::wstring exe_name);

  // Find Discord processes and pick the best PID for process-loopback capture.
  // Preference: process (or tree root) with an active audio render session.
  [[nodiscard]] std::optional<DiscordProcess> Locate() const;

  [[nodiscard]] std::vector<DiscordProcess> ListCandidates() const;

 private:
  ProcessMonitor monitor_;
  mutable std::vector<std::wstring> extra_names_;
  [[nodiscard]] bool HasActiveRenderSession(uint32_t pid) const;
};

}  // namespace lta
