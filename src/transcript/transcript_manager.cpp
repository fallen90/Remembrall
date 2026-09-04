#include "transcript/transcript_manager.h"

#include "system/diagnostics.h"

#include <algorithm>

namespace lta {

TranscriptManager::TranscriptManager(std::chrono::minutes retention)
    : retention_(retention) {}

void TranscriptManager::SetRetention(std::chrono::minutes retention) {
  std::lock_guard lock(mutex_);
  retention_ = retention;
}

void TranscriptManager::UpdatePartial(std::string text, TimePoint audio_start) {
  std::lock_guard lock(mutex_);
  const auto now = SteadyClock::now();
  if (!has_partial_) {
    partial_ = TranscriptSegment{};
    partial_.id = next_id_++;
    partial_.state = SegmentState::Partial;
    partial_.audio_start = audio_start;
    partial_.first_partial = now;
    has_partial_ = true;

    const double latency_ms =
        std::chrono::duration<double, std::milli>(now - audio_start).count();
    Diagnostics::Instance().RecordPartialLatency(latency_ms);
  }
  partial_.text = std::move(text);
  partial_.audio_end = now;
}

TranscriptSegment TranscriptManager::FinalizeCurrent(TimePoint audio_end) {
  std::lock_guard lock(mutex_);
  TranscriptSegment done{};
  if (!has_partial_) {
    return done;
  }
  partial_.state = SegmentState::Final;
  partial_.audio_end = audio_end;
  partial_.finalized = SteadyClock::now();

  const double endpoint_ms =
      std::chrono::duration<double, std::milli>(partial_.finalized - partial_.audio_end).count();
  Diagnostics::Instance().RecordEndpointLatency(std::max(0.0, endpoint_ms));

  done = partial_;
  finals_.push_back(partial_);
  has_partial_ = false;
  partial_ = {};
  TrimLocked(SteadyClock::now());
  return done;
}

void TranscriptManager::Clear() {
  std::lock_guard lock(mutex_);
  finals_.clear();
  has_partial_ = false;
  partial_ = {};
}

std::vector<TranscriptSegment> TranscriptManager::GetSegments() const {
  std::lock_guard lock(mutex_);
  auto out = finals_;
  if (has_partial_) {
    out.push_back(partial_);
  }
  return out;
}

std::optional<TranscriptSegment> TranscriptManager::ActivePartial() const {
  std::lock_guard lock(mutex_);
  if (!has_partial_) {
    return std::nullopt;
  }
  return partial_;
}

size_t TranscriptManager::SegmentCount() const {
  std::lock_guard lock(mutex_);
  return finals_.size() + (has_partial_ ? 1 : 0);
}

void TranscriptManager::TrimLocked(TimePoint now) {
  const auto cutoff = now - retention_;
  while (!finals_.empty() && finals_.front().finalized < cutoff) {
    finals_.erase(finals_.begin());
  }
}

}  // namespace lta
