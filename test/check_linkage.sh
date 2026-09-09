#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/test/build/linkage"
mkdir -p "$BUILD"
CXX="${CXX:-c++}"
FLAGS=(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
for standard in c++11 c++14 c++17 c++20 c++23; do
  for mode in default opt-out; do
    DEFINES=()
    if [[ "$mode" == opt-out ]]; then
      DEFINES=(-DUB_NO_LEGACY_CONFIG_GLOBALS)
    fi
    "$CXX" -std="$standard" "${FLAGS[@]}" ${DEFINES[@]+"${DEFINES[@]}"} -I"$ROOT/src" \
      "$ROOT/test/linkage/a.cpp" "$ROOT/test/linkage/b.cpp" -o "$BUILD/$standard-$mode"
    "$BUILD/$standard-$mode"
  done
done
echo "PASS: default and opt-out multi-file linkage (C++11/14/17/20/23)"
