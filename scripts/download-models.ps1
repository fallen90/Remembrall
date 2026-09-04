# Developer helper only — Remembrall downloads models itself on first launch.
# Prefer just running Remembrall.exe. Use this for offline/CI prep if needed.
#
#   .\scripts\download-models.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $Root "models\zipformer-en"
$Url = "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-en-2023-06-26.tar.bz2"
$Archive = Join-Path $env:TEMP "sherpa-onnx-streaming-zipformer-en-2023-06-26.tar.bz2"

New-Item -ItemType Directory -Force -Path $Dest | Out-Null

Write-Host "Downloading $Url"
& curl.exe -L --retry 5 --connect-timeout 30 --max-time 600 -o $Archive $Url
if ($LASTEXITCODE -ne 0) { throw "download failed" }

Write-Host "Extracting..."
$ExtractDir = Join-Path $env:TEMP "sherpa-zipformer-en-extract"
if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }
New-Item -ItemType Directory -Force -Path $ExtractDir | Out-Null
tar.exe -xjf $Archive -C $ExtractDir

$Inner = Get-ChildItem $ExtractDir -Directory | Select-Object -First 1
Copy-Item (Join-Path $Inner.FullName "encoder-epoch-99-avg-1.int8.onnx") $Dest -Force
Copy-Item (Join-Path $Inner.FullName "decoder-epoch-99-avg-1.onnx") $Dest -Force
Copy-Item (Join-Path $Inner.FullName "joiner-epoch-99-avg-1.int8.onnx") $Dest -Force
Copy-Item (Join-Path $Inner.FullName "tokens.txt") $Dest -Force

Write-Host "Model installed to $Dest"
Get-ChildItem $Dest | Format-Table Name, Length
