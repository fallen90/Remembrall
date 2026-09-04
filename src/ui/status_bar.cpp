#include "ui/status_bar.h"

#include "common/types.h"

#include <sstream>

namespace lta {

void StatusBarView::Ensure(ID2D1RenderTarget* target, IDWriteFactory* dwrite) {
  if (ready_) {
    return;
  }
  dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                           13.0f, L"en-us", &format_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.75f, 0.78f, 0.82f), &text_brush_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.11f, 0.13f), &bg_brush_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.78f, 0.45f), &dot_ok_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.35f, 0.30f), &dot_bad_);
  ready_ = true;
}

void StatusBarView::Render(ID2D1RenderTarget* target,
                           IDWriteFactory* dwrite,
                           const D2D1_RECT_F& bounds,
                           const DiagnosticsSnapshot& snap,
                           bool discord_connected) {
  Ensure(target, dwrite);
  target->FillRectangle(bounds, bg_brush_.Get());

  std::wostringstream oss;
  const char* state = ToString(snap.app_state);
  while (*state) {
    oss << static_cast<wchar_t>(*state++);
  }
  oss << L"     English     "
      << static_cast<int>(snap.partial_latency_ms + 0.5) << L" ms     "
      << (snap.working_set_bytes / (1024 * 1024)) << L" MB";
  if (snap.performance_warning) {
    oss << L"     ! lag";
  }

  const auto text = oss.str();
  target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format_.Get(),
                    D2D1::RectF(bounds.left + 16.0f, bounds.top + 8.0f, bounds.right - 16.0f,
                                bounds.bottom - 4.0f),
                    text_brush_.Get());

  // Discord indicator drawn by main window title area; keep status bar text-only.
  (void)discord_connected;
}

}  // namespace lta
