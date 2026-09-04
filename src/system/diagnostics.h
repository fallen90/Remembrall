#pragma once

#include "common/types.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace lta {

struct DiagnosticsSnapshot {
  AppState app_state = AppState::Starting;
  uint32_t discord_pid = 0;
  double capture_callback_interval_ms = 0;
  double ring_occupancy_pct = 0;
  double asr_queue_occupancy_pct = 0;
  double resample_duration_ms = 0;
  double asr_inference_ms = 0;
  double rtf = 0;
  double partial_latency_ms = 0;
  double endpoint_latency_ms = 0;
  uint64_t dropped_capture_frames = 0;
  uint64_t dropped_asr_frames = 0;
  uint64_t working_set_bytes = 0;
  double cpu_utilization_pct = 0;
  bool performance_warning = false;
  std::string last_error;
};

class Diagnostics {
 public:
  static Diagnostics& Instance();

  void SetAppState(AppState state);
  void SetDiscordPid(uint32_t pid);
  void SetLastError(std::string error);
  void RecordCaptureInterval(double ms);
  void SetRingOccupancy(double pct);
  void SetAsrQueueOccupancy(double pct);
  void RecordResampleDuration(double ms);
  void RecordAsrInference(double inference_ms, double audio_ms);
  void RecordPartialLatency(double ms);
  void RecordEndpointLatency(double ms);
  void AddDroppedCaptureFrames(uint64_t count);
  void AddDroppedAsrFrames(uint64_t count);
  void SetWorkingSet(uint64_t bytes);
  void SetCpuUtilization(double pct);
  void SetPerformanceWarning(bool warning);

  DiagnosticsSnapshot Snapshot() const;

 private:
  Diagnostics() = default;

  mutable std::atomic<AppState> app_state_{AppState::Starting};
  std::atomic<uint32_t> discord_pid_{0};
  std::atomic<double> capture_interval_ms_{0};
  std::atomic<double> ring_occupancy_pct_{0};
  std::atomic<double> asr_queue_occupancy_pct_{0};
  std::atomic<double> resample_ms_{0};
  std::atomic<double> asr_inference_ms_{0};
  std::atomic<double> rtf_{0};
  std::atomic<double> partial_latency_ms_{0};
  std::atomic<double> endpoint_latency_ms_{0};
  std::atomic<uint64_t> dropped_capture_{0};
  std::atomic<uint64_t> dropped_asr_{0};
  std::atomic<uint64_t> working_set_{0};
  std::atomic<double> cpu_pct_{0};
  std::atomic<bool> perf_warning_{false};

  mutable std::mutex error_mutex_;
  std::string last_error_;
};

}  // namespace lta
