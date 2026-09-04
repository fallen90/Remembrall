# Configure and build on Windows with Visual Studio 2022 + Windows SDK >= 10.0.20348
# Usage (Developer PowerShell):
#   .\scripts\build.ps1
#   .\scripts\build.ps1 -Config RelWithDebInfo

param(
  [ValidateSet("Debug", "Release", "RelWithDebInfo")]
  [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "build"

# Models are downloaded by Remembrall.exe on first launch; optional for local build.
& (Join-Path $PSScriptRoot "download-sherpa.ps1")

$SherpaRoot = Join-Path $Root "third_party\sherpa-onnx"
cmake -S $Root -B $Build -G "Visual Studio 17 2022" -A x64 `
  "-DLTA_SHERPA_ROOT=$SherpaRoot" `
  -DLTA_FETCH_SHERPA=OFF
cmake --build $Build --config $Config --parallel

Write-Host "Built: $Build\$Config\Remembrall.exe"
Write-Host "POC:   $Build\$Config\remembrall-capture-poc.exe"
Write-Host "Tests: ctest --test-dir $Build -C $Config"
