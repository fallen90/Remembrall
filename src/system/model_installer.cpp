#include "system/model_installer.h"

#include "system/http_client.h"
#include "system/logger.h"

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace lta {
namespace {

constexpr const wchar_t* kModelUrl =
    L"https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
    L"sherpa-onnx-streaming-zipformer-en-2023-06-26.tar.bz2";

constexpr const char* kRequired[] = {
    "encoder-epoch-99-avg-1.int8.onnx",
    "decoder-epoch-99-avg-1.onnx",
    "joiner-epoch-99-avg-1.int8.onnx",
    "tokens.txt",
};

void Report(const ModelInstaller::ProgressFn& progress, const std::string& msg) {
  LTA_LOG_INFO(msg);
  if (progress) {
    progress(msg);
  }
}

bool RunTarExtract(const std::filesystem::path& archive,
                   const std::filesystem::path& out_dir,
                   std::string& error) {
  // Windows 10+ ships bsdtar as tar.exe — no PowerShell, no execution policy.
  const std::wstring cmd =
      L"tar.exe -xjf \"" + archive.wstring() + L"\" -C \"" + out_dir.wstring() + L"\"";

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi{};

  std::wstring mutable_cmd = cmd;
  if (!CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    error = "Failed to start tar.exe (Windows 10+ required)";
    return false;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (code != 0) {
    error = "tar.exe extract failed (exit " + std::to_string(code) + ")";
    return false;
  }
  return true;
}

std::filesystem::path FindExtractedRoot(const std::filesystem::path& extract_dir) {
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(extract_dir, ec)) {
    if (entry.is_directory()) {
      return entry.path();
    }
  }
  return {};
}

}  // namespace

bool ModelInstaller::IsInstalled(const std::filesystem::path& model_directory) {
  for (const char* name : kRequired) {
    if (!std::filesystem::exists(model_directory / name)) {
      return false;
    }
  }
  return true;
}

bool ModelInstaller::EnsureInstalled(const std::filesystem::path& model_directory,
                                     ProgressFn progress) {
  if (IsInstalled(model_directory)) {
    return true;
  }

  std::error_code ec;
  std::filesystem::create_directories(model_directory, ec);

  const auto temp_root =
      std::filesystem::temp_directory_path(ec) / L"remembrall-model-setup";
  std::filesystem::remove_all(temp_root, ec);
  std::filesystem::create_directories(temp_root, ec);

  const auto archive = temp_root / L"zipformer-en.tar.bz2";
  const auto extract_dir = temp_root / L"extract";
  std::filesystem::create_directories(extract_dir, ec);

  Report(progress, "Downloading speech model (~70 MB)…");
  if (!HttpClient::DownloadToFile(kModelUrl, archive)) {
    Report(progress, "Model download failed — check network and retry");
    return false;
  }

  Report(progress, "Extracting speech model…");
  std::string error;
  if (!RunTarExtract(archive, extract_dir, error)) {
    Report(progress, error);
    return false;
  }

  const auto inner = FindExtractedRoot(extract_dir);
  if (inner.empty()) {
    Report(progress, "Model archive layout unexpected");
    return false;
  }

  for (const char* name : kRequired) {
    const auto src = inner / name;
    const auto dst = model_directory / name;
    if (!std::filesystem::exists(src)) {
      Report(progress, std::string("Missing file in archive: ") + name);
      return false;
    }
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      Report(progress, "Failed to install " + std::string(name));
      return false;
    }
  }

  std::filesystem::remove_all(temp_root, ec);
  Report(progress, "Speech model ready");
  return IsInstalled(model_directory);
}

}  // namespace lta
