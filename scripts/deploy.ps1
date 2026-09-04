# Deploy Remembrall to GitHub Releases.
#
# Prerequisites (Windows):
#   - Visual Studio 2022 + CMake
#   - GitHub CLI: https://cli.github.com/  (`gh auth login`)
#   - git remote origin pointing at your GitHub repo
#
# Usage:
#   .\scripts\deploy.ps1 -Version 1.0.1
#   .\scripts\deploy.ps1 -Version 1.0.1 -Repo fallen90/Remembrall -SkipBuild
#   .\scripts\deploy.ps1 -Version 1.0.1 -Draft
#
# Creates tag v{Version}, builds RelWithDebInfo, zips the portable package, and
# publishes a GitHub Release asset named:
#   Remembrall-windows-x64-v{Version}.zip
#
# The in-app updater looks for assets matching Remembrall-windows-x64*.zip
# on the latest GitHub Release.

param(
  [Parameter(Mandatory = $true)]
  [ValidatePattern('^\d+\.\d+\.\d+$')]
  [string]$Version,

  [string]$Repo = "",
  [string]$Config = "RelWithDebInfo",
  [switch]$SkipBuild,
  [switch]$Draft,
  [switch]$IncludeModels,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "build"
$Dist = Join-Path $Root "dist"
$Tag = "v$Version"

function Resolve-Repo {
  param([string]$Explicit)
  if ($Explicit) { return $Explicit }
  Push-Location $Root
  try {
    $fromGh = gh repo view --json nameWithOwner -q .nameWithOwner 2>$null
    if ($LASTEXITCODE -eq 0 -and $fromGh) { return $fromGh.Trim() }
  } catch {}
  try {
    $url = git remote get-url origin 2>$null
    if ($url -match 'github\.com[:/](.+?)(?:\.git)?$') {
      return $Matches[1]
    }
  } catch {}
  finally { Pop-Location }
  throw "Could not resolve GitHub repo. Pass -Repo owner/name or set git remote / gh auth."
}

$Repo = Resolve-Repo -Explicit $Repo
Write-Host "Repo:    $Repo"
Write-Host "Version: $Version  ($Tag)"
Write-Host "Config:  $Config"

# Keep CMake project version in sync for embedded updater semver.
$cmakePath = Join-Path $Root "CMakeLists.txt"
$cmake = Get-Content $cmakePath -Raw
$cmake2 = [regex]::Replace(
  $cmake,
  'project\(remembrall VERSION \d+\.\d+\.\d+',
  "project(remembrall VERSION $Version"
)
if ($cmake2 -ne $cmake) {
  if ($DryRun) {
    Write-Host "[dry-run] Would bump CMakeLists.txt VERSION to $Version"
  } else {
    Set-Content -Path $cmakePath -Value $cmake2 -NoNewline
    Write-Host "Updated CMakeLists.txt project VERSION to $Version"
  }
}

if (-not $SkipBuild) {
  Write-Host "Configuring / building..."
  cmake -S $Root -B $Build -G "Visual Studio 17 2022" -A x64 `
    "-DLTA_GITHUB_REPO=$Repo" `
    "-DPROJECT_VERSION=$Version"
  # PROJECT_VERSION is set by project() — reconfigure after VERSION bump:
  cmake -S $Root -B $Build -G "Visual Studio 17 2022" -A x64 "-DLTA_GITHUB_REPO=$Repo"
  cmake --build $Build --config $Config --parallel
}

$OutDir = Join-Path $Build $Config
$Exe = Join-Path $OutDir "Remembrall.exe"
if (-not (Test-Path $Exe)) {
  # Multi-config vs single-config generators.
  $alt = Get-ChildItem -Path $Build -Recurse -Filter "Remembrall.exe" | Select-Object -First 1
  if (-not $alt) { throw "Remembrall.exe not found under $Build" }
  $Exe = $alt.FullName
  $OutDir = $alt.Directory.FullName
}

$Stage = Join-Path $Dist "Remembrall-$Version"
$ZipName = "Remembrall-windows-x64-v$Version.zip"
$ZipPath = Join-Path $Dist $ZipName

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

Write-Host "Staging portable package from $OutDir"
Copy-Item $Exe $Stage
Get-ChildItem $OutDir -Filter "*.dll" | ForEach-Object { Copy-Item $_.FullName $Stage }
# Ship PNG for docs / future installer branding (optional).
$IconPng = Join-Path $Root "assets\remembrall.png"
if (Test-Path $IconPng) { Copy-Item $IconPng $Stage }

# Optional: ship models inside the zip (large). Default: users run download-models.ps1 / first-run fetch.
if ($IncludeModels) {
  $modelsSrc = Join-Path $Root "models\zipformer-en"
  if (Test-Path (Join-Path $modelsSrc "tokens.txt")) {
    $modelsDst = Join-Path $Stage "models\zipformer-en"
    New-Item -ItemType Directory -Force -Path $modelsDst | Out-Null
    Copy-Item (Join-Path $modelsSrc "*") $modelsDst -Recurse -Force
  } else {
    Write-Warning "models\zipformer-en missing — package will not include ASR models"
  }
} else {
  # Include download helper so fresh installs can fetch models.
  Copy-Item (Join-Path $Root "scripts\download-models.ps1") $Stage -ErrorAction SilentlyContinue
  Set-Content -Path (Join-Path $Stage "GETTING_STARTED.txt") -Value @"
Remembrall $Version

1. Run download-models.ps1 once (or place Zipformer files under models\zipformer-en\).
2. Start Remembrall.exe
3. Join a Discord voice call — transcripts appear automatically.

Updates: the app checks GitHub Releases ($Repo) for Remembrall-windows-x64*.zip
"@
}

if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $ZipPath -Force
Write-Host "Packed: $ZipPath ($([math]::Round((Get-Item $ZipPath).Length / 1MB, 1)) MB)"

$notes = @"
## Remembrall $Version

Portable Windows x64 build.

### Install
1. Unzip ``$ZipName``
2. Run ``download-models.ps1`` once (unless models are bundled)
3. Start ``Remembrall.exe``

### Auto-update
Existing installs with update checks enabled will offer this release automatically.
"@

if ($DryRun) {
  Write-Host "[dry-run] Would create git tag $Tag and gh release on $Repo"
  Write-Host "[dry-run] Asset: $ZipPath"
  exit 0
}

Push-Location $Root
try {
  git add CMakeLists.txt 2>$null
  $status = git status --porcelain
  if ($status) {
    git commit -m "release: v$Version" -- CMakeLists.txt
  }

  $tagExists = git rev-parse -q --verify "refs/tags/$Tag" 2>$null
  if (-not $tagExists) {
    git tag -a $Tag -m "Remembrall $Version"
  } else {
    Write-Host "Tag $Tag already exists locally"
  }

  git push origin HEAD
  git push origin $Tag

  $ghArgs = @(
    "release", "create", $Tag,
    $ZipPath,
    "--repo", $Repo,
    "--title", "Remembrall $Version",
    "--notes", $notes
  )
  if ($Draft) { $ghArgs += "--draft" }

  gh release view $Tag --repo $Repo 2>$null
  if ($LASTEXITCODE -eq 0) {
    Write-Host "Release $Tag exists — uploading asset"
    gh release upload $Tag $ZipPath --repo $Repo --clobber
  } else {
    & gh @ghArgs
  }

  Write-Host ""
  Write-Host "Published: https://github.com/$Repo/releases/tag/$Tag"
  Write-Host "Asset:     $ZipName"
}
finally {
  Pop-Location
}
