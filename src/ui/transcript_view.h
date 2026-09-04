#pragma once

#include "common/types.h"

#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace lta {

class TranscriptView {
 public:
  TranscriptView();

  void SetSegments(std::vector<TranscriptSegment> segments);
  void Render(ID2D1RenderTarget* target,
              IDWriteFactory* dwrite,
              const D2D1_RECT_F& bounds);

 private:
  std::vector<TranscriptSegment> segments_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> body_format_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> partial_format_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> final_brush_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> partial_brush_;
  bool formats_ready_ = false;

  void EnsureResources(ID2D1RenderTarget* target, IDWriteFactory* dwrite);
};

}  // namespace lta
