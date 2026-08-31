# Universal_Button tests

The test directory validates the public contracts that matter most to a reusable embedded input library.

## Local gates

```bash
./test/run_host_checks.sh
./test/run_native_tests.sh
CXX=clang++ ./test/run_native_tests.sh
./test/run_sanitizers.sh
./test/check_examples_host.sh
./test/check_release_contracts.sh
```

`run_host_checks.sh` runs the complete host-side validation suite in the normal release order.

`run_native_tests.sh` uses a deterministic host-side input model so debounce, interaction timing, recovery and rollover cases can be exercised without waiting on physical hardware.

`run_sanitizers.sh` repeats the contract suite with AddressSanitizer and UndefinedBehaviorSanitizer.

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
