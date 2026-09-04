# Download official sherpa-onnx Windows x64 shared /MD Release (no TTS) prebuilts.
# Layout: third_party/sherpa-onnx/{include,lib}
param(
  [string]$Version = "1.12.23",
  [string]$DestRoot = ""
)

$ErrorActionPreference = "Stop"
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
Invoke-WebRequest -Uri $Url -OutFile $Archive -UseBasicParsing

if (Test-Path $Extract) { Remove-Item -Recurse -Force $Extract }
New-Item -ItemType Directory -Force -Path $Extract | Out-Null

# tar.exe on Windows 10+ handles .tar.bz2
tar -xjf $Archive -C $Extract
$Inner = Get-ChildItem $Extract -Directory | Select-Object -First 1
if (-not $Inner) { throw "Unexpected archive layout for $Asset" }

New-Item -ItemType Directory -Force -Path (Split-Path $DestRoot) | Out-Null
if (Test-Path $DestRoot) { Remove-Item -Recurse -Force $DestRoot }
Move-Item $Inner.FullName $DestRoot

if (-not (Test-Path $Marker)) {
  throw "Download succeeded but cxx-api.h missing under $DestRoot"
}

Write-Host "Installed sherpa-onnx $Version -> $DestRoot"
