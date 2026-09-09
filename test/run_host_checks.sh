#!/bin/sh
# The public entry point also accepts `sh test/run_host_checks.sh`.
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

is_gnu_gcc() {
  local macros
  macros=$("$1" -dM -E -x c++ /dev/null 2>/dev/null) || return 1
  [[ "$macros" == *"#define __GNUC__ "* && "$macros" != *"#define __clang__ "* ]]
}

# Explicit selections may be absolute paths outside PATH. macOS g++ is often Clang.
GCC_CXX="${GCC_CXX:-${GNU_CXX:-}}"
if [[ -n "$GCC_CXX" ]]; then
  is_gnu_gcc "$GCC_CXX" || { echo "FAIL: GCC_CXX/GNU_CXX must select genuine GNU GCC" >&2; exit 1; }
else
  for candidate in g++ /opt/homebrew/bin/g++-* /usr/local/bin/g++-* /usr/bin/g++-*; do
    if is_gnu_gcc "$candidate"; then
      GCC_CXX="$candidate"
      break
    fi
  done
  [[ -n "$GCC_CXX" ]] || { echo "FAIL: GNU GCC is required; set GCC_CXX to its executable" >&2; exit 1; }
fi

CLANG_CXX="${CLANG_CXX:-clang++}"
clang_macros=$("$CLANG_CXX" -dM -E -x c++ /dev/null) || { echo "FAIL: Clang is required" >&2; exit 1; }
[[ "$clang_macros" == *"#define __clang__ "* ]] || { echo "FAIL: CLANG_CXX must select Clang" >&2; exit 1; }

echo "[host] GNU GCC: $GCC_CXX"
CXX="$GCC_CXX" bash "$ROOT/test/run_native_tests.sh"
echo "[host] Clang: $CLANG_CXX"
CXX="$CLANG_CXX" bash "$ROOT/test/run_native_tests.sh"
echo "[host] Required Clang AddressSanitizer and UndefinedBehaviorSanitizer"
CXX="$CLANG_CXX" bash "$ROOT/test/run_sanitizers.sh"
CXX="$CLANG_CXX" bash "$ROOT/test/check_examples_host.sh"
CXX="$CLANG_CXX" bash "$ROOT/test/check_release_contracts.sh"
