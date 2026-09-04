#include "system/diagnostics.h"

namespace lta {

Diagnostics& Diagnostics::Instance() {
  static Diagnostics instance;
  return instance;
}

void Diagnostics::SetAppState(AppState state) { app_state_.store(state); }
void Diagnostics::SetDiscordPid(uint32_t pid) { discord_pid_.store(pid); }

void Diagnostics::SetLastError(std::string error) {
  std::lock_guard lock(error_mutex_);
  last_error_ = std::move(error);
}

void Diagnostics::RecordCaptureInterval(double ms) { capture_interval_ms_.store(ms); }
void Diagnostics::SetRingOccupancy(double pct) { ring_occupancy_pct_.store(pct); }
void Diagnostics::SetAsrQueueOccupancy(double pct) { asr_queue_occupancy_pct_.store(pct); }
void Diagnostics::RecordResampleDuration(double ms) { resample_ms_.store(ms); }

void Diagnostics::RecordAsrInference(double inference_ms, double audio_ms) {
  asr_inference_ms_.store(inference_ms);
  if (audio_ms > 0.0) {
    rtf_.store(inference_ms / audio_ms);
  }
}

void Diagnostics::RecordPartialLatency(double ms) { partial_latency_ms_.store(ms); }
void Diagnostics::RecordEndpointLatency(double ms) { endpoint_latency_ms_.store(ms); }
void Diagnostics::AddDroppedCaptureFrames(uint64_t count) { dropped_capture_.fetch_add(count); }
void Diagnostics::AddDroppedAsrFrames(uint64_t count) { dropped_asr_.fetch_add(count); }
void Diagnostics::SetWorkingSet(uint64_t bytes) { working_set_.store(bytes); }
void Diagnostics::SetCpuUtilization(double pct) { cpu_pct_.store(pct); }
void Diagnostics::SetPerformanceWarning(bool warning) { perf_warning_.store(warning); }

DiagnosticsSnapshot Diagnostics::Snapshot() const {
  DiagnosticsSnapshot s;
  s.app_state = app_state_.load();
  s.discord_pid = discord_pid_.load();
  s.capture_callback_interval_ms = capture_interval_ms_.load();
  s.ring_occupancy_pct = ring_occupancy_pct_.load();
  s.asr_queue_occupancy_pct = asr_queue_occupancy_pct_.load();
  s.resample_duration_ms = resample_ms_.load();
  s.asr_inference_ms = asr_inference_ms_.load();
  s.rtf = rtf_.load();
  s.partial_latency_ms = partial_latency_ms_.load();
  s.endpoint_latency_ms = endpoint_latency_ms_.load();
  s.dropped_capture_frames = dropped_capture_.load();
  s.dropped_asr_frames = dropped_asr_.load();
  s.working_set_bytes = working_set_.load();
  s.cpu_utilization_pct = cpu_pct_.load();
  s.performance_warning = perf_warning_.load();
  {
    std::lock_guard lock(error_mutex_);
    s.last_error = last_error_;
  }
  return s;
}

}  // namespace lta
