# packaging/installer.ps1
# ---------------------------------------------------------------------------
# Lodestar — Commercial packaging installer.
#
# Packages the built desktop app + its runtime DLLs into an installable bundle
# and (optionally) installs it to a target directory.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File packaging/installer.ps1 `
#       -BuildDir build/ui/Release `
#       -InstallDir "C:\Program Files\Lodestar" `
#       -StageDir build/package
#
# The script:
#   1. Validates the built app exists (lodestar_app.exe).
#   2. Stages the app + Qt runtime DLLs + platform plugins into a bundle.
#   3. Copies the LICENSE, user guide, and support docs into the bundle.
#   4. (Optional) Installs the bundle to InstallDir.
#   5. Produces a ZIP archive of the bundle for distribution.
# ---------------------------------------------------------------------------

param(
    [string]$BuildDir = "build/ui/Release",
    [string]$InstallDir = "",
    [string]$StageDir = "build/package",
    [string]$AppName = "lodestar_app.exe",
    [switch]$SkipInstall
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) {
    Write-Host "==> $msg" -ForegroundColor Cyan
}

# --- 1. Validate the built app ---------------------------------------------
$appPath = Join-Path $BuildDir $AppName
if (-not (Test-Path $appPath)) {
    Write-Error "Built app not found at '$appPath'. Build the UI first with: cmake --build build --config Release --target lodestar_app"
    exit 1
}
Write-Step "Found built app: $appPath"

# --- 2. Stage the bundle ---------------------------------------------------
$stage = Join-Path (Get-Location) $StageDir
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Write-Step "Staging bundle to: $stage"

# Copy the app executable.
Copy-Item $appPath (Join-Path $stage $AppName)

# Copy Qt runtime DLLs (Core/Gui/Widgets) if present next to the app.
foreach ($dll in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll",
                   "Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll")) {
    $src = Join-Path $BuildDir $dll
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $stage $dll)
        Write-Step "Copied runtime DLL: $dll"
    }
}

# Copy platform plugins (qwindows / qoffscreen) if present.
$platformsSrc = Join-Path $BuildDir "platforms"
if (Test-Path $platformsSrc) {
    $platformsDst = Join-Path $stage "platforms"
    New-Item -ItemType Directory -Force -Path $platformsDst | Out-Null
    Copy-Item (Join-Path $platformsSrc "*") $platformsDst -Recurse
    Write-Step "Copied platform plugins"
}

# --- 3. Copy license + docs into the bundle --------------------------------
$root = (Get-Location).Path
foreach ($doc in @("LICENSE.md", "docs/user-guide.md", "docs/support.md")) {
    $src = Join-Path $root $doc
    if (Test-Path $src) {
        $dstDir = Join-Path $stage (Split-Path $doc -Parent)
        if ($dstDir -ne $stage) { New-Item -ItemType Directory -Force -Path $dstDir | Out-Null }
        Copy-Item $src (Join-Path $stage $doc)
        Write-Step "Bundled doc: $doc"
    }
}

# --- 4. Install to target directory (unless skipped) -----------------------
if ($InstallDir -and -not $SkipInstall) {
    Write-Step "Installing to: $InstallDir"
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Copy-Item (Join-Path $stage "*") $InstallDir -Recurse -Force
    Write-Step "Install complete."
}

# --- 5. Produce a distributable ZIP ---------------------------------------
$zip = Join-Path (Get-Location) "build/lodestar-package.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Write-Step "Created distributable archive: $zip"

Write-Host "`nPackaging complete. Bundle staged at: $stage" -ForegroundColor Green
