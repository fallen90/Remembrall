#include "audio/device_change_monitor.h"

#include "system/logger.h"

namespace lta {

DeviceChangeMonitor::DeviceChangeMonitor() = default;

DeviceChangeMonitor::~DeviceChangeMonitor() {
  Stop();
}

bool DeviceChangeMonitor::Start(Callback on_change) {
  Stop();
  {
    std::lock_guard lock(mutex_);
    callback_ = std::move(on_change);
  }

  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), &enumerator_);
  if (FAILED(hr)) {
    LTA_LOG_ERROR("MMDeviceEnumerator create failed");
    return false;
  }
  hr = enumerator_->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    LTA_LOG_ERROR("RegisterEndpointNotificationCallback failed");
    return false;
  }
  registered_ = true;
  LTA_LOG_INFO("Device change monitor started");
  return true;
}

void DeviceChangeMonitor::Stop() {
  if (registered_ && enumerator_) {
    enumerator_->UnregisterEndpointNotificationCallback(this);
    registered_ = false;
  }
  enumerator_.Reset();
  std::lock_guard lock(mutex_);
  callback_ = nullptr;
}

void DeviceChangeMonitor::Fire() {
  Callback cb;
  {
    std::lock_guard lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb();
  }
}

HRESULT DeviceChangeMonitor::QueryInterface(REFIID riid, void** ppv) {
  if (!ppv) {
    return E_POINTER;
  }
  if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
    *ppv = static_cast<IMMNotificationClient*>(this);
    AddRef();
    return S_OK;
  }
  *ppv = nullptr;
  return E_NOINTERFACE;
}

ULONG DeviceChangeMonitor::AddRef() {
  return InterlockedIncrement(&ref_);
}

ULONG DeviceChangeMonitor::Release() {
  // Owned by Application stack/member — do not delete from Release.
  return InterlockedDecrement(&ref_);
}

HRESULT DeviceChangeMonitor::OnDeviceStateChanged(LPCWSTR, DWORD) {
  Fire();
  return S_OK;
}

HRESULT DeviceChangeMonitor::OnDeviceAdded(LPCWSTR) {
  return S_OK;
}

HRESULT DeviceChangeMonitor::OnDeviceRemoved(LPCWSTR) {
  Fire();
  return S_OK;
}

HRESULT DeviceChangeMonitor::OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR) {
  if (flow == eRender) {
    LTA_LOG_INFO("Default render device changed");
    Fire();
  }
  return S_OK;
}

HRESULT DeviceChangeMonitor::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) {
  return S_OK;
}

}  // namespace lta
