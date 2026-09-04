#include "audio/audio_format.h"
#include "audio/resampler.h"

#include <cmath>
#include <iostream>
#include <vector>

static int g_failures = 0;
#define EXPECT_TRUE(x) do { if (!(x)) { std::cerr << "FAIL " << #x << " @ " << __LINE__ << "\n"; ++g_failures; } } while (0)

int RunAudioFormatTests() {
  // 48 kHz stereo -> 16 kHz mono should yield ~1/3 frames.
  lta::Resampler rs;
  EXPECT_TRUE(rs.Configure(48000, 2));

  constexpr size_t frames = 480;  // 10 ms
  std::vector<float> stereo(frames * 2);
  for (size_t i = 0; i < frames; ++i) {
    const float s = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 48000.0f);
    stereo[i * 2] = s;
    stereo[i * 2 + 1] = s;
  }

  std::vector<float> out;
  EXPECT_TRUE(rs.Process(stereo.data(), frames, out));
  // Expect roughly 160 samples (+/- a few due to phase carry).
  EXPECT_TRUE(out.size() >= 140 && out.size() <= 180);

  lta::AudioFormat af;
  af.sample_rate = 48000;
  af.channels = 2;
  af.bits_per_sample = 32;
  af.is_float = true;
  EXPECT_TRUE(af.Valid());
  EXPECT_TRUE(af.BytesPerFrame() == 8);
  return g_failures;
}
