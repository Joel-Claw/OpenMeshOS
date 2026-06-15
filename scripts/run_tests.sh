#!/bin/bash
# OpenMeshOS — Host-side unit test runner
# Copyright 2026 Joel Claw & contributors — WTFPL v2
#
# Compiles and runs all host-side unit tests (no Arduino, no hardware).
# Tests must compile with g++ -std=c++14 with no Arduino includes.
#
# Usage: ./scripts/run_tests.sh [--keep]
#   --keep  Keep compiled test binaries (don't delete after run)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$ROOT_DIR/test"

KEEP_BINARIES=false
if [[ "$1" == "--keep" ]]; then
    KEEP_BINARIES=true
fi

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++14 -Wall -Wextra -Werror -I$ROOT_DIR/src"

PASS=0
FAIL=0
SKIP=0
BINARY_LIST=()

echo "=== OpenMeshOS Host-Side Unit Tests ==="
echo "Compiler: $CXX"
echo ""

# Skip tests that require SPIFFS (they need Arduino filesystem)
SKIP_TESTS=(
    "test_spiffs_integration.cpp"
    "test_spiffs_stress.cpp"
)

should_skip() {
    local file="$1"
    for skip in "${SKIP_TESTS[@]}"; do
        if [[ "$file" == "$skip" ]]; then
            return 0
        fi
    done
    return 1
}

for test_file in "$TEST_DIR"/*.cpp; do
    test_name=$(basename "$test_file" .cpp)
    test_bin="/tmp/oms_${test_name}"

    if should_skip "$(basename "$test_file")"; then
        echo "SKIP  $test_name (requires Arduino filesystem)"
        ((SKIP++)) || true
        continue
    fi

    echo -n "  $test_name ... "

    if $CXX $CXXFLAGS -o "$test_bin" "$test_file" -lm 2>/dev/null; then
        BINARY_LIST+=("$test_bin")
        if "$test_bin" > /tmp/oms_${test_name}_output.txt 2>&1; then
            echo "PASS"
            ((PASS++)) || true
        else
            echo "FAIL"
            cat /tmp/oms_${test_name}_output.txt
            ((FAIL++)) || true
        fi
    else
        echo "COMPILE FAIL"
        ((FAIL++)) || true
    fi
done

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="

if [[ "$KEEP_BINARIES" != true ]]; then
    for bin in "${BINARY_LIST[@]}"; do
        rm -f "$bin"
    done
fi

rm -f /tmp/oms_*_output.txt

exit $FAIL