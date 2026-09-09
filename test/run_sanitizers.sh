#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/test/build"
mkdir -p "$BUILD"
CXX="${CXX:-c++}"
COMMON=(-std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -fno-omit-frame-pointer)
"$CXX" "${COMMON[@]}" -fsanitize=address -I"$ROOT/src" "$ROOT/test/test_button_handler.cpp" -o "$BUILD/test_button_handler_asan"
ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=1}"
if [[ "$(uname -s)" == "Darwin" && -z "${ASAN_OPTIONS:-}" ]]; then
  ASAN_OPTS="detect_leaks=0"
fi
ASAN_OPTIONS="$ASAN_OPTS" "$BUILD/test_button_handler_asan"
"$CXX" "${COMMON[@]}" -fsanitize=undefined -I"$ROOT/src" "$ROOT/test/test_button_handler.cpp" -o "$BUILD/test_button_handler_ubsan"
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$BUILD/test_button_handler_ubsan"
