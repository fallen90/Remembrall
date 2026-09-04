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

Write-Host "[$(Get-Date -Format o)] Downloading $Url"
# Explicit timeouts — GHA Windows runners can hang forever without --max-time.
& curl.exe -L --retry 5 --retry-all-errors --connect-timeout 30 --max-time 180 `
  -o $Archive $Url
if ($LASTEXITCODE -ne 0) { throw "curl failed with exit $LASTEXITCODE" }
$bytes = (Get-Item $Archive).Length
Write-Host "[$(Get-Date -Format o)] Downloaded $bytes bytes"

if (Test-Path $Extract) { Remove-Item -Recurse -Force $Extract }
New-Item -ItemType Directory -Force -Path $Extract | Out-Null

# Windows bsdtar often stalls on .tar.bz2; Python's tarfile is reliable.
Write-Host "[$(Get-Date -Format o)] Extracting with Python tarfile"
$py = @"
import tarfile, sys
with tarfile.open(r'''$Archive''', 'r:bz2') as t:
    t.extractall(r'''$Extract''')
print('extracted ok')
"@
python -c $py
if ($LASTEXITCODE -ne 0) { throw "python extract failed with exit $LASTEXITCODE" }
Write-Host "[$(Get-Date -Format o)] Extract complete"

$Inner = Get-ChildItem $Extract -Directory | Select-Object -First 1
if (-not $Inner) { throw "Unexpected archive layout for $Asset" }

New-Item -ItemType Directory -Force -Path (Split-Path $DestRoot) | Out-Null
if (Test-Path $DestRoot) { Remove-Item -Recurse -Force $DestRoot }
Move-Item $Inner.FullName $DestRoot

if (-not (Test-Path $Marker)) {
  throw "Download succeeded but cxx-api.h missing under $DestRoot"
}

Write-Host "[$(Get-Date -Format o)] Installed sherpa-onnx $Version -> $DestRoot"
