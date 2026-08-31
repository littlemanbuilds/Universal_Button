#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/test/build"
mkdir -p "$BUILD"
CXX="${CXX:-c++}"
FLAGS=(-std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
"$CXX" "${FLAGS[@]}" -I"$ROOT/src" "$ROOT/test/test_button_handler.cpp" -o "$BUILD/test_button_handler"
"$BUILD/test_button_handler"
