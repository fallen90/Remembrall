#pragma once

#include "audio/ring_buffer.h"
#include "common/types.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace lta {

enum class CaptureState {
  Stopped,
  Starting,
  Running,
  Failed,
};

class ProcessLoopbackCapture {
 public:
  using ErrorCallback = std::function<void(const std::string& message)>;

  ProcessLoopbackCapture();
  ~ProcessLoopbackCapture();

  ProcessLoopbackCapture(const ProcessLoopbackCapture&) = delete;
  ProcessLoopbackCapture& operator=(const ProcessLoopbackCapture&) = delete;

  // Start capturing rendered audio for target_pid (+ child tree).
  // PCM float32 interleaved frames are written into the provided ring buffer
  // as raw float bytes (channels * frames * sizeof(float)), preceded by nothing —
  // consumers must know the negotiated mix format via SourceFormat().
  bool Start(uint32_t target_pid, RingBuffer& output_ring);
  void Stop();

  [[nodiscard]] CaptureState State() const { return state_.load(); }
  [[nodiscard]] AudioFormat SourceFormat() const;
  [[nodiscard]] uint32_t TargetPid() const { return target_pid_.load(); }
  [[nodiscard]] double LastRms() const { return last_rms_.load(); }
  [[nodiscard]] uint64_t FramesCaptured() const { return frames_captured_.load(); }

  void SetErrorCallback(ErrorCallback cb);

 private:
  void CaptureThreadMain();
  bool ActivateAndRun(RingBuffer& output_ring);

  std::atomic<CaptureState> state_{CaptureState::Stopped};
  std::atomic<uint32_t> target_pid_{0};
  std::atomic<double> last_rms_{0};
  std::atomic<uint64_t> frames_captured_{0};
  std::atomic<bool> stop_requested_{false};

  mutable std::mutex format_mutex_;
  AudioFormat source_format_{};

  std::mutex callback_mutex_;
  ErrorCallback error_callback_;

  RingBuffer* ring_ = nullptr;
  std::thread capture_thread_;
};

}  // namespace lta
