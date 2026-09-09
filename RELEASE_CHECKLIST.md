# Universal_Button Release Checklist

Use this checklist before publishing 2.0.1. Unchecked physical, CI and archive checks still require release validation.

## Source and API

- [ ] `UNIVERSAL_BUTTON_VERSION*`, `library.properties` and `library.json` agree, and generic `LIBRARY_VERSION*` aliases remain absent.
- [ ] `Universal_Button.h` requires an explicit `BUTTON_LIST(X)`.
- [ ] Logical pressed-state readers are not polarity-transformed.
- [ ] Electrical-level readers apply `active_low` exactly once.
- [ ] Validity-aware reader failures retain stable state and report invalid health.
- [ ] Runtime reconfiguration synchronizes without synthetic events.
- [ ] `reset()` / `resetAndSync()` baseline current hardware.
- [ ] Disable/reset latch changes advance the durable counter exactly once, even on failed synchronization; lifecycle flags still clear.
- [ ] `invalidate()` produces edge-free recovery on the next valid acquisition.
- [ ] Detailed event queue reports overflow/loss explicitly.
- [ ] `LongStarted`, `LongReleased` and `isLongHeld()` semantics match README/tests.

## Documentation and examples

- [ ] README contains Introduction/Contents/Installation/Beginner path/API/Testing/Migration sections.
- [ ] Every public example remains beginner-readable and ESP32-S3 appropriate.
- [ ] Public source and examples document callable behavior, wiring and limitations.
- [ ] README migration notes explain callback semantics and explicit `BUTTON_LIST` requirement.
- [ ] `keywords.txt` covers the public v2 API.

## Validation

- [ ] `./test/run_host_checks.sh` requires GNU GCC, Clang, ASan and UBSan, with no skipped capabilities.
- [ ] `./test/run_native_tests.sh`
- [ ] `CXX=clang++ ./test/run_native_tests.sh`
- [ ] `./test/run_sanitizers.sh`
- [ ] `./test/check_examples_host.sh`
- [ ] `./test/check_release_contracts.sh`
- [ ] PlatformIO portable compile matrix passes in CI.
- [ ] ESP32-S3 public examples compile in CI.
- [ ] Hardware smoke test performed on ESP32-S3 for native GPIO.
- [ ] Hardware expander/cached-read example exercised when relevant.

## Package hygiene

- [ ] No `.DS_Store`, `__MACOSX`, `.pio`, build output or nested release archive.
- [ ] Archive contains one top-level `Universal_Button/` folder.
- [ ] Final archive is unpacked and all local validation gates rerun from the unpacked copy.
