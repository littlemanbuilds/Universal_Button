# Universal_Button tests

The test directory validates the public contracts that matter most to a reusable embedded input library.

## Local gates

```bash
./test/run_host_checks.sh
bash test/check_linkage.sh
./test/run_native_tests.sh
CXX=clang++ ./test/run_native_tests.sh
./test/run_sanitizers.sh
./test/check_examples_host.sh
./test/check_release_contracts.sh
```

`run_host_checks.sh` (also callable with `sh`) requires genuine GNU GCC, Clang, ASan and UBSan. Missing capabilities fail. Set `GCC_CXX` to a GCC executable, including an absolute path outside PATH; `GNU_CXX` is a compatible fallback. `CLANG_CXX` selects Clang. A Clang-based `g++` alias does not count as GCC.

`check_linkage.sh` compiles, links and executes two translation units through the default convenience header in C++11/14/17/20/23, checking constexpr values, reference type and local/cross-file addresses. It separately exercises `UB_NO_LEGACY_CONFIG_GLOBALS`. The host entry point runs this inexpensive check with both compilers before the runtime suites. Earlier mapping checks each built one translation unit, example checks used syntax-only compilation, and the portable sketch used the explicit-pins API, leaving default multi-file linkage unchecked.

`run_native_tests.sh` uses a deterministic host-side input model so debounce, interaction timing, recovery, latch lifecycle accounting and rollover cases can be exercised without waiting on physical hardware.

`run_sanitizers.sh` repeats the contract suite with AddressSanitizer and UndefinedBehaviorSanitizer. Negative probes require a fatal heap-overrun/overflow diagnostic with the same effective instrumentation and runtime settings as the real suite.

`check_examples_host.sh` compiles every public example against lightweight Arduino/MCP23017 stubs with C++11 and strict warnings.

`check_release_contracts.sh` validates version/package/documentation contracts and compile-time behavior such as the mandatory v2 button mapping.

## CI compile matrix

`portable_compile/portable_compile.ino` is intentionally independent of `BUTTON_LIST` and third-party expanders. GitHub Actions compiles it for the supported architecture families.

Public examples are compiled separately for ESP32-S3 because the expander examples intentionally use Adafruit MCP23017.

## Hardware evidence still required

Host tests do not replace physical validation of:

- contact bounce on the actual switch/wiring;
- ESP32-S3 GPIO electrical behavior;
- I²C/expander failure modes;
- application-specific task cadence and event-consumer latency.
