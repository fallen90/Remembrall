#pragma once

#include "app/state_machine.h"
#include "asr/zipformer_recognizer.h"
#include "audio/discord_locator.h"
#include "audio/process_loopback.h"
#include "audio/resampler.h"
#include "audio/ring_buffer.h"
#include "audio/device_change_monitor.h"
#include "config/configuration.h"
#include "system/auto_updater.h"
#include "transcript/transcript_manager.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace lta {

class Application {
 public:
  Application();
  ~Application();

  bool Initialize(Configuration config);
  void Start();
  void Shutdown();

  [[nodiscard]] StateMachine& States() { return state_machine_; }
  [[nodiscard]] const StateMachine& States() const { return state_machine_; }
  [[nodiscard]] TranscriptManager& Transcripts() { return transcripts_; }
  [[nodiscard]] const TranscriptManager& Transcripts() const { return transcripts_; }
  [[nodiscard]] Configuration& Config() { return config_; }
  [[nodiscard]] const Configuration& Config() const { return config_; }
  [[nodiscard]] AutoUpdater& Updater() { return updater_; }
  [[nodiscard]] const AutoUpdater& Updater() const { return updater_; }
  [[nodiscard]] bool IsRunning() const { return running_.load(); }

  void CheckForUpdates(bool interactive);
  bool ApplyPendingUpdate();

  [[nodiscard]] std::wstring StatusLine() const;
  [[nodiscard]] bool ModelsReady() const { return models_ready_.load(); }

 private:
  void SessionMonitorLoop();
  void PreprocessLoop();
  void AsrLoop();
  void MemoryPollLoop();
  void ModelSetupLoop();
  bool TryLoadRecognizer();

  bool StartCaptureForPid(uint32_t pid);
  void StopCapture();
  void RequestRecovery(const std::string& reason);

  struct AsrChunk {
    TimePoint timestamp{};
    std::vector<float> samples;
  };
  bool EnqueueAsr(AsrChunk chunk);
  bool DequeueAsr(AsrChunk& out);

  Configuration config_;
  StateMachine state_machine_;
  DiscordLocator locator_;
  TranscriptManager transcripts_;
  ZipformerRecognizer recognizer_;
  AutoUpdater updater_;

  std::unique_ptr<RingBuffer> capture_ring_;
  std::unique_ptr<ProcessLoopbackCapture> capture_;
  DeviceChangeMonitor device_monitor_;
  Resampler resampler_;
  AudioFormat last_capture_format_{};

  std::mutex asr_mutex_;
  std::condition_variable asr_cv_;
  std::deque<AsrChunk> asr_queue_;
  size_t asr_queued_samples_ = 0;

  std::atomic<bool> running_{false};
  std::atomic<bool> recovery_requested_{false};
  std::atomic<bool> models_ready_{false};
  std::atomic<uint32_t> active_pid_{0};

  mutable std::mutex status_mutex_;
  std::string model_status_;

  std::thread session_thread_;
  std::thread preprocess_thread_;
  std::thread asr_thread_;
  std::thread memory_thread_;
  std::thread model_thread_;
};

}  // namespace lta
