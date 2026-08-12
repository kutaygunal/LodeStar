#!/usr/bin/env bash
# ci/run_all_tests.sh
# ---------------------------------------------------------------------------
# Lodestar CI test-gate script (S2 Phase 9).
#
# Enumerates every `*_tests.exe` in the build tree and runs each one with a
# per-test timeout. The gate FAILS (non-zero exit) if any test binary is
# missing, times out, or exits non-zero.
#
# Usage:
#   ci/run_all_tests.sh [BUILD_DIR] [TIMEOUT_SECS]
#     BUILD_DIR    path to the CMake build tree (default: ./build)
#     TIMEOUT_SECS per-test timeout in seconds (default: 120)
# ---------------------------------------------------------------------------
set -u

BUILD_DIR="${1:-build}"
TIMEOUT_SECS="${2:-120}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: build directory not found: $BUILD_DIR" >&2
    exit 1
fi

# Enumerate every test binary in the build tree.
mapfile -t TESTS < <(find "$BUILD_DIR" -type f -name '*_tests.exe' 2>/dev/null | sort)

if [ "${#TESTS[@]}" -eq 0 ]; then
    echo "ERROR: no *_tests.exe binaries found under $BUILD_DIR" >&2
    exit 1
fi

echo "Lodestar CI test gate: ${#TESTS[@]} test binary(s) found"
echo "Per-test timeout: ${TIMEOUT_SECS}s"
echo "------------------------------------------------------------"

FAILED=0
for t in "${TESTS[@]}"; do
    echo ""
    echo "==> Running: $t"
    if timeout "$TIMEOUT_SECS" "$t"; then
        echo "    [PASS] $t"
    else
        rc=$?
        echo "    [FAIL] $t (exit code $rc)" >&2
        FAILED=1
    fi
done

echo ""
echo "------------------------------------------------------------"
if [ "$FAILED" -ne 0 ]; then
    echo "CI TEST GATE FAILED: one or more test binaries failed."
    exit 1
fi

echo "CI TEST GATE PASSED: all ${#TESTS[@]} test binary(s) succeeded."
exit 0
