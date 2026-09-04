#pragma once

#include "app/application.h"
#include "ui/diagnostics_view.h"
#include "ui/status_bar.h"
#include "ui/transcript_view.h"

#include <memory>
#include <atomic>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace lta {

class MainWindow {
 public:
  MainWindow();
  ~MainWindow();

  bool Create(HINSTANCE instance, Application* app);
  int RunMessageLoop();
  [[nodiscard]] HWND Handle() const { return hwnd_; }

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  bool InitDeviceResources();
  void DiscardDeviceResources();
  void OnPaint();
  void OnTimer();
  void ApplyAlwaysOnTop();
  void ShowSettingsMenu();
  void OnUpdateStateChanged();
  void PromptIfUpdateAvailable();
  void PromptIfUpdateReady();

  HINSTANCE instance_ = nullptr;
  HWND hwnd_ = nullptr;
  Application* app_ = nullptr;

  Microsoft::WRL::ComPtr<ID2D1Factory> d2d_factory_;
  Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory_;
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg_brush_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> title_brush_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accent_brush_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> title_format_;

  TranscriptView transcript_view_;
  StatusBarView status_bar_;
  DiagnosticsView diagnostics_view_;

  UINT_PTR timer_id_ = 1;
  bool update_prompt_shown_ = false;
  bool update_ready_prompt_shown_ = false;
  bool manual_update_check_ = false;
  std::atomic<bool> update_event_pending_{false};
};

}  // namespace lta
