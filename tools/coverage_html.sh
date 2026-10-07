#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-${ROOT_DIR}/build/coverage/index.html}"

if [[ "${OUT_FILE}" != /* ]]; then
  OUT_FILE="${ROOT_DIR}/${OUT_FILE}"
fi

# Instrumentation is the ONLY thing that differs from the normal build; the C standard,
# include paths, warning set and the list of sources all come from the Makefile so they
# cannot drift apart.
COVERAGE_OPT='-O0 -g --coverage'

cd "${ROOT_DIR}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "Error: gcovr is not installed."
  echo "Install with: pip install gcovr"
  exit 1
fi

# gcovr reads gcov data, so the instrumented build uses gcc whatever CC is set to elsewhere.
# The unit tests are built and run once per build configuration (see the Makefile): the
# default one, and one with the TC Segment Header compiled in.
make clean >/dev/null
make unit-tests CC=gcc OPT="${COVERAGE_OPT}" >/dev/null
./build/bin/unit_tests >/dev/null
./build/segment_header/unit_tests >/dev/null

mkdir -p "$(dirname "${OUT_FILE}")"

# Print the text summary (line + branch) of one build configuration and require 100% of
# both. Each configuration is gated on its own, so code that only one of them compiles
# cannot hide behind the other's tests. gcovr's chatty "(INFO)" progress lines are filtered
# from stderr; warnings and errors still pass through.
#   $1  label printed above the summary
#   $2  directory holding that configuration's coverage data
status=0
check_configuration() {
  echo "Coverage ($1):"
  gcovr -r "${ROOT_DIR}" \
    --filter "${ROOT_DIR}/src" \
    --txt - \
    --txt-summary \
    --fail-under-line 100 \
    --fail-under-branch 100 \
    "$2" \
    2> >(grep -v '^(INFO)' >&2) || status=1
}

check_configuration "default configuration" "${ROOT_DIR}/build/obj"
check_configuration "TC_SEGMENT_HEADER_ENABLED" "${ROOT_DIR}/build/segment_header"

# One HTML report for both configurations together, so it also shows the lines that only
# the segment-header build compiles.
gcovr -r "${ROOT_DIR}" \
  --filter "${ROOT_DIR}/src" \
  --html-details \
  --output "${OUT_FILE}" \
  2> >(grep -v '^(INFO)' >&2)

echo "Coverage HTML report written to: ${OUT_FILE}"
exit "${status}"
