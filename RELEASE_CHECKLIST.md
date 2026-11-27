# Universal_Button Release Checklist

Use this checklist before publishing 2.0.3. Record results for the exact release commit and each distribution.

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
- [ ] Early default and opt-out multi-file mapping checks compile, link and execute with GCC and Clang.
- [ ] `./test/run_native_tests.sh`
- [ ] `CXX=clang++ ./test/run_native_tests.sh`
- [ ] `./test/run_sanitizers.sh`
- [ ] `./test/check_examples_host.sh`
- [ ] `./test/check_release_contracts.sh`
- [ ] PlatformIO portable compile matrix passes in CI.
- [ ] ESP32-S3 public examples compile in CI.

## Hardware integration

Version 2.0.3 follows the software-qualified release policy for compatible maintenance updates. It retains the qualified 2.0.2 button behavior. The project hardware integrator is responsible for completing and recording these checks on the actual wiring and devices before relying on the inputs in service.

| Check | Disposition | Integration requirement |
| --- | --- | --- |
| ESP32-S3 native GPIO | DEFERRED / NOT RUN | Verify that Example 01 reports the correct pressed and released states with the actual switch wiring. |
| ESP32-S3 with MCP23017 | DEFERRED / NOT RUN | When using an MCP23017, verify the mapped inputs and press and release behavior with the direct and cached reads demonstrated in Examples 04 and 05. |

## Package hygiene

- [ ] No `.DS_Store`, `__MACOSX`, `.pio`, build output or nested release archive.
- [ ] GitHub source ZIP and TAR contain the full reviewed source tree under one enclosing directory.
- [ ] PlatformIO archive contains only the files in the manifest export list, at the archive root.
- [ ] Arduino Library Manager ZIP matches the reviewed source inventory minus repository dotfiles and matches the index checksum.
- [ ] Public archive contents match the corresponding source files and examples compile from the unpacked library.
