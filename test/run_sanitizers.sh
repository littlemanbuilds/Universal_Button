#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/test/build"
mkdir -p "$BUILD"
CXX="${CXX:-c++}"
COMMON=(-std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -fno-omit-frame-pointer)
ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=1}"
if [[ "$(uname -s)" == "Darwin" && -z "${ASAN_OPTIONS:-}" ]]; then
  ASAN_OPTS="detect_leaks=0"
fi
# Enforce fatal diagnostics even when inherited options request recovery.
export ASAN_OPTIONS="$ASAN_OPTS:halt_on_error=1:abort_on_error=0"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}:halt_on_error=1"
cat > "$BUILD/sanitizer_negative.cpp" <<'CPP'
#include <limits>
int main()
{
#ifdef PROBE_ADDRESS
    volatile int index = 1;
    int* values = new int[1];
    values[index] = 7;
    delete[] values;
#else
    volatile int value = std::numeric_limits<int>::max();
    value += 1;
#endif
    return 0;
}
CPP
for sanitizer in address undefined; do
  EXTRA=()
  diagnostic='runtime error: signed integer overflow'
  if [[ "$sanitizer" == address ]]; then
    EXTRA=(-DPROBE_ADDRESS)
    diagnostic='AddressSanitizer: heap-buffer-overflow'
  fi
  FLAGS=("${COMMON[@]}" "-fsanitize=$sanitizer" -fno-sanitize-recover=all)
  "$CXX" "${FLAGS[@]}" ${EXTRA[@]+"${EXTRA[@]}"} "$BUILD/sanitizer_negative.cpp" -o "$BUILD/negative_$sanitizer"
  if "$BUILD/negative_$sanitizer" > "$BUILD/negative_$sanitizer.log" 2>&1; then
    echo "FAIL: $sanitizer negative probe exited successfully" >&2
    exit 1
  fi
  if ! grep -Fq "$diagnostic" "$BUILD/negative_$sanitizer.log"; then
    echo "FAIL: $sanitizer negative probe missing expected diagnostic" >&2
    exit 1
  fi
  "$CXX" "${FLAGS[@]}" -I"$ROOT/src" "$ROOT/test/test_button_handler.cpp" -o "$BUILD/test_button_handler_$sanitizer"
  "$BUILD/test_button_handler_$sanitizer"
  echo "PASS: $sanitizer suite and fatal negative probe"
done
