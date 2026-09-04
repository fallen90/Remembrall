#include "ui/main_window.h"

#include "common/branding.h"
#include "resource.h"
#include "system/auto_updater.h"
#include "system/diagnostics.h"
#include "system/logger.h"
#include "system/version.h"

#include <dwmapi.h>
#include <string>

namespace lta {
namespace {

constexpr int kUiTimerMs = 100;
constexpr UINT kMsgUpdateState = WM_APP + 40;

std::wstring Widen(const std::string& s) {
  std::wstring out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    out.push_back(static_cast<wchar_t>(c));
  }
  return out;
}

}  // namespace

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
  if (hwnd_) {
    KillTimer(hwnd_, timer_id_);
  }
  DiscardDeviceResources();
}

bool MainWindow::Create(HINSTANCE instance, Application* app) {
  instance_ = instance;
  app_ = app;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
  wc.hIconSm = reinterpret_cast<HICON>(LoadImageW(
      instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = branding::kWindowClass;
  RegisterClassExW(&wc);

  std::wstring title = branding::kAppNameW;
  title += L"  v";
  title += Widen(CurrentVersionString());

  hwnd_ = CreateWindowExW(
      WS_EX_APPWINDOW,
      branding::kWindowClass,
      title.c_str(),
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 900, 560,
      nullptr, nullptr, instance, this);

  if (!hwnd_) {
    return false;
  }

  if (wc.hIcon) {
    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
  }
  if (wc.hIconSm) {
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIconSm));
  }

  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.GetAddressOf());
  DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                      reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));

  app_->Updater().SetStateCallback([this](UpdateState, const UpdateInfo&, const std::string&) {
    update_event_pending_.store(true);
    if (hwnd_) {
      PostMessageW(hwnd_, kMsgUpdateState, 0, 0);
    }
  });

  // If startup check already found an update before the window existed, re-notify.
  if (app_->Updater().State() == UpdateState::Available ||
      app_->Updater().State() == UpdateState::ReadyToApply) {
    PostMessageW(hwnd_, kMsgUpdateState, 0, 0);
  }

  ApplyAlwaysOnTop();
  ShowWindow(hwnd_, app_->Config().start_minimized ? SW_SHOWMINIMIZED : SW_SHOW);
  UpdateWindow(hwnd_);
  SetTimer(hwnd_, timer_id_, kUiTimerMs, nullptr);
  return true;
}

int MainWindow::RunMessageLoop() {
  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  MainWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<MainWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_SIZE:
      if (render_target_) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        render_target_->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
      }
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    case WM_DISPLAYCHANGE:
      DiscardDeviceResources();
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    case WM_PAINT:
      OnPaint();
      return 0;
    case WM_TIMER:
      OnTimer();
      return 0;
    case kMsgUpdateState:
      OnUpdateStateChanged();
      return 0;
    case WM_KEYDOWN:
      if (wparam == 'D' && (GetKeyState(VK_CONTROL) & 0x8000) &&
          (GetKeyState(VK_SHIFT) & 0x8000)) {
        diagnostics_view_.Toggle();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
      }
      if (wparam == VK_F2) {
        ShowSettingsMenu();
        return 0;
      }
      break;
    case WM_DESTROY:
      KillTimer(hwnd_, timer_id_);
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

bool MainWindow::InitDeviceResources() {
  if (render_target_) {
    return true;
  }
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  const D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  HRESULT hr = d2d_factory_->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(hwnd_, size),
      &render_target_);
  if (FAILED(hr)) {
    return false;
  }
  render_target_->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.10f, 0.12f), &bg_brush_);
  render_target_->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.96f, 0.98f), &title_brush_);
  render_target_->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.75f, 0.45f), &accent_brush_);
  dwrite_factory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                    16.0f, L"en-us", &title_format_);
  return true;
}

void MainWindow::DiscardDeviceResources() {
  render_target_.Reset();
  bg_brush_.Reset();
  title_brush_.Reset();
  accent_brush_.Reset();
  title_format_.Reset();
}

void MainWindow::OnTimer() {
  if (!app_) {
    return;
  }
  transcript_view_.SetSegments(app_->Transcripts().GetSegments());
  if (update_event_pending_.exchange(false)) {
    OnUpdateStateChanged();
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::OnUpdateStateChanged() {
  if (!app_) {
    return;
  }
  const auto state = app_->Updater().State();
  if (state == UpdateState::Available) {
    if (app_->Config().auto_download_updates) {
      app_->Updater().DownloadAndStageAsync();
    } else {
      PromptIfUpdateAvailable();
    }
  } else if (state == UpdateState::ReadyToApply) {
    PromptIfUpdateReady();
  } else if (state == UpdateState::UpToDate && manual_update_check_) {
    manual_update_check_ = false;
    MessageBoxW(hwnd_, L"You're on the latest version.", L"Check for updates",
                MB_OK | MB_ICONINFORMATION);
  } else if (state == UpdateState::Failed && manual_update_check_) {
    manual_update_check_ = false;
    const auto msg = Widen(app_->Updater().LastMessage());
    MessageBoxW(hwnd_, msg.c_str(), L"Update check failed", MB_OK | MB_ICONWARNING);
  } else if (state == UpdateState::Failed) {
    LTA_LOG_WARN("Updater: " + app_->Updater().LastMessage());
  }
}

void MainWindow::PromptIfUpdateAvailable() {
  if (update_prompt_shown_ || !app_) {
    return;
  }
  update_prompt_shown_ = true;
  const auto info = app_->Updater().Info();
  std::wstring msg = L"Version ";
  msg += Widen(info.remote_version.ToString());
  msg += L" is available (you have ";
  msg += Widen(CurrentVersionString());
  msg += L").\n\nDownload and install now?";
  const int choice = MessageBoxW(hwnd_, msg.c_str(), L"Update available",
                                 MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
  if (choice == IDYES) {
    app_->Updater().DownloadAndStageAsync();
  }
}

void MainWindow::PromptIfUpdateReady() {
  if (update_ready_prompt_shown_ || !app_) {
    return;
  }
  update_ready_prompt_shown_ = true;
  const int choice = MessageBoxW(
      hwnd_,
      L"Update downloaded. Restart Remembrall to apply it now?",
      L"Restart to update",
      MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
  if (choice == IDYES) {
    if (app_->ApplyPendingUpdate()) {
      DestroyWindow(hwnd_);
    }
  }
}

void MainWindow::OnPaint() {
  PAINTSTRUCT ps{};
  BeginPaint(hwnd_, &ps);
  if (!InitDeviceResources()) {
    EndPaint(hwnd_, &ps);
    return;
  }

  render_target_->BeginDraw();
  render_target_->Clear(D2D1::ColorF(0.09f, 0.10f, 0.12f));

  const auto size = render_target_->GetSize();
  constexpr float kTitleH = 44.0f;
  constexpr float kStatusH = 36.0f;

  std::wstring heading = branding::kAppNameW;
  render_target_->DrawTextW(heading.c_str(), static_cast<UINT32>(heading.size()), title_format_.Get(),
                            D2D1::RectF(16, 12, 280, kTitleH), title_brush_.Get());

  const auto snap = Diagnostics::Instance().Snapshot();
  const bool connected = snap.discord_pid != 0 &&
                         (snap.app_state == AppState::Capturing ||
                          snap.app_state == AppState::Transcribing ||
                          snap.app_state == AppState::WaitingForAudio ||
                          snap.app_state == AppState::DiscordFound);
  auto* dot = connected ? accent_brush_.Get() : title_brush_.Get();
  render_target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(size.width - 96, 22), 5, 5), dot);
  render_target_->DrawTextW(L"Discord", 7, title_format_.Get(),
                            D2D1::RectF(size.width - 84, 12, size.width - 12, kTitleH),
                            title_brush_.Get());

  transcript_view_.Render(render_target_.Get(), dwrite_factory_.Get(),
                          D2D1::RectF(0, kTitleH, size.width, size.height - kStatusH));

  status_bar_.Render(render_target_.Get(), dwrite_factory_.Get(),
                     D2D1::RectF(0, size.height - kStatusH, size.width, size.height),
                     snap, connected);

  if (diagnostics_view_.Visible()) {
    diagnostics_view_.Render(render_target_.Get(), dwrite_factory_.Get(),
                             D2D1::RectF(size.width * 0.45f, kTitleH, size.width - 8,
                                         size.height - kStatusH - 8),
                             snap);
  }

  const HRESULT hr = render_target_->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    DiscardDeviceResources();
  }
  EndPaint(hwnd_, &ps);
}

void MainWindow::ApplyAlwaysOnTop() {
  if (!app_) {
    return;
  }
  SetWindowPos(hwnd_, app_->Config().always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
               0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void MainWindow::ShowSettingsMenu() {
  if (!app_) {
    return;
  }
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING | (app_->Config().always_on_top ? MF_CHECKED : 0), 1,
              L"Always on top");
  AppendMenuW(menu, MF_STRING | (app_->Config().start_minimized ? MF_CHECKED : 0), 2,
              L"Start minimized");
  AppendMenuW(menu, MF_STRING | (app_->Config().launch_with_windows ? MF_CHECKED : 0), 3,
              L"Launch with Windows");
  AppendMenuW(menu, MF_STRING | (app_->Config().show_partial_results ? MF_CHECKED : 0), 4,
              L"Show partial results");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (app_->Config().check_for_updates ? MF_CHECKED : 0), 5,
              L"Check for updates automatically");
  AppendMenuW(menu, MF_STRING | (app_->Config().auto_download_updates ? MF_CHECKED : 0), 6,
              L"Auto-download updates");
  AppendMenuW(menu, MF_STRING, 7, L"Check for updates now…");

  std::wstring about = L"Version ";
  about += Widen(CurrentVersionString());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | MF_GRAYED, 8, about.c_str());

  POINT pt{};
  GetCursorPos(&pt);
  const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  auto& cfg = app_->Config();
  switch (cmd) {
    case 1:
      cfg.always_on_top = !cfg.always_on_top;
      ApplyAlwaysOnTop();
      break;
    case 2:
      cfg.start_minimized = !cfg.start_minimized;
      break;
    case 3:
      cfg.launch_with_windows = !cfg.launch_with_windows;
      cfg.ApplyLaunchWithWindows();
      break;
    case 4:
      cfg.show_partial_results = !cfg.show_partial_results;
      break;
    case 5:
      cfg.check_for_updates = !cfg.check_for_updates;
      app_->Updater().SetEnabled(cfg.check_for_updates);
      break;
    case 6:
      cfg.auto_download_updates = !cfg.auto_download_updates;
      break;
    case 7:
      update_prompt_shown_ = false;
      update_ready_prompt_shown_ = false;
      manual_update_check_ = true;
      app_->CheckForUpdates(true);
      break;
    default:
      return;
  }
  cfg.SaveToFile(cfg.settings_path);
}

}  // namespace lta
