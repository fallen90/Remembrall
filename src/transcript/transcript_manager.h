#pragma once

#include "transcript/transcript_segment.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace lta {

class TranscriptManager {
 public:
  explicit TranscriptManager(std::chrono::minutes retention = std::chrono::minutes(45));

  void SetRetention(std::chrono::minutes retention);

  void UpdatePartial(std::string text, TimePoint audio_start);
  TranscriptSegment FinalizeCurrent(TimePoint audio_end);
  void Clear();

  [[nodiscard]] std::vector<TranscriptSegment> GetSegments() const;
  [[nodiscard]] std::optional<TranscriptSegment> ActivePartial() const;
  [[nodiscard]] size_t SegmentCount() const;

 private:
  void TrimLocked(TimePoint now);

  mutable std::mutex mutex_;
  std::chrono::minutes retention_;
  std::vector<TranscriptSegment> finals_;
  TranscriptSegment partial_{};
  bool has_partial_ = false;
  uint64_t next_id_ = 1;
};

}  // namespace lta
