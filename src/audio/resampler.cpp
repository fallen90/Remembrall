#include "audio/resampler.h"

#include <algorithm>
#include <cmath>

namespace lta {

constexpr uint32_t kTargetSampleRate = 16000;

Resampler::Resampler() = default;
Resampler::~Resampler() = default;

bool Resampler::Configure(uint32_t source_sample_rate, uint16_t source_channels) {
  if (source_sample_rate == 0 || source_channels == 0) {
    return false;
  }
  source_rate_ = source_sample_rate;
  source_channels_ = source_channels;
  Reset();
  return true;
}

void Resampler::Reset() {
  phase_ = 0.0;
  last_sample_ = 0.0f;
  have_last_ = false;
  pending_.clear();
}

void DownmixToMono(const float* interleaved,
                   size_t frame_count,
                   uint16_t channels,
                   std::vector<float>& mono_out) {
  mono_out.resize(frame_count);
  if (channels == 1) {
    std::copy(interleaved, interleaved + frame_count, mono_out.begin());
    return;
  }
  const float inv = 1.0f / static_cast<float>(channels);
  for (size_t i = 0; i < frame_count; ++i) {
    float sum = 0.0f;
    for (uint16_t c = 0; c < channels; ++c) {
      sum += interleaved[i * channels + c];
    }
    mono_out[i] = sum * inv;
  }
}

bool Resampler::Process(const float* interleaved,
                        size_t frame_count,
                        std::vector<float>& out_mono_16k) {
  if (!interleaved || frame_count == 0 || source_rate_ == 0) {
    return false;
  }

  std::vector<float> mono;
  DownmixToMono(interleaved, frame_count, source_channels_, mono);

  if (source_rate_ == kTargetSampleRate) {
    out_mono_16k.insert(out_mono_16k.end(), mono.begin(), mono.end());
    return true;
  }

  pending_.insert(pending_.end(), mono.begin(), mono.end());

  const double step = static_cast<double>(source_rate_) / static_cast<double>(kTargetSampleRate);

  while (phase_ + 1.0 < static_cast<double>(pending_.size())) {
    const size_t i0 = static_cast<size_t>(phase_);
    const size_t i1 = i0 + 1;
    const float frac = static_cast<float>(phase_ - static_cast<double>(i0));
    out_mono_16k.push_back(pending_[i0] + (pending_[i1] - pending_[i0]) * frac);
    phase_ += step;
  }

  // Drop consumed whole samples; keep fractional phase relative to remaining buffer.
  const size_t drop = static_cast<size_t>(phase_);
  if (drop > 0 && drop <= pending_.size()) {
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(drop));
    phase_ -= static_cast<double>(drop);
  }

  // Cap pending growth under pathological input (should stay tiny).
  constexpr size_t kMaxPending = 48000;
  if (pending_.size() > kMaxPending) {
    pending_.erase(pending_.begin(),
                   pending_.begin() + static_cast<std::ptrdiff_t>(pending_.size() - kMaxPending / 2));
    phase_ = 0.0;
  }

  return true;
}

}  // namespace lta
