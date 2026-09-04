#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace lta {

// Ensures the default English Zipformer INT8 model is present under
// %LOCALAPPDATA%\Remembrall\models\zipformer-en (or the given directory).
class ModelInstaller {
 public:
  using ProgressFn = std::function<void(const std::string& message)>;

  static bool IsInstalled(const std::filesystem::path& model_directory);

  // Downloads + extracts if missing. Blocking; call from a worker thread.
  static bool EnsureInstalled(const std::filesystem::path& model_directory,
                              ProgressFn progress = {});
};

}  // namespace lta
