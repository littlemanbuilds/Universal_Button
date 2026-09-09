#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-c++}"
BUILD="$ROOT/test/build/contracts"
mkdir -p "$BUILD"

fail() { echo "release-contract failure: $*" >&2; exit 1; }

# ---- Version consistency ----
python3 - "$ROOT" <<'PY_VERSION'
import json
import re
import sys
from pathlib import Path
root = Path(sys.argv[1])
properties = dict(line.split("=", 1) for line in (root / "library.properties").read_text().splitlines() if "=" in line)
version = properties["version"]
assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version), "invalid package version"
assert properties["includes"] == "Universal_Button.h", "umbrella include contract"
assert json.loads((root / "library.json").read_text())["version"] == version, "JSON version mismatch"
header = (root / "src/Universal_Button.h").read_text()
assert re.search(r'^#define UNIVERSAL_BUTTON_VERSION "' + re.escape(version) + r'"$', header, re.M), "version macro mismatch"
for suffix, value in zip(("MAJOR", "MINOR", "PATCH"), version.split(".")):
    assert re.search(r"^#define UNIVERSAL_BUTTON_VERSION_" + suffix + r"\s+" + value + r"$", header, re.M), suffix + " macro mismatch"
PY_VERSION
! grep -Eq '^#define[[:space:]]+LIBRARY_VERSION([_[:space:]])' "$ROOT/src/Universal_Button.h" || fail "generic LIBRARY_VERSION aliases must remain absent"

# ---- Required documentation flow ----
for heading in '# Universal_Button' '## Contents' '## Beginner path' '## Installation' '## Testing' '## API reference' '## Migrating from v1.7.x'; do
  grep -Fq "$heading" "$ROOT/README.md" || fail "README missing: $heading"
done
grep -Fq './test/run_host_checks.sh' "$ROOT/README.md" || fail "README missing one-command host validation"

# ---- Required public API contracts ----
grep -q 'LongStarted' "$ROOT/src/ButtonTypes.h" || fail "LongStarted missing"
grep -q 'ButtonPressedResult' "$ROOT/src/ButtonTypes.h" || fail "pressed result missing"
grep -q 'ButtonLevelResult' "$ROOT/src/ButtonTypes.h" || fail "level result missing"
grep -q 'event_overflow' "$ROOT/src/ButtonHandler.h" || fail "event overflow missing"
grep -q 'invalidate(ButtonReadError' "$ROOT/src/ButtonHandler.h" || fail "invalidate API missing"
grep -q 'namespace UB' "$ROOT/src/ButtonHandler_Config.h" || fail "namespaced config missing"
grep -q 'makeButtonsWithElectricalResultReader' "$ROOT/src/Universal_Button.h" || fail "electrical result factory missing"

# ---- Metadata and CI claims ----
grep -q '"atmelmegaavr"' "$ROOT/library.json" || fail "library.json missing PlatformIO megaAVR platform"
! grep -q '"megaavr"' "$ROOT/library.json" || fail "library.json uses Arduino megaavr name for PlatformIO"
! grep -q '"samd"' "$ROOT/library.json" || fail "library.json uses non-PlatformIO samd platform"
[[ -f "$ROOT/.github/workflows/ci.yml" ]] || fail "CI workflow missing"
if git -C "$ROOT" check-ignore --no-index -q test/test_button_handler.cpp; then
  fail ".gitignore excludes committed test sources"
fi
if git -C "$ROOT" check-ignore --no-index -q CHANGELOG.md ||
   git -C "$ROOT" check-ignore --no-index -q RELEASE_CHECKLIST.md; then
  fail ".gitignore excludes release documentation"
fi

# ---- v2 must reject a missing BUTTON_LIST when the convenience header is used ----
cat > "$BUILD/missing_mapping.cpp" <<'CPP'
#include <Universal_Button.h>
int main() { return 0; }
CPP
if "$CXX" -std=c++11 -I"$ROOT/src" "$BUILD/missing_mapping.cpp" -o "$BUILD/missing_mapping" >/dev/null 2>&1; then
  fail "Universal_Button.h accepted missing BUTTON_LIST"
fi

# ---- Namespaced config compiles without legacy globals ----
cat > "$BUILD/namespaced_mapping.cpp" <<'CPP'
#define UB_NO_LEGACY_CONFIG_GLOBALS
#define BUTTON_LIST(X) X(Start, 4) X(Stop, 5)
#include <Universal_Button.h>
static bool readPressed(uint8_t id) { return id == 4u; }
int main()
{
    Button buttons = makeButtonsWithReader(readPressed);
    return UB::config::NUM_BUTTONS == 2u &&
           static_cast<uint8_t>(UB::config::ButtonIndex::Stop) == 1u ? 0 : 1;
}
CPP
"$CXX" -std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
  -I"$ROOT/src" "$BUILD/namespaced_mapping.cpp" -o "$BUILD/namespaced_mapping"
"$BUILD/namespaced_mapping"

# ---- v1.x mapping names remain source-compatible during migration ----
cat > "$BUILD/legacy_mapping.cpp" <<'CPP'
#define BUTTON_LIST(X) X(A, 4) X(B, 5)
#include <Universal_Button.h>
int main()
{
    static_assert(NUM_BUTTONS == 2u, "legacy count");
    static_assert(static_cast<uint8_t>(ButtonIndex::B) == 1u, "legacy enum");
    Button b = makeButtons(true ? ButtonTimingConfig{} : ButtonTimingConfig{});
    (void)b;
    return 0;
}
CPP
"$CXX" -std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
  -DARDUINO=10819 -I"$ROOT/test/host_stubs" -I"$ROOT/src" "$BUILD/legacy_mapping.cpp" -o "$BUILD/legacy_mapping"

# ---- v2 factory time-source overloads compile for new reader contracts ----
cat > "$BUILD/time_factory_overloads.cpp" <<'CPP'
#define BUTTON_LIST(X) X(A, 4)
#include <Universal_Button.h>
static uint32_t fixedNow() { return 123u; }
static bool readPressed(uint8_t id) { (void)id; return false; }
static bool readHigh(uint8_t id) { (void)id; return true; }
static ButtonPressedResult readPressedResult(uint8_t id)
{
    (void)id;
    return ButtonPressedResult::success(false);
}
static ButtonLevelResult readLevelResult(uint8_t id)
{
    (void)id;
    return ButtonLevelResult::success(true);
}
int main()
{
    const uint8_t pins[1] = {4u};
    ButtonTimingConfig t{};
    Button a = makeButtonsWithReader(readPressed, t, true, fixedNow);
    Button b = makeButtonsWithResultReader(readPressedResult, t, true, fixedNow);
    Button c = makeButtonsWithElectricalReader(readHigh, t, true, fixedNow);
    Button d = makeButtonsWithElectricalResultReader(readLevelResult, t, true, fixedNow);
    ButtonHandler<1> e = makeButtonsWithPinsAndResultReader(pins, readPressedResult, t, true, fixedNow);
    ButtonHandler<1> f = makeButtonsWithPinsAndElectricalResultReader(pins, readLevelResult, t, true, fixedNow);
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    return 0;
}
CPP
"$CXX" -std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
  -I"$ROOT/src" "$BUILD/time_factory_overloads.cpp" -o "$BUILD/time_factory_overloads"

# ---- Public examples and Doxygen headers ----
example_count=0
for sketch in "$ROOT"/examples/*/*.ino; do
  example_count=$((example_count + 1))
  grep -q '@file ' "$sketch" || fail "missing @file in $sketch"
  grep -q '@author Little Man Builds' "$sketch" || fail "missing LMB author in $sketch"
done
[[ "$example_count" -eq 6 ]] || fail "expected 6 public examples"

for header in "$ROOT"/src/*.h; do
  grep -q '@file ' "$header" || fail "missing @file in $header"
  grep -q '@author Little Man Builds' "$header" || fail "missing LMB author in $header"
done

# ---- Package hygiene in repository tree ----
tracked_debris=false
while IFS= read -r -d '' path; do
  [[ -e "$ROOT/$path" ]] || continue
  [[ "$path" == ".vscode/extensions.json" ]] && continue
  if [[ "$path" =~ (^|/)(\.DS_Store|__MACOSX(/|$)|\.pio/|\.vscode/|build/|dist/) ]] ||
     [[ "$path" =~ \.(zip|ZIP|o|obj|elf|bin|hex|map)$ ]]; then
    tracked_debris=true
    break
  fi
done < <(git -C "$ROOT" ls-files -z)
if "$tracked_debris"; then
  fail "tracked package debris found in release content"
fi

[[ ! -e "$ROOT/platformio.ci.ini" ]] || fail "legacy platformio.ci.ini present"

echo "PASS: release contracts"
