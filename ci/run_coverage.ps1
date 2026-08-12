# ci/run_coverage.ps1
# ---------------------------------------------------------------------------
# Lodestar real structural coverage (S3 Phase 3) - Windows native.
#
# Produces REAL, instrumentation-measured statement coverage (not a stored
# percentage): builds the Debug config, runs the full CTest suite under
# OpenCppCoverage (https://github.com/OpenCppCoverage/OpenCppCoverage,
# https://github.com/OpenCppCoverage/OpenCppCoverage), and imports the
# resulting Cobertura report into a Lodestar SQLite database via
# lodestar_coverage_ingest.
#
# Debug (not Release) is used deliberately: optimizations inline/eliminate/
# merge lines in ways that make coverage attribution misleading, so measuring
# against an unoptimized build is standard practice for coverage evidence -
# this is a coverage build, not the shipped build.
#
# `--cover_children` is the key flag: rather than instrumenting one test
# binary at a time, OpenCppCoverage wraps `ctest` itself and instruments
# every child process ctest spawns, aggregating all 50+ test binaries into
# one Cobertura report in a single pass.
#
# Only statement (line) coverage is produced - see
# core/testforge/CoberturaImport.h for why decision/MC-DC is out of scope
# for the currently-installed toolchain.
#
# Uses its own build directory (default build-coverage/, not build/) rather
# than the normal CI build tree. Two reasons: (1) it needs a Debug build,
# which the normal Release CI flow doesn't produce, and (2) it needs a
# raised per-test CTest TIMEOUT (instrumentation overhead can multiply a
# bulk/perf test's runtime well past the normal 120s safety net - see
# LODESTAR_TEST_TIMEOUT_SECS in core/CMakeLists.txt), and that timeout is a
# CMake cache variable baked into CTestTestfile.cmake at configure time, not
# something `ctest --timeout` can override once a test has its own TIMEOUT
# property (verified: an explicit per-test property wins over the CLI flag).
# Keeping it in a separate directory means the normal CI build's tight
# 120s-hang-detection timeout is never accidentally loosened.
#
# Requires: OpenCppCoverage installed (https://github.com/OpenCppCoverage/OpenCppCoverage/releases).
#
# Usage:
#   powershell -File ci/run_coverage.ps1 [-BuildDir <path>] [-RunId <id>] [-SkipBuild]
#   -SkipBuild skips the (slow) compile step but still reconfigures (fast) so
#   a build dir shared across runs stays in sync with this script's settings.
# ---------------------------------------------------------------------------
param(
    [string]$BuildDir = "build-coverage",
    [string]$RunId = "",
    [string]$CoverageDb = "lodestar_coverage.db",
    [int]$TestTimeoutSecs = 900,
    [switch]$SkipBuild
)

# Performance/scale-stress binaries excluded from coverage runs. This is a
# deliberate, evidence-based exclusion, not an arbitrary one - both were
# tried at TestTimeoutSecs=900 (7.5x the normal 120s) and neither can pass
# under instrumentation:
#   - lodestar_wp8_tests: its 10k-entity/50k-link bulk-load test still had
#     not finished at 900s (native/uninstrumented runtime is under 2s -
#     OpenCppCoverage's per-line breakpoint-trap mechanism has real overhead
#     on tight loops with tens of thousands of iterations; this is a
#     documented characteristic of the tool, not a Lodestar bug).
#   - lodestar_wp5_assurecheck_tests: its T6 "10k scale" test has a
#     hardcoded internal assertion (core/test/wp5_assurecheck_tests.cpp:360)
#     that a batched evaluation completes within a 60,000ms *wall-clock*
#     budget - measuring real-world speed is the explicit point of that
#     test, so it fails under any coverage instrumentation by design, not
#     by any CTest timeout setting.
# Both still run at full scale in the normal (non-coverage) CI test gate -
# this exclusion applies only to the coverage-measurement pass. The
# functional (non-bulk-scale) code paths they share with other tests are
# still measured via those other tests; only the bulk-scale-specific lines
# unique to these two are left unmeasured, which the coverage report will
# correctly show as 0% rather than fabricate.
$excludedFromCoverage = "^(lodestar_wp8_tests|lodestar_wp5_assurecheck_tests)$"

$ErrorActionPreference = "Stop"

function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

$occ = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
if (-not (Test-Path $occ)) {
    Write-Error "OpenCppCoverage not found at '$occ'. Install it from https://github.com/OpenCppCoverage/OpenCppCoverage/releases"
    exit 1
}

$ctest = (Get-Command ctest -ErrorAction SilentlyContinue).Source
if (-not $ctest) {
    Write-Error "ctest not found on PATH (expected alongside cmake)."
    exit 1
}

if (-not $RunId) {
    $RunId = "coverage-" + (Get-Date -Format "yyyy-MM-ddTHH-mm-ss")
}

Write-Step "Configuring $BuildDir (LODESTAR_BUILD_TESTS=ON, LODESTAR_TEST_TIMEOUT_SECS=$TestTimeoutSecs)"
cmake -S . -B $BuildDir -DLODESTAR_BUILD_TESTS=ON -DLODESTAR_TEST_TIMEOUT_SECS=$TestTimeoutSecs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipBuild) {
    Write-Step "Building Debug (unoptimized - required for accurate line attribution)"
    cmake --build $BuildDir --config Debug
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Step "Building lodestar_coverage_ingest (Release - the ingest CLI itself doesn't need to be Debug)"
    cmake --build $BuildDir --config Release --target lodestar_coverage_ingest
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$reportDir = Join-Path $BuildDir "coverage"
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$reportPath = Join-Path $reportDir "cobertura.xml"

Write-Step "Running the full test suite under OpenCppCoverage (this is slower than a normal test run - instrumentation has real overhead, expect tens of minutes)"
# The real timeout enforcement is LODESTAR_TEST_TIMEOUT_SECS baked into
# CTestTestfile.cmake by the configure step above. --timeout here is just
# defense-in-depth for any test that somehow has no explicit property.
& $occ `
    --sources "core\" `
    --excluded_sources "core\test\" `
    --cover_children `
    --export_type "cobertura:$reportPath" `
    -- $ctest --test-dir $BuildDir -C Debug --timeout $TestTimeoutSecs -E $excludedFromCoverage
$occExit = $LASTEXITCODE

if (-not (Test-Path $reportPath)) {
    Write-Error "OpenCppCoverage did not produce a report at $reportPath"
    exit 1
}

Write-Step "Importing $reportPath into $CoverageDb (run id: $RunId)"
$ingest = Join-Path $BuildDir "core\Release\lodestar_coverage_ingest.exe"
if (-not (Test-Path $ingest)) {
    # Fall back to the Debug-config binary if Release wasn't built.
    $ingest = Join-Path $BuildDir "core\Debug\lodestar_coverage_ingest.exe"
}
if (-not (Test-Path $ingest)) {
    Write-Error "lodestar_coverage_ingest.exe not found - build it with: cmake --build $BuildDir --target lodestar_coverage_ingest"
    exit 1
}
& $ingest $reportPath $CoverageDb $RunId
$ingestExit = $LASTEXITCODE

Write-Host ""
if ($occExit -ne 0) {
    Write-Host "NOTE: the test suite had failures under coverage instrumentation (ctest exit $occExit) - coverage was still measured and imported for whatever ran; check ctest output above." -ForegroundColor Yellow
}
if ($ingestExit -ne 0) {
    Write-Error "Coverage import failed (exit $ingestExit)."
    exit $ingestExit
}
Write-Host "Coverage run '$RunId' complete. Cobertura report: $reportPath  DB: $CoverageDb" -ForegroundColor Green
