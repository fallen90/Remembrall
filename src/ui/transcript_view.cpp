#include "ui/transcript_view.h"

#include <algorithm>
#include <sstream>

namespace lta {

TranscriptView::TranscriptView() = default;

void TranscriptView::SetSegments(std::vector<TranscriptSegment> segments) {
  segments_ = std::move(segments);
}

void TranscriptView::EnsureResources(ID2D1RenderTarget* target, IDWriteFactory* dwrite) {
  if (formats_ready_) {
    return;
  }
  dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                           18.0f, L"en-us", &body_format_);
  dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                           DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_NORMAL,
                           18.0f, L"en-us", &partial_format_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.93f, 0.95f), &final_brush_);
  target->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.72f, 0.95f, 0.85f), &partial_brush_);
  formats_ready_ = true;
}

void TranscriptView::Render(ID2D1RenderTarget* target,
                            IDWriteFactory* dwrite,
                            const D2D1_RECT_F& bounds) {
  EnsureResources(target, dwrite);
  float y = bounds.top + 12.0f;
  const float line_height = 28.0f;
  const float max_y = bounds.bottom - 8.0f;

  // Show newest lines that fit; walk from end.
  std::vector<const TranscriptSegment*> visible;
  float needed = 0;
  for (auto it = segments_.rbegin(); it != segments_.rend(); ++it) {
    needed += line_height;
    visible.push_back(&(*it));
    if (y + needed > max_y) {
      break;
    }
  }
  std::reverse(visible.begin(), visible.end());

  for (const auto* seg : visible) {
    if (y + line_height > max_y) {
      break;
    }
    std::wstring text;
    text.reserve(seg->text.size());
    for (unsigned char c : seg->text) {
      text.push_back(static_cast<wchar_t>(c));
    }
    if (seg->state == SegmentState::Partial) {
      text.push_back(L' ');
      text.push_back(L'|');
    }

    const D2D1_RECT_F rect = D2D1::RectF(bounds.left + 16.0f, y, bounds.right - 16.0f, y + line_height);
    auto* format = seg->state == SegmentState::Partial ? partial_format_.Get() : body_format_.Get();
    auto* brush = seg->state == SegmentState::Partial ? partial_brush_.Get() : final_brush_.Get();
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, brush);
    y += line_height;
  }
}

}  // namespace lta
