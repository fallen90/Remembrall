# Download default English streaming Zipformer INT8 model for Remembrall.
# Run from repo root on Windows (PowerShell):
#   .\scripts\download-models.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $Root "models\zipformer-en"
$Url = "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-en-2023-06-26.tar.bz2"
$Archive = Join-Path $env:TEMP "sherpa-onnx-streaming-zipformer-en-2023-06-26.tar.bz2"

New-Item -ItemType Directory -Force -Path $Dest | Out-Null

Write-Host "Downloading $Url"
Invoke-WebRequest -Uri $Url -OutFile $Archive

Write-Host "Extracting..."
# Requires tar (Windows 10+).
$ExtractDir = Join-Path $env:TEMP "sherpa-zipformer-en-extract"
if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }
New-Item -ItemType Directory -Force -Path $ExtractDir | Out-Null
tar -xjf $Archive -C $ExtractDir

$Inner = Get-ChildItem $ExtractDir -Directory | Select-Object -First 1
Copy-Item (Join-Path $Inner.FullName "encoder-epoch-99-avg-1.int8.onnx") $Dest -Force
Copy-Item (Join-Path $Inner.FullName "decoder-epoch-99-avg-1.onnx") $Dest -Force
Copy-Item (Join-Path $Inner.FullName "joiner-epoch-99-avg-1.int8.onnx") $Dest -Force
Copy-Item (Join-Path $Inner.FullName "tokens.txt") $Dest -Force

Write-Host "Model installed to $Dest"
Get-ChildItem $Dest | Format-Table Name, Length
