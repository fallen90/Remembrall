#include "audio/discord_locator.h"
#include "audio/process_loopback.h"
#include "audio/ring_buffer.h"
#include "common/branding.h"
#include "system/logger.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>

int wmain() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    std::wcerr << L"COM init failed\n";
    return 1;
  }

  lta::log::Initialize(L".");
  std::wcout << lta::branding::kAppNameW << L" — Capture POC\n";
  std::wcout << L"Searching for Discord...\n";

  lta::DiscordLocator locator;
  std::optional<lta::DiscordProcess> discord;
  for (int i = 0; i < 60 && !discord; ++i) {
    discord = locator.Locate();
    if (!discord) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  if (!discord) {
    std::wcerr << L"Discord not found.\n";
    return 2;
  }

  std::wcout << L"Found " << discord->exe_name << L" pid=" << discord->pid
             << L" audio_session=" << (discord->has_active_audio_session ? L"yes" : L"no")
             << L"\n";

  lta::RingBuffer ring(48000 * 2 * 4);
  lta::ProcessLoopbackCapture capture;
  capture.Start(discord->pid, ring);

  using clock = std::chrono::steady_clock;
  const auto end = clock::now() + std::chrono::seconds(30);
  while (clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const double rms = capture.LastRms();
    const double db = rms > 1e-9 ? 20.0 * std::log10(rms) : -100.0;
    std::wcout << L"\rframes=" << capture.FramesCaptured()
               << L"  rms=" << rms
               << L"  dBFS~" << db
               << L"  ring%=" << ring.OccupancyPercent()
               << L"   " << std::flush;
  }
  std::wcout << L"\nDone.\n";
  capture.Stop();
  lta::log::Shutdown();
  CoUninitialize();
  return 0;
}
