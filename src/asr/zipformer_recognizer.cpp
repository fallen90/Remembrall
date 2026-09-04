#include "asr/zipformer_recognizer.h"

#include "system/diagnostics.h"
#include "system/logger.h"

#include <chrono>

namespace lta {

ZipformerRecognizer::ZipformerRecognizer() = default;
ZipformerRecognizer::~ZipformerRecognizer() = default;

bool ZipformerRecognizer::Initialize(const std::filesystem::path& model_directory,
                                     int num_threads,
                                     double endpoint_rule1_s,
                                     double endpoint_rule2_s,
                                     double endpoint_rule3_s) {
  std::lock_guard lock(mutex_);
  ready_ = false;

#if !defined(LTA_HAS_SHERPA)
  (void)model_directory;
  (void)num_threads;
  (void)endpoint_rule1_s;
  (void)endpoint_rule2_s;
  (void)endpoint_rule3_s;
  LTA_LOG_ERROR("Built without sherpa-onnx (LTA_HAS_SHERPA not defined)");
  return false;
#else
  const auto encoder = model_directory / "encoder-epoch-99-avg-1.int8.onnx";
  const auto decoder = model_directory / "decoder-epoch-99-avg-1.onnx";
  const auto joiner = model_directory / "joiner-epoch-99-avg-1.int8.onnx";
  const auto tokens = model_directory / "tokens.txt";

  if (!std::filesystem::exists(encoder) || !std::filesystem::exists(decoder) ||
      !std::filesystem::exists(joiner) || !std::filesystem::exists(tokens)) {
    LTA_LOG_ERROR("Model files missing under " + model_directory.string());
    return false;
  }

  sherpa_onnx::cxx::OnlineRecognizerConfig config;
  config.model_config.transducer.encoder = encoder.string();
  config.model_config.transducer.decoder = decoder.string();
  config.model_config.transducer.joiner = joiner.string();
  config.model_config.tokens = tokens.string();
  config.model_config.num_threads = num_threads > 0 ? num_threads : 1;
  config.model_config.provider = "cpu";
  config.decoding_method = "greedy_search";
  config.enable_endpoint = true;
  config.rule1_min_trailing_silence = static_cast<float>(endpoint_rule1_s);
  config.rule2_min_trailing_silence = static_cast<float>(endpoint_rule2_s);
  config.rule3_min_utterance_length = static_cast<float>(endpoint_rule3_s);
  config.feat_config.sampling_rate = 16000;
  config.feat_config.feature_dim = 80;

  LTA_LOG_INFO("Loading Zipformer model from " + model_directory.string());
  auto recognizer = sherpa_onnx::cxx::OnlineRecognizer::Create(config);
  if (!recognizer.Get()) {
    LTA_LOG_ERROR("OnlineRecognizer::Create failed");
    return false;
  }

  recognizer_ = std::make_unique<sherpa_onnx::cxx::OnlineRecognizer>(std::move(recognizer));
  stream_ = std::make_unique<sherpa_onnx::cxx::OnlineStream>(recognizer_->CreateStream());
  ready_ = true;
  LTA_LOG_INFO("Zipformer recognizer ready");
  return true;
#endif
}

void ZipformerRecognizer::AcceptAudio(const float* samples_16k_mono, size_t sample_count) {
  std::lock_guard lock(mutex_);
#if defined(LTA_HAS_SHERPA)
  if (!ready_ || !stream_ || !samples_16k_mono || sample_count == 0) {
    return;
  }
  stream_->AcceptWaveform(16000, samples_16k_mono, static_cast<int32_t>(sample_count));
#else
  (void)samples_16k_mono;
  (void)sample_count;
#endif
}

void ZipformerRecognizer::Decode() {
  std::lock_guard lock(mutex_);
#if defined(LTA_HAS_SHERPA)
  if (!ready_ || !recognizer_ || !stream_) {
    return;
  }
  const auto t0 = std::chrono::steady_clock::now();
  int chunks = 0;
  while (recognizer_->IsReady(stream_.get())) {
    recognizer_->Decode(stream_.get());
    ++chunks;
  }
  const auto t1 = std::chrono::steady_clock::now();
  if (chunks > 0) {
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    // Approximate audio per decode chunk ~10–20 ms; use 15 ms * chunks.
    Diagnostics::Instance().RecordAsrInference(ms, 15.0 * chunks);
  }
#endif
}

RecognitionResult ZipformerRecognizer::GetPartialResult() {
  std::lock_guard lock(mutex_);
  RecognitionResult result;
#if defined(LTA_HAS_SHERPA)
  if (!ready_ || !recognizer_ || !stream_) {
    return result;
  }
  auto r = recognizer_->GetResult(stream_.get());
  result.text = r.text;
  result.is_endpoint = recognizer_->IsEndpoint(stream_.get());
#endif
  return result;
}

bool ZipformerRecognizer::IsEndpoint() {
  std::lock_guard lock(mutex_);
#if defined(LTA_HAS_SHERPA)
  if (!ready_ || !recognizer_ || !stream_) {
    return false;
  }
  return recognizer_->IsEndpoint(stream_.get());
#else
  return false;
#endif
}

void ZipformerRecognizer::FinalizeAndReset() {
  std::lock_guard lock(mutex_);
#if defined(LTA_HAS_SHERPA)
  if (!ready_ || !recognizer_ || !stream_) {
    return;
  }
  recognizer_->Reset(stream_.get());
#endif
}

void ZipformerRecognizer::Reset() {
  FinalizeAndReset();
}

bool ZipformerRecognizer::Ready() const {
  std::lock_guard lock(mutex_);
  return ready_;
}

}  // namespace lta
