#pragma once

#include <functional>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace lta {

// Listens for default render device changes and session-relevant device events.
class DeviceChangeMonitor : public IMMNotificationClient {
 public:
  using Callback = std::function<void()>;

  DeviceChangeMonitor();
  ~DeviceChangeMonitor();

  DeviceChangeMonitor(const DeviceChangeMonitor&) = delete;
  DeviceChangeMonitor& operator=(const DeviceChangeMonitor&) = delete;

  bool Start(Callback on_change);
  void Stop();

  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  // IMMNotificationClient
  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override;
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override;
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override;
  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override;
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override;

 private:
  void Fire();

  LONG ref_ = 1;
  std::mutex mutex_;
  Callback callback_;
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
  bool registered_ = false;
};

}  // namespace lta
