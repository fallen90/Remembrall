#include "app/application.h"
#include "common/branding.h"
#include "config/configuration.h"
#include "system/logger.h"
#include "ui/main_window.h"

#include <string>

#include <objbase.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    return 1;
  }

  lta::log::Initialize(lta::DefaultLogDirectory().wstring());
  LTA_LOG_INFO(std::string(lta::branding::kAppName) + " starting");

  auto config = lta::Configuration::LoadFromFile(lta::DefaultSettingsPath());
  config.ApplyLaunchWithWindows();

  lta::Application app;
  if (!app.Initialize(config)) {
    LTA_LOG_ERROR("Application initialize failed");
  }
  app.Start();

  lta::MainWindow window;
  if (!window.Create(instance, &app)) {
    LTA_LOG_ERROR("Failed to create main window");
    app.Shutdown();
    lta::log::Shutdown();
    CoUninitialize();
    return 1;
  }

  const int code = window.RunMessageLoop();
  app.Shutdown();
  lta::log::Shutdown();
  CoUninitialize();
  return code;
}
