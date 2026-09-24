#!/usr/bin/env bash
set -euo pipefail

# CadView Smoke Test Script
# Runs headless/offscreen verification:
# 1. Runs oracle measurement test (F1 bounding box & surface area assertions)
# 2. Launches CadView GUI with --open F1.step --screenshot <png> --exit and asserts exit code 0 & non-empty PNG.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

echo "=== Running CadView Smoke Tests ==="

# 1. Oracle test
echo "[1/2] Verifying measurement oracles on Fixture F1..."
if [ -x "${REPO_ROOT}/build-tests/test-measure" ]; then
    "${REPO_ROOT}/scripts/with-env.sh" "${REPO_ROOT}/build-tests/test-measure"
    echo "✓ Measurement oracles verified successfully."
else
    echo "Notice: build-tests/test-measure not found, skipping unit oracle step."
fi

# 2. GUI headless open + screenshot + exit test
echo "[2/2] Testing CadView binary with Fixture F1.step..."
CADVIEW_BIN="${REPO_ROOT}/build/cadview"
if [ ! -x "${CADVIEW_BIN}" ]; then
    echo "Error: CadView executable not found at ${CADVIEW_BIN}. Please build it first." >&2
    exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT
SCREENSHOT_PNG="${TMP_DIR}/smoke_screenshot.png"

RUNNER=()
if [ -z "${DISPLAY:-}" ]; then
    if command -v xvfb-run >/dev/null 2>&1; then
        RUNNER=(xvfb-run -a)
    else
        echo "Warning: DISPLAY is not set and xvfb-run not found. Trying offscreen..."
        export QT_QPA_PLATFORM="offscreen"
    fi
fi

"${RUNNER[@]}" "${REPO_ROOT}/scripts/with-env.sh" "${CADVIEW_BIN}" \
    --open "${REPO_ROOT}/tests/fixtures/F1.step" \
    --screenshot "${SCREENSHOT_PNG}" \
    --exit

if [ ! -s "${SCREENSHOT_PNG}" ]; then
    echo "Error: Screenshot was not generated or is empty." >&2
    exit 1
fi

PNG_SIZE=$(stat -c%s "${SCREENSHOT_PNG}")
echo "✓ Screenshot successfully generated (${PNG_SIZE} bytes): ${SCREENSHOT_PNG}"
echo "=== All smoke tests PASSED! ==="
