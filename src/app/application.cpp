#include "app/application.h"

#include "audio/audio_format.h"
#include "system/diagnostics.h"
#include "system/logger.h"
#include "system/memory_monitor.h"
#include "system/model_installer.h"
#include "system/process_monitor.h"
#include "system/version.h"

#include <chrono>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>

namespace lta {
namespace {

constexpr size_t kFloatBytes = sizeof(float);
constexpr int kMonitorIntervalMs = 750;
constexpr size_t kPreprocessReadBytes = 48000 * 2 * sizeof(float) / 20;  // ~50 ms @ 48k stereo

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

}  // namespace

Application::Application() = default;

Application::~Application() {
  Shutdown();
}

bool Application::TryLoadRecognizer() {
  if (!ModelInstaller::IsInstalled(config_.model_directory)) {
    return false;
  }
  if (!recognizer_.Initialize(config_.model_directory,
                              config_.asr_num_threads,
                              config_.endpoint_rule1_silence_s,
                              config_.endpoint_rule2_silence_s,
                              config_.endpoint_rule3_min_utterance_s)) {
    state_machine_.OnAsrFailed("Failed to load ASR model");
    LTA_LOG_ERROR("ASR model load failed");
    return false;
  }
  models_ready_.store(true);
  {
    std::lock_guard lock(status_mutex_);
    model_status_.clear();
  }
  if (active_pid_.load() != 0) {
    state_machine_.OnTranscribing();
  } else {
    state_machine_.Transition(AppState::SearchingForDiscord);
  }
  return true;
}

bool Application::Initialize(Configuration config) {
  config_ = std::move(config);
  transcripts_.SetRetention(std::chrono::minutes(config_.transcript_retention_minutes));

  capture_ring_ = std::make_unique<RingBuffer>(config_.capture_ring_capacity_bytes);
  capture_ = std::make_unique<ProcessLoopbackCapture>();
  capture_->SetErrorCallback([this](const std::string& msg) {
    RequestRecovery(msg);
  });

  device_monitor_.Start([this]() {
    // Debounce rapid BT/profile notifications.
    static auto last = SteadyClock::time_point{};
    const auto now = SteadyClock::now();
    if (now - last < std::chrono::milliseconds(750)) {
      return;
    }
    last = now;
    state_machine_.OnDeviceChange();
    RequestRecovery("Audio device change");
  });

  if (ModelInstaller::IsInstalled(config_.model_directory)) {
    TryLoadRecognizer();
  } else {
    state_machine_.Transition(AppState::AsrFailed);
    std::lock_guard lock(status_mutex_);
    model_status_ = "Speech model will download on first launch…";
  }

  updater_.SetEnabled(config_.check_for_updates);
  if (!config_.github_repo.empty()) {
    updater_.SetGithubRepo(config_.github_repo);
  } else {
    updater_.SetGithubRepo(DefaultGithubRepo());
  }

  if (models_ready_.load()) {
    state_machine_.Transition(AppState::SearchingForDiscord);
  }
  return true;
}

void Application::Start() {
  if (running_.exchange(true)) {
    return;
  }
  session_thread_ = std::thread([this] { SessionMonitorLoop(); });
  preprocess_thread_ = std::thread([this] { PreprocessLoop(); });
  asr_thread_ = std::thread([this] { AsrLoop(); });
  memory_thread_ = std::thread([this] { MemoryPollLoop(); });
  if (!models_ready_.load()) {
    model_thread_ = std::thread([this] { ModelSetupLoop(); });
  }

  if (config_.check_for_updates) {
    updater_.CheckForUpdatesAsync(false);
  }
  LTA_LOG_INFO(std::string("Application workers started v") + CurrentVersionString());
}

void Application::ModelSetupLoop() {
  const bool ok = ModelInstaller::EnsureInstalled(
      config_.model_directory,
      [this](const std::string& msg) {
        std::lock_guard lock(status_mutex_);
        model_status_ = msg;
      });
  if (!ok) {
    state_machine_.OnAsrFailed("Speech model download failed");
    return;
  }
  TryLoadRecognizer();
}

void Application::CheckForUpdates(bool interactive) {
  updater_.SetEnabled(true);
  updater_.CheckForUpdatesAsync(interactive);
}

bool Application::ApplyPendingUpdate() {
  return updater_.ApplyAndRelaunch();
}

void Application::Shutdown() {
  running_.store(false);
  asr_cv_.notify_all();
  device_monitor_.Stop();
  StopCapture();

  auto join = [](std::thread& t) {
    if (t.joinable()) {
      t.join();
    }
  };
  join(session_thread_);
  join(preprocess_thread_);
  join(asr_thread_);
  join(memory_thread_);
  join(model_thread_);
  state_machine_.Transition(AppState::ShuttingDown);
  LTA_LOG_INFO("Application shutdown complete");
}

std::wstring Application::StatusLine() const {
  {
    std::lock_guard lock(status_mutex_);
    if (!model_status_.empty()) {
      return Utf8ToWide(model_status_);
    }
  }
  const auto snap = Diagnostics::Instance().Snapshot();
  std::wostringstream oss;
  const char* s = ToString(state_machine_.State());
  while (*s) {
    oss << static_cast<wchar_t>(*s++);
  }
  oss << L"     English     "
      << static_cast<int>(snap.partial_latency_ms) << L" ms     "
      << (snap.working_set_bytes / (1024 * 1024)) << L" MB";
  return oss.str();
}

void Application::RequestRecovery(const std::string& reason) {
  LTA_LOG_WARN("Recovery requested: " + reason);
  Diagnostics::Instance().SetLastError(reason);
  recovery_requested_.store(true);
  state_machine_.OnRecovering();
}

bool Application::StartCaptureForPid(uint32_t pid) {
  StopCapture();
  capture_ring_->Clear();
  active_pid_.store(pid);
  state_machine_.OnDiscordFound(pid);
  state_machine_.Transition(AppState::WaitingForAudio);
  if (!capture_->Start(pid, *capture_ring_)) {
    state_machine_.OnCaptureFailed("Failed to start capture thread");
    return false;
  }
  state_machine_.OnCaptureStarted();
  if (recognizer_.Ready()) {
    state_machine_.OnTranscribing();
  }
  return true;
}

void Application::StopCapture() {
  if (capture_) {
    capture_->Stop();
  }
  active_pid_.store(0);
}

bool Application::EnqueueAsr(AsrChunk chunk) {
  std::lock_guard lock(asr_mutex_);
  const size_t incoming = chunk.samples.size();
  while (asr_queued_samples_ + incoming > config_.asr_queue_capacity_samples &&
         !asr_queue_.empty()) {
    asr_queued_samples_ -= asr_queue_.front().samples.size();
    Diagnostics::Instance().AddDroppedAsrFrames(asr_queue_.front().samples.size());
    Diagnostics::Instance().SetPerformanceWarning(true);
    asr_queue_.pop_front();
  }
  if (asr_queued_samples_ + incoming > config_.asr_queue_capacity_samples) {
    // Still too large — drop this chunk.
    Diagnostics::Instance().AddDroppedAsrFrames(incoming);
    return false;
  }
  asr_queued_samples_ += incoming;
  asr_queue_.push_back(std::move(chunk));
  const double pct =
      100.0 * static_cast<double>(asr_queued_samples_) /
      static_cast<double>(config_.asr_queue_capacity_samples);
  Diagnostics::Instance().SetAsrQueueOccupancy(pct);
  asr_cv_.notify_one();
  return true;
}

bool Application::DequeueAsr(AsrChunk& out) {
  std::unique_lock lock(asr_mutex_);
  asr_cv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
    return !asr_queue_.empty() || !running_.load();
  });
  if (asr_queue_.empty()) {
    return false;
  }
  out = std::move(asr_queue_.front());
  asr_queue_.pop_front();
  asr_queued_samples_ -= out.samples.size();
  return true;
}

void Application::SessionMonitorLoop() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool should_uninit = SUCCEEDED(hr) || hr == S_FALSE;
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    LTA_LOG_ERROR("Session monitor COM init failed");
    return;
  }

  ProcessMonitor life({L"Discord.exe", L"DiscordCanary.exe", L"DiscordPTB.exe"});
  while (running_.load()) {
    if (recovery_requested_.exchange(false)) {
      StopCapture();
      state_machine_.Transition(AppState::SearchingForDiscord);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    const uint32_t pid = active_pid_.load();
    if (pid != 0) {
      if (!life.IsProcessAlive(pid)) {
        LTA_LOG_INFO("Discord process exited");
        StopCapture();
        state_machine_.OnDiscordLost();
        state_machine_.Transition(AppState::SearchingForDiscord);
      } else if (capture_ && capture_->State() == CaptureState::Failed) {
        RequestRecovery("Capture failed");
      }
    } else {
      auto found = locator_.Locate();
      if (found) {
        StartCaptureForPid(found->pid);
      } else if (state_machine_.State() != AppState::SearchingForDiscord) {
        state_machine_.Transition(AppState::SearchingForDiscord);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kMonitorIntervalMs));
  }

  if (should_uninit) {
    CoUninitialize();
  }
}

void Application::PreprocessLoop() {
  std::vector<uint8_t> raw(kPreprocessReadBytes);
  std::vector<float> resampled;
  resampled.reserve(1600);

  while (running_.load()) {
    if (!capture_ring_ || capture_->State() != CaptureState::Running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    const auto format = capture_->SourceFormat();
    if (!format.Valid()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    if (format.sample_rate != last_capture_format_.sample_rate ||
        format.channels != last_capture_format_.channels) {
      resampler_.Configure(format.sample_rate, format.channels);
      last_capture_format_ = format;
      LTA_LOG_INFO("Resampler reconfigured for " + FormatToString(format));
    }

    const size_t bytes_per_float_frame = static_cast<size_t>(format.channels) * kFloatBytes;
    const size_t aligned = raw.size() - (raw.size() % bytes_per_float_frame);
    const size_t nread = capture_ring_->Read(raw.data(), aligned);
    if (nread < bytes_per_float_frame) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    const size_t frames = nread / bytes_per_float_frame;
    const auto* samples = reinterpret_cast<const float*>(raw.data());
    const auto t0 = std::chrono::steady_clock::now();
    resampled.clear();
    resampler_.Process(samples, frames, resampled);
    const auto t1 = std::chrono::steady_clock::now();
    Diagnostics::Instance().RecordResampleDuration(
        std::chrono::duration<double, std::milli>(t1 - t0).count());

    if (!resampled.empty()) {
      AsrChunk chunk;
      chunk.timestamp = t0;
      chunk.samples = std::move(resampled);
      EnqueueAsr(std::move(chunk));
    }
  }
}

void Application::AsrLoop() {
  std::string last_partial;
  TimePoint utterance_start{};
  bool in_utterance = false;

  while (running_.load()) {
    if (!recognizer_.Ready()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    AsrChunk chunk;
    if (!DequeueAsr(chunk)) {
      continue;
    }

    if (!in_utterance) {
      utterance_start = chunk.timestamp;
      in_utterance = true;
    }

    recognizer_.AcceptAudio(chunk.samples.data(), chunk.samples.size());
    recognizer_.Decode();
    auto result = recognizer_.GetPartialResult();

    if (config_.show_partial_results && !result.text.empty() && result.text != last_partial) {
      transcripts_.UpdatePartial(result.text, utterance_start);
      last_partial = result.text;
    }

    if (recognizer_.IsEndpoint()) {
      if (!last_partial.empty()) {
        transcripts_.FinalizeCurrent(SteadyClock::now());
      }
      recognizer_.FinalizeAndReset();
      last_partial.clear();
      in_utterance = false;
    }
  }
}

void Application::MemoryPollLoop() {
  while (running_.load()) {
    const auto ws = MemoryMonitor::WorkingSetBytes();
    Diagnostics::Instance().SetWorkingSet(ws);
    if (ws > 1024ull * 1024ull * 1024ull) {
      Diagnostics::Instance().SetPerformanceWarning(true);
      LTA_LOG_WARN("Working set exceeded 1 GB ceiling");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

}  // namespace lta
