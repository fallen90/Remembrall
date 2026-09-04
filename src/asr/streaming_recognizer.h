#pragma once

#include "common/types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace lta {

struct RecognitionResult {
  std::string text;
  bool is_endpoint = false;
};

class IStreamingRecognizer {
 public:
  virtual ~IStreamingRecognizer() = default;

  virtual bool Initialize(const std::filesystem::path& model_directory,
                          int num_threads,
                          double endpoint_rule1_s,
                          double endpoint_rule2_s,
                          double endpoint_rule3_s) = 0;

  virtual void AcceptAudio(const float* samples_16k_mono, size_t sample_count) = 0;
  virtual void Decode() = 0;
  [[nodiscard]] virtual RecognitionResult GetPartialResult() = 0;
  [[nodiscard]] virtual bool IsEndpoint() = 0;
  virtual void FinalizeAndReset() = 0;
  virtual void Reset() = 0;
  [[nodiscard]] virtual bool Ready() const = 0;
};

}  // namespace lta
