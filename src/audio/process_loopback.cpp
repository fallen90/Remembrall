#include "audio/process_loopback.h"

#include "audio/audio_format.h"
#include "system/diagnostics.h"
#include "system/logger.h"

#include <avrt.h>
#include <chrono>
#include <cmath>
#include <cstring>

#include <wil/com.h>
#include <wil/result.h>
#include <wil/resource.h>

#include <Audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>

namespace lta {
namespace {

class ActivationCompletionHandler final : public IActivateAudioInterfaceCompletionHandler {
 public:
  ActivationCompletionHandler() {
    event_.create();
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) {
      return E_POINTER;
    }
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
      *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_);
  }

  STDMETHODIMP_(ULONG) Release() override {
    const LONG r = InterlockedDecrement(&ref_);
    if (r == 0) {
      delete this;
    }
    return r;
  }

  STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
    HRESULT activate_hr = E_FAIL;
    wil::com_ptr_nothrow<IUnknown> punk;
    HRESULT hr = operation->GetActivateResult(&activate_hr, &punk);
    if (SUCCEEDED(hr) && SUCCEEDED(activate_hr)) {
      hr = punk.query_to(&audio_client_);
      result_ = hr;
    } else {
      result_ = FAILED(hr) ? hr : activate_hr;
    }
    event_.SetEvent();
    return S_OK;
  }

  HRESULT Wait(wil::com_ptr_nothrow<IAudioClient>& out_client) {
    event_.wait();
    if (FAILED(result_)) {
      return result_;
    }
    out_client = audio_client_;
    return S_OK;
  }

 private:
  LONG ref_ = 1;
  wil::unique_event event_;
  HRESULT result_ = E_FAIL;
  wil::com_ptr_nothrow<IAudioClient> audio_client_;
};

float ComputeRms(const float* samples, size_t count) {
  if (count == 0) {
    return 0.0f;
  }
  double sum = 0.0;
  for (size_t i = 0; i < count; ++i) {
    sum += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

}  // namespace

ProcessLoopbackCapture::ProcessLoopbackCapture() = default;

ProcessLoopbackCapture::~ProcessLoopbackCapture() {
  Stop();
}

void ProcessLoopbackCapture::SetErrorCallback(ErrorCallback cb) {
  std::lock_guard lock(callback_mutex_);
  error_callback_ = std::move(cb);
}

AudioFormat ProcessLoopbackCapture::SourceFormat() const {
  std::lock_guard lock(format_mutex_);
  return source_format_;
}

bool ProcessLoopbackCapture::Start(uint32_t target_pid, RingBuffer& output_ring) {
  Stop();
  target_pid_.store(target_pid);
  ring_ = &output_ring;
  stop_requested_.store(false);
  state_.store(CaptureState::Starting);

  capture_thread_ = std::thread([this]() { CaptureThreadMain(); });
  return true;
}

void ProcessLoopbackCapture::Stop() {
  stop_requested_.store(true);
  if (capture_thread_.joinable()) {
    capture_thread_.join();
  }
  ring_ = nullptr;
  state_.store(CaptureState::Stopped);
}

void ProcessLoopbackCapture::CaptureThreadMain() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool should_uninit = SUCCEEDED(hr);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    state_.store(CaptureState::Failed);
    return;
  }

  DWORD task_index = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Audio", &task_index);

  const bool ok = ActivateAndRun(*ring_);
  if (!ok) {
    state_.store(CaptureState::Failed);
  } else if (!stop_requested_.load()) {
    state_.store(CaptureState::Failed);
  } else {
    state_.store(CaptureState::Stopped);
  }

  if (mmcss) {
    AvRevertMmThreadCharacteristics(mmcss);
  }
  if (should_uninit) {
    CoUninitialize();
  }
}

bool ProcessLoopbackCapture::ActivateAndRun(RingBuffer& output_ring) {
  const uint32_t pid = target_pid_.load();
  LTA_LOG_INFO("Activating process loopback for pid=" + std::to_string(pid));

  AUDIOCLIENT_ACTIVATION_PARAMS activation_params{};
  activation_params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  activation_params.ProcessLoopbackParams.TargetProcessId = pid;
  activation_params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

  PROPVARIANT activate_params{};
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(activation_params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE*>(&activation_params);

  auto* handler = new ActivationCompletionHandler();
  wil::com_ptr_nothrow<IActivateAudioInterfaceAsyncOperation> async_op;
  HRESULT hr = ActivateAudioInterfaceAsync(
      VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
      __uuidof(IAudioClient),
      &activate_params,
      handler,
      &async_op);
  if (FAILED(hr)) {
    handler->Release();
    LTA_LOG_ERROR("ActivateAudioInterfaceAsync failed hr=" + std::to_string(hr));
    std::lock_guard lock(callback_mutex_);
    if (error_callback_) {
      error_callback_("ActivateAudioInterfaceAsync failed");
    }
    return false;
  }

  wil::com_ptr_nothrow<IAudioClient> audio_client;
  hr = handler->Wait(audio_client);
  handler->Release();
  if (FAILED(hr) || !audio_client) {
    LTA_LOG_ERROR("Process loopback activation failed hr=" + std::to_string(hr));
    std::lock_guard lock(callback_mutex_);
    if (error_callback_) {
      error_callback_("Process loopback activation failed");
    }
    return false;
  }

  // Capture at engine mix format — do not assume rate/channels.
  WAVEFORMATEX* mix_format = nullptr;
  hr = audio_client->GetMixFormat(&mix_format);
  if (FAILED(hr) || !mix_format) {
    LTA_LOG_ERROR("GetMixFormat failed");
    return false;
  }
  wil::unique_cotaskmem_ptr<WAVEFORMATEX> mix_format_guard(mix_format);

  {
    std::lock_guard lock(format_mutex_);
    source_format_ = DescribeWaveFormat(mix_format);
    LTA_LOG_INFO("Capture format: " + FormatToString(source_format_));
  }

  constexpr REFERENCE_TIME kBufferDuration = 20 * 10000;  // 20 ms
  hr = audio_client->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
          AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
      kBufferDuration,
      0,
      mix_format,
      nullptr);
  if (FAILED(hr)) {
    LTA_LOG_ERROR("IAudioClient::Initialize failed hr=" + std::to_string(hr));
    return false;
  }

  wil::unique_event_nothrow capture_event;
  capture_event.create();
  hr = audio_client->SetEventHandle(capture_event.get());
  if (FAILED(hr)) {
    return false;
  }

  wil::com_ptr_nothrow<IAudioCaptureClient> capture_client;
  hr = audio_client->GetService(__uuidof(IAudioCaptureClient), capture_client.put_void());
  if (FAILED(hr)) {
    return false;
  }

  hr = audio_client->Start();
  if (FAILED(hr)) {
    return false;
  }

  state_.store(CaptureState::Running);
  auto last_callback = std::chrono::steady_clock::now();
  std::vector<float> float_buf;

  while (!stop_requested_.load()) {
    const DWORD wait = WaitForSingleObject(capture_event.get(), 50);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_TIMEOUT) {
      break;
    }

    const auto now = std::chrono::steady_clock::now();
    const double interval_ms =
        std::chrono::duration<double, std::milli>(now - last_callback).count();
    Diagnostics::Instance().RecordCaptureInterval(interval_ms);
    last_callback = now;

    UINT32 packet_frames = 0;
    hr = capture_client->GetNextPacketSize(&packet_frames);
    if (FAILED(hr)) {
      LTA_LOG_WARN("GetNextPacketSize failed — device/session likely invalidated");
      std::lock_guard lock(callback_mutex_);
      if (error_callback_) {
        error_callback_("Capture session invalidated");
      }
      break;
    }

    while (packet_frames > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      hr = capture_client->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
      if (FAILED(hr)) {
        LTA_LOG_WARN("GetBuffer failed");
        break;
      }

      if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && frames > 0) {
        if (ConvertToFloatInterleaved(data, frames, mix_format, float_buf)) {
          last_rms_.store(ComputeRms(float_buf.data(), float_buf.size()));
          frames_captured_.fetch_add(frames);

          const auto* bytes = reinterpret_cast<const uint8_t*>(float_buf.data());
          const size_t nbytes = float_buf.size() * sizeof(float);
          const size_t written = output_ring.Write(bytes, nbytes);
          if (written < nbytes) {
            const size_t dropped_samples = (nbytes - written) / sizeof(float);
            Diagnostics::Instance().AddDroppedCaptureFrames(dropped_samples);
            Diagnostics::Instance().SetPerformanceWarning(true);
          }
          Diagnostics::Instance().SetRingOccupancy(output_ring.OccupancyPercent());
        }
      } else {
        last_rms_.store(0.0);
      }

      capture_client->ReleaseBuffer(frames);
      hr = capture_client->GetNextPacketSize(&packet_frames);
      if (FAILED(hr)) {
        break;
      }
    }
  }

  audio_client->Stop();
  return true;
}

}  // namespace lta
