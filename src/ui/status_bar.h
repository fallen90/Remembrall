#pragma once

#include "system/diagnostics.h"

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace lta {

class StatusBarView {
 public:
  void Render(ID2D1RenderTarget* target,
              IDWriteFactory* dwrite,
              const D2D1_RECT_F& bounds,
              const DiagnosticsSnapshot& snap,
              bool discord_connected);

 private:
  Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text_brush_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg_brush_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> dot_ok_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> dot_bad_;
  bool ready_ = false;

  void Ensure(ID2D1RenderTarget* target, IDWriteFactory* dwrite);
};

}  // namespace lta
