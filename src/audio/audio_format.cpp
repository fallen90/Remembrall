#include "audio/audio_format.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <initguid.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

namespace lta {

AudioFormat DescribeWaveFormat(const WAVEFORMATEX* format) {
  AudioFormat af;
  if (!format) {
    return af;
  }
  af.sample_rate = format->nSamplesPerSec;
  af.channels = format->nChannels;
  af.bits_per_sample = format->wBitsPerSample;

  if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    af.is_float = true;
  } else if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    af.is_float = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
  }
  return af;
}

std::string FormatToString(const AudioFormat& format) {
  std::ostringstream oss;
  oss << format.sample_rate << " Hz, " << format.channels << " ch, "
      << format.bits_per_sample << "-bit " << (format.is_float ? "float" : "PCM");
  return oss.str();
}

bool ConvertToFloatInterleaved(const BYTE* data,
                               UINT32 frames,
                               const WAVEFORMATEX* format,
                               std::vector<float>& out_interleaved) {
  if (!data || !format || frames == 0) {
    return false;
  }

  const auto channels = format->nChannels;
  out_interleaved.resize(static_cast<size_t>(frames) * channels);

  bool is_float = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
  WORD bits = format->wBitsPerSample;
  if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    is_float = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    bits = ext->Format.wBitsPerSample;
  }

  if (is_float && bits == 32) {
    const auto* src = reinterpret_cast<const float*>(data);
    std::copy(src, src + out_interleaved.size(), out_interleaved.begin());
    return true;
  }

  if (!is_float && bits == 16) {
    const auto* src = reinterpret_cast<const int16_t*>(data);
    for (size_t i = 0; i < out_interleaved.size(); ++i) {
      out_interleaved[i] = static_cast<float>(src[i]) / 32768.0f;
    }
    return true;
  }

  if (!is_float && bits == 32) {
    const auto* src = reinterpret_cast<const int32_t*>(data);
    for (size_t i = 0; i < out_interleaved.size(); ++i) {
      out_interleaved[i] = static_cast<float>(src[i]) / 2147483648.0f;
    }
    return true;
  }

  if (!is_float && bits == 24) {
    for (UINT32 f = 0; f < frames; ++f) {
      for (WORD c = 0; c < channels; ++c) {
        const size_t idx = static_cast<size_t>(f) * channels + c;
        const BYTE* p = data + idx * 3;
        int32_t sample = (static_cast<int32_t>(p[2]) << 16) |
                         (static_cast<int32_t>(p[1]) << 8) |
                         static_cast<int32_t>(p[0]);
        if (sample & 0x800000) {
          sample |= ~0xFFFFFF;
        }
        out_interleaved[idx] = static_cast<float>(sample) / 8388608.0f;
      }
    }
    return true;
  }

  return false;
}

}  // namespace lta
