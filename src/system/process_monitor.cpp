#include "system/process_monitor.h"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>

namespace lta {
namespace {

std::wstring ToLower(std::wstring s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
  return s;
}

}  // namespace

ProcessMonitor::ProcessMonitor(ProcessNames names) : names_(std::move(names)) {
  for (auto& n : names_) {
    n = ToLower(n);
  }
}

std::vector<ProcessInfo> ProcessMonitor::FindMatchingProcesses() const {
  std::vector<ProcessInfo> result;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) {
    return result;
  }

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      const auto exe = ToLower(pe.szExeFile);
      for (const auto& target : names_) {
        if (exe == target) {
          ProcessInfo info;
          info.pid = pe.th32ProcessID;
          info.name = pe.szExeFile;
          result.push_back(std::move(info));
          break;
        }
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return result;
}

bool ProcessMonitor::IsProcessAlive(uint32_t pid) const {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) {
    return false;
  }
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
  CloseHandle(h);
  return alive;
}

}  // namespace lta
