#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace lta {

struct ProcessInfo {
  uint32_t pid = 0;
  std::wstring name;
  std::wstring path;
};

class ProcessMonitor {
 public:
  using ProcessNames = std::vector<std::wstring>;

  explicit ProcessMonitor(ProcessNames names);

  [[nodiscard]] std::vector<ProcessInfo> FindMatchingProcesses() const;
  [[nodiscard]] bool IsProcessAlive(uint32_t pid) const;

 private:
  ProcessNames names_;
};

}  // namespace lta
