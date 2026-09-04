#pragma once

#include "asr/streaming_recognizer.h"

#include <memory>
#include <mutex>

#if defined(LTA_HAS_SHERPA)
#include "sherpa-onnx/c-api/cxx-api.h"
#endif

namespace lta {

class ZipformerRecognizer final : public IStreamingRecognizer {
 public:
  ZipformerRecognizer();
  ~ZipformerRecognizer() override;

  bool Initialize(const std::filesystem::path& model_directory,
                  int num_threads,
                  double endpoint_rule1_s,
                  double endpoint_rule2_s,
                  double endpoint_rule3_s) override;

  void AcceptAudio(const float* samples_16k_mono, size_t sample_count) override;
  void Decode() override;
  RecognitionResult GetPartialResult() override;
  bool IsEndpoint() override;
  void FinalizeAndReset() override;
  void Reset() override;
  bool Ready() const override;

 private:
  mutable std::mutex mutex_;
  bool ready_ = false;

#if defined(LTA_HAS_SHERPA)
  std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer> recognizer_;
  std::unique_ptr<sherpa_onnx::cxx::OnlineStream> stream_;
#endif
};

}  // namespace lta
