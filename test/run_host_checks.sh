#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

bash "$ROOT/test/run_native_tests.sh"

if command -v clang++ >/dev/null 2>&1; then
  CXX=clang++ bash "$ROOT/test/run_native_tests.sh"
else
  echo "[skip] clang++ not found; GCC/default compiler suite already ran."
fi

bash "$ROOT/test/run_sanitizers.sh"
bash "$ROOT/test/check_examples_host.sh"
bash "$ROOT/test/check_release_contracts.sh"
