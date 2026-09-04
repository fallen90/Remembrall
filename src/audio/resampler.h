#pragma once

#include "common/types.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace lta {

class Resampler {
 public:
  Resampler();
  ~Resampler();

  Resampler(const Resampler&) = delete;
  Resampler& operator=(const Resampler&) = delete;

  // Configure for a source format. Target is always 16 kHz mono float32.
  bool Configure(uint32_t source_sample_rate, uint16_t source_channels);

  // Input: interleaved float32. Output: 16 kHz mono float32 samples appended to out.
  bool Process(const float* interleaved,
               size_t frame_count,
               std::vector<float>& out_mono_16k);

  void Reset();

  [[nodiscard]] uint32_t SourceRate() const { return source_rate_; }
  [[nodiscard]] uint16_t SourceChannels() const { return source_channels_; }

 private:
  uint32_t source_rate_ = 0;
  uint16_t source_channels_ = 0;

  // Simple linear resampler state (no MF dependency for predictability / low RAM).
  double phase_ = 0.0;
  float last_sample_ = 0.0f;
  bool have_last_ = false;
  std::vector<float> pending_;
};

// Downmix interleaved float to mono (average channels).
void DownmixToMono(const float* interleaved,
                   size_t frame_count,
                   uint16_t channels,
                   std::vector<float>& mono_out);

}  // namespace lta
