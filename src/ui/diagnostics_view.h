#pragma once

#include "system/diagnostics.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace lta {

class DiagnosticsView {
 public:
  void SetVisible(bool visible) { visible_ = visible; }
  [[nodiscard]] bool Visible() const { return visible_; }
  void Toggle() { visible_ = !visible_; }

  void Render(ID2D1RenderTarget* target,
              IDWriteFactory* dwrite,
              const D2D1_RECT_F& bounds,
              const DiagnosticsSnapshot& snap);

 private:
  bool visible_ = false;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text_;
  bool ready_ = false;
  void Ensure(ID2D1RenderTarget* target, IDWriteFactory* dwrite);
};

}  // namespace lta
