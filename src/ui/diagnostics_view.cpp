#include "ui/diagnostics_view.h"

#include <sstream>

namespace lta {

void DiagnosticsView::Ensure(ID2D1RenderTarget* target, IDWriteFactory* dwrite) {
  if (ready_) {
    return;
  }
  dwrite->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                           12.0f, L"en-us", &format_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.05f, 0.06f, 0.08f, 0.92f), &bg_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.80f, 0.90f, 0.70f), &text_);
  ready_ = true;
}

void DiagnosticsView::Render(ID2D1RenderTarget* target,
                             IDWriteFactory* dwrite,
                             const D2D1_RECT_F& bounds,
                             const DiagnosticsSnapshot& snap) {
  if (!visible_) {
    return;
  }
  Ensure(target, dwrite);
  target->FillRectangle(bounds, bg_.Get());

  std::wostringstream oss;
  oss << L"Diagnostics (Ctrl+Shift+D)\n"
      << L"PID: " << snap.discord_pid << L"\n"
      << L"Capture interval: " << snap.capture_callback_interval_ms << L" ms\n"
      << L"Ring occupancy: " << snap.ring_occupancy_pct << L" %\n"
      << L"ASR queue: " << snap.asr_queue_occupancy_pct << L" %\n"
      << L"Resample: " << snap.resample_duration_ms << L" ms\n"
      << L"ASR inference: " << snap.asr_inference_ms << L" ms\n"
      << L"RTF: " << snap.rtf << L"\n"
      << L"Partial latency: " << snap.partial_latency_ms << L" ms\n"
      << L"Endpoint latency: " << snap.endpoint_latency_ms << L" ms\n"
      << L"Dropped capture: " << snap.dropped_capture_frames << L"\n"
      << L"Dropped ASR: " << snap.dropped_asr_frames << L"\n"
      << L"Working set: " << (snap.working_set_bytes / (1024 * 1024)) << L" MB\n"
      << L"Perf warning: " << (snap.performance_warning ? L"yes" : L"no") << L"\n";
  if (!snap.last_error.empty()) {
    oss << L"Last error: ";
    for (char c : snap.last_error) {
      oss << static_cast<wchar_t>(c);
    }
    oss << L"\n";
  }

  const auto text = oss.str();
  target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format_.Get(),
                    D2D1::RectF(bounds.left + 12, bounds.top + 12, bounds.right - 12,
                                bounds.bottom - 12),
                    text_.Get());
}

}  // namespace lta
