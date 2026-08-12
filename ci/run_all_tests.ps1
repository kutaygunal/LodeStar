# ci/run_all_tests.ps1
# ---------------------------------------------------------------------------
# Lodestar CI test-gate script (S2 Phase 9) - Windows native.
#
# Enumerates every `*_tests.exe` in the build tree and runs each one with a
# per-test timeout. The gate FAILS (non-zero exit) if any test binary is
# missing, times out, or exits non-zero.
#
# As of S3 Phase 2, every target here is also registered with CTest, which is
# the pipeline's actual gate (see ci/Jenkinsfile: `ctest --test-dir build`) -
# it gets a real pass/fail per test plus JUnit XML for Jenkins to publish.
# This script is kept as a quick manual/offline runner that doesn't require a
# CMake test-dir configure step.
#
# Usage:
#   powershell -File ci/run_all_tests.ps1 [-BuildDir <path>] [-TimeoutSecs <n>]
# ---------------------------------------------------------------------------
param(
    [string]$BuildDir = "build",
    [int]$TimeoutSecs = 120
)

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir"
    exit 1
}

$Tests = Get-ChildItem -Path $BuildDir -Recurse -Filter "*_tests.exe" -File |
    Sort-Object FullName

if ($Tests.Count -eq 0) {
    Write-Error "No *_tests.exe binaries found under $BuildDir"
    exit 1
}

Write-Host "Lodestar CI test gate: $($Tests.Count) test binary(s) found"
Write-Host "Per-test timeout: ${TimeoutSecs}s"
Write-Host "------------------------------------------------------------"

$Failed = $false
foreach ($t in $Tests) {
    Write-Host ""
    Write-Host "==> Running: $($t.FullName)"
    $proc = Start-Process -FilePath $t.FullName -NoNewWindow -PassThru
    if (-not $proc.WaitForExit($TimeoutSecs * 1000)) {
        $proc.Kill()
        Write-Host "    [FAIL] $($t.FullName) (timed out after ${TimeoutSecs}s)" -ForegroundColor Red
        $Failed = $true
        continue
    }
    if ($proc.ExitCode -eq 0) {
        Write-Host "    [PASS] $($t.FullName)"
    } else {
        Write-Host "    [FAIL] $($t.FullName) (exit code $($proc.ExitCode))" -ForegroundColor Red
        $Failed = $true
    }
}

Write-Host ""
Write-Host "------------------------------------------------------------"
if ($Failed) {
    Write-Host "CI TEST GATE FAILED: one or more test binaries failed." -ForegroundColor Red
    exit 1
}

Write-Host "CI TEST GATE PASSED: all $($Tests.Count) test binary(s) succeeded."
exit 0
