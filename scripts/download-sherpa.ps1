# Download official sherpa-onnx Windows x64 shared /MD Release (no TTS) prebuilts.
# Layout: third_party/sherpa-onnx/{include,lib}
param(
  [string]$Version = "1.12.23",
  [string]$DestRoot = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $DestRoot) {
  $DestRoot = Join-Path $RepoRoot "third_party\sherpa-onnx"
}

$Asset = "sherpa-onnx-v$Version-win-x64-shared-MD-Release-no-tts.tar.bz2"
$Url = "https://github.com/k2-fsa/sherpa-onnx/releases/download/v$Version/$Asset"
$Marker = Join-Path $DestRoot "include\sherpa-onnx\c-api\cxx-api.h"

if (Test-Path $Marker) {
  Write-Host "sherpa-onnx already present at $DestRoot"
  exit 0
}

$Temp = Join-Path $env:TEMP "remembrall-sherpa-$Version"
New-Item -ItemType Directory -Force -Path $Temp | Out-Null
$Archive = Join-Path $Temp $Asset
$Extract = Join-Path $Temp "extract"

Write-Host "Downloading $Url"
# curl.exe is far more reliable than Invoke-WebRequest on GitHub Actions for large assets.
& curl.exe -fsSL --retry 5 --retry-delay 2 -o $Archive $Url
if ($LASTEXITCODE -ne 0) { throw "curl failed with exit $LASTEXITCODE" }

if (Test-Path $Extract) { Remove-Item -Recurse -Force $Extract }
New-Item -ItemType Directory -Force -Path $Extract | Out-Null

Write-Host "Extracting $Archive"
tar.exe -xjf $Archive -C $Extract
if ($LASTEXITCODE -ne 0) { throw "tar extract failed with exit $LASTEXITCODE" }

$Inner = Get-ChildItem $Extract -Directory | Select-Object -First 1
if (-not $Inner) { throw "Unexpected archive layout for $Asset" }

New-Item -ItemType Directory -Force -Path (Split-Path $DestRoot) | Out-Null
if (Test-Path $DestRoot) { Remove-Item -Recurse -Force $DestRoot }
Move-Item $Inner.FullName $DestRoot

if (-not (Test-Path $Marker)) {
  throw "Download succeeded but cxx-api.h missing under $DestRoot"
}

Write-Host "Installed sherpa-onnx $Version -> $DestRoot"
