#pragma once

#include "common/types.h"

#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmreg.h>
#include <Audioclient.h>

namespace lta {

AudioFormat DescribeWaveFormat(const WAVEFORMATEX* format);
std::string FormatToString(const AudioFormat& format);

// Convert a WASAPI packet (any PCM/float mix format) into interleaved float32 samples.
bool ConvertToFloatInterleaved(const BYTE* data,
                               UINT32 frames,
                               const WAVEFORMATEX* format,
                               std::vector<float>& out_interleaved);

}  // namespace lta
