#pragma once

#include <filesystem>
#include <string>

namespace lta {

struct Configuration {
  bool always_on_top = false;
  bool start_minimized = false;
  bool launch_with_windows = false;
  bool show_partial_results = true;
  bool check_for_updates = true;
  bool auto_download_updates = false;
  std::string github_repo;  // "owner/name" — empty uses compile-time default
  std::wstring language = L"en";
  std::filesystem::path model_directory;
  std::filesystem::path settings_path;

  // Internal tunables (not exposed in V1 UI)
  double endpoint_rule1_silence_s = 2.4;
  double endpoint_rule2_silence_s = 0.8;
  double endpoint_rule3_min_utterance_s = 20.0;
  size_t capture_ring_capacity_bytes = 48000 * 2 * 4 * 1 / 2;  // ~500 ms @ 48k stereo f32
  size_t asr_queue_capacity_samples = 16000;  // 1 second @ 16 kHz mono
  size_t transcript_retention_minutes = 45;
  int asr_num_threads = 1;

  static Configuration LoadDefaults();
  static Configuration LoadFromFile(const std::filesystem::path& path);
  bool SaveToFile(const std::filesystem::path& path) const;
  void ApplyLaunchWithWindows() const;
};

std::filesystem::path DefaultSettingsPath();
std::filesystem::path DefaultModelDirectory();
std::filesystem::path DefaultLogDirectory();

}  // namespace lta
