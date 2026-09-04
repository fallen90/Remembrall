#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace lta {

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

enum class AppState {
  Starting,
  SearchingForDiscord,
  DiscordFound,
  WaitingForAudio,
  Capturing,
  Transcribing,
  DiscordExited,
  CaptureFailed,
  AsrFailed,
  DeviceChange,
  Recovering,
  ShuttingDown,
};

inline const char* ToString(AppState state) {
  switch (state) {
    case AppState::Starting: return "Starting";
    case AppState::SearchingForDiscord: return "Searching for Discord";
    case AppState::DiscordFound: return "Discord detected";
    case AppState::WaitingForAudio: return "Waiting for voice";
    case AppState::Capturing: return "Capturing";
    case AppState::Transcribing: return "Listening";
    case AppState::DiscordExited: return "Discord exited";
    case AppState::CaptureFailed: return "Capture error";
    case AppState::AsrFailed: return "ASR error";
    case AppState::DeviceChange: return "Device change";
    case AppState::Recovering: return "Recovering";
    case AppState::ShuttingDown: return "Shutting down";
  }
  return "Unknown";
}

enum class SegmentState {
  Partial,
  Final,
};

struct AudioFormat {
  uint32_t sample_rate = 0;
  uint16_t channels = 0;
  uint16_t bits_per_sample = 0;
  bool is_float = false;

  [[nodiscard]] bool Valid() const {
    return sample_rate > 0 && channels > 0 && bits_per_sample > 0;
  }

  [[nodiscard]] size_t BytesPerFrame() const {
    return static_cast<size_t>(channels) * (bits_per_sample / 8);
  }
};

struct AudioFrame {
  TimePoint timestamp{};
  uint32_t sample_rate = 0;
  uint16_t channel_count = 0;
  uint32_t frame_count = 0;
  std::vector<float> samples;  // interleaved float32
};

struct TranscriptSegment {
  uint64_t id = 0;
  std::string text;
  SegmentState state = SegmentState::Partial;
  TimePoint audio_start{};
  TimePoint audio_end{};
  TimePoint first_partial{};
  TimePoint finalized{};
};

}  // namespace lta
