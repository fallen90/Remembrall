#include "system/memory_monitor.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>

namespace lta {

uint64_t MemoryMonitor::WorkingSetBytes() {
  PROCESS_MEMORY_COUNTERS_EX pmc{};
  pmc.cb = sizeof(pmc);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                            sizeof(pmc))) {
    return 0;
  }
  return static_cast<uint64_t>(pmc.WorkingSetSize);
}

}  // namespace lta
