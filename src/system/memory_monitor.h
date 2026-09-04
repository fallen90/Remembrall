#pragma once

#include <cstdint>

namespace lta {

class MemoryMonitor {
 public:
  // Returns current process working set in bytes (0 on failure).
  [[nodiscard]] static uint64_t WorkingSetBytes();
};

}  // namespace lta
