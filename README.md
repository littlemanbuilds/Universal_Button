# Universal_Button

A small, header-only Arduino library for turning physical buttons into **stable levels and trustworthy interactions**.

Universal_Button handles the repetitive parts of button input that become surprisingly difficult in larger embedded projects:

- debounce;
- short, long and double-click classification;
- a long-press threshold event while the button is still held;
- explicit input validity/freshness;
- logical pressed-state **and** electrical HIGH/LOW readers;
- validity-aware readers for expanders and cached input systems;
- bounded event preservation with overflow diagnostics;
- optional latching;
- synchronized runtime reconfiguration;
- enum-friendly APIs;
- no dynamic allocation.

ESP32-S3 is the primary development target, but the core remains intentionally portable.

**2.0.1 fixes durable latch-change accounting during disable and reset.** Reader, event and synchronization features introduced in 2.0.0 remain available.

> **Since v2.0.0:** if you include `Universal_Button.h`, you must explicitly define your button mapping. The old silent GPIO25 fallback has been removed.

---

## Contents

1. [What problem does this solve?](#what-problem-does-this-solve)
2. [Design boundaries](#design-boundaries)
3. [Installation](#installation)
4. [Supported targets](#supported-targets)
5. [Beginner path](#beginner-path)
6. [The two reader contracts](#the-two-reader-contracts)
7. [Input validity and freshness](#input-validity-and-freshness)
8. [Debounced state vs interaction events](#debounced-state-vs-interaction-events)
9. [Short, double and long semantics](#short-double-and-long-semantics)
10. [The bounded event queue](#the-bounded-event-queue)
11. [Timing configuration](#timing-configuration)
12. [Synchronization, reset and recovery](#synchronization-reset-and-recovery)
13. [Per-button configuration](#per-button-configuration)
14. [Latching](#latching)
15. [Compile-time button mapping](#compile-time-button-mapping)
16. [Port expanders and cached input](#port-expanders-and-cached-input)
17. [Examples](#examples)
18. [Testing](#testing)
19. [API reference](#api-reference)
20. [Migrating from v1.7.x](#migrating-from-v17x)
21. [Application integration](#application-integration)
22. [Repository structure](#repository-structure)
23. [Limitations and design notes](#limitations-and-design-notes)
24. [Version history](#version-history)
25. [License](#license)

---

## What problem does this solve?

A button looks simple:

```cpp
if (digitalRead(pin) == LOW)
{
    // pressed
}
```

That is fine until the project needs to answer harder questions:

- Did the contact bounce?
- Has the input source stopped responding?
- Was this one press or two?
- Has the long threshold been reached **while the user is still holding the button**?
- Did two events occur before the consumer had time to read them?
- Was the callback returning a logical button state or a raw electrical level?
- Did changing reader/polarity create a fake button edge?
- Is a held button safe to use as a hold-to-enable input?
- Can a port-expander read failure be distinguished from “not pressed”?

Universal_Button separates these concerns instead of hiding them behind one ambiguous boolean.

The normal flow is:

```text
physical input
    ↓
reader / acquisition validity
    ↓
logical pressed state
    ↓
debounce
    ↓
stable level ─────────────→ isPressed() / isLongHeld()
    ↓
interaction classification
    ↓
bounded event queue ──────→ Short / Double / LongStarted / LongReleased
```

---

## Design boundaries

Universal_Button intentionally **does**:

- read one or more button inputs;
- normalize electrical polarity when requested;
- debounce each logical input;
- expose stable pressed/held state;
- classify user interactions;
- retain multiple interaction events in a bounded queue;
- expose input health, freshness and sequence metadata;
- support explicit latching behavior.

Universal_Button intentionally **does not**:

- own an application task;
- depend on an application transport, persistence layer or safety controller;
- persist configuration;
- decide whether a button is allowed to move a vehicle;
- turn one-shot interaction events into a system-wide event bus;
- allocate from the heap.

That keeps the library useful by itself while allowing a larger project to wrap it with whatever transport/safety architecture it needs.

---

## Installation

### Arduino Library Manager

When the release is available through Arduino Library Manager:

1. Open **Sketch → Include Library → Manage Libraries...**
2. Search for **Universal_Button**.
3. Install the latest version.

### PlatformIO

Add the library to your project dependencies:

```ini
lib_deps =
    littlemanbuilds/Universal_Button@^2.0.1
```

### Manual installation

Copy the `Universal_Button` folder into your Arduino `libraries` directory or your PlatformIO project's `lib/` directory.

---

## Supported targets

The implementation is standard C++11 and avoids dynamic allocation.

`library.properties` declares support for AVR, megaAVR, SAMD, ESP32, ESP8266, RP2040, Teensy and STM32. Repository CI uses one representative compile target for each of those architecture families:

- ESP32-S3 DevKitC-1;
- ESP8266;
- RP2040 Pico;
- SAMD (MKR Zero);
- Nano Every;
- AVR Uno;
- Teensy 4.1;
- STM32 Blue Pill.

The public learning examples are primarily written for **ESP32-S3**.

---

## Beginner path

You do not need to understand event queues, freshness counters or port expanders to get started.

The simplest useful workflow is:

1. define your button;
2. create the handler;
3. call `update()` repeatedly;
4. ask `isPressed()`;
5. optionally read `getPressType()` later.

### 1. Wire one button

For the normal beginner wiring:

```text
ESP32-S3 GPIO6 ---- button ---- GND
```

Universal_Button's native GPIO path uses `INPUT_PULLUP`, so:

- released = HIGH;
- pressed = LOW.

No external pull-up resistor is normally required.

### 2. Define the button before including the library

```cpp
#define BUTTON_LIST(X) \
    X(TestButton, 6)

#include <Universal_Button.h>

#include <Arduino.h>
```

There is deliberately **no hidden default button** in v2.

### 3. Create the handler

```cpp
static Button buttons = makeButtons();
```

### 4. Update it in `loop()`

```cpp
void loop()
{
    buttons.update();

    if (buttons.isPressed(UB::config::ButtonIndex::TestButton))
    {
        Serial.println("Pressed");
    }
}
```

That is enough for a stable debounced level.

### 5. Add short/long/double interactions only when you need them

```cpp
const ButtonPressType event =
    buttons.getPressType(UB::config::ButtonIndex::TestButton);

if (event == ButtonPressType::Short)
{
    Serial.println("Short press");
}
else if (event == ButtonPressType::Long)
{
    Serial.println("Long press completed");
}
```

`getPressType()` is the familiar/simple interface. The detailed queue is there when a bigger project needs it.

### 6. Check validity in larger applications

With native GPIO there is usually no software-level read failure to report.

With an expander, cached input source or other hardware abstraction, prefer a validity-aware reader and check:

```cpp
if (!buttons.valid())
{
    // Do not use the input as a fresh command.
}
```

That is the point where a beginner sketch becomes a production input provider.

---

## The two reader contracts

The biggest v1.x ambiguity was that a callback documented as “pressed?” was then polarity-transformed again.

v2 makes the contract explicit.

### 1. Logical pressed-state reader

Use this when your callback already knows whether the button is pressed:

```cpp
static bool readPressed(uint8_t key)
{
    return myHardwareSaysPressed(key);
}

static Button buttons = makeButtonsWithReader(readPressed);
```

The contract is:

```text
false = not pressed
true  = pressed
```

`active_low` is irrelevant to this callback because polarity has already been resolved.

### 2. Electrical-level reader

Use this when the callback returns the actual HIGH/LOW level:

```cpp
static bool readLevel(uint8_t pin)
{
    return digitalRead(pin) == HIGH;
}

static Button buttons = makeButtonsWithElectricalReader(readLevel);
```

The contract is:

```text
false = LOW
true  = HIGH
```

Universal_Button then applies `ButtonPerConfig::active_low`.

With the default:

```cpp
active_low = true;
```

LOW becomes pressed.

### Why the distinction matters

These two callbacks may both return `bool`, but they do **not** mean the same thing.

v2 therefore uses deliberately different factory/setter names rather than guessing.

---

## Input validity and freshness

A plain `bool` reader is convenient, but it cannot say “the read failed.”

For hardware where that distinction exists, use a result reader.

### Logical pressed-state result

```cpp
ButtonPressedResult readButton(uint8_t key)
{
    if (!hardwareOk())
        return ButtonPressedResult::failure();

    return ButtonPressedResult::success(isPressed(key));
}
```

### Electrical-level result

```cpp
ButtonLevelResult readLevel(uint8_t key)
{
    if (!hardwareOk())
        return ButtonLevelResult::failure();

    return ButtonLevelResult::success(readHigh(key));
}
```

### What happens on failure?

Universal_Button does **not** turn the failure into `false`.

Instead it:

- marks the input invalid;
- retains the last stable debounced level;
- records the failure timestamp/error;
- cancels ambiguous interaction timing;
- marks an acquisition gap;
- waits for a successful read;
- rebaselines the recovered level without generating synthetic events.

Useful queries include:

```cpp
buttons.configured();
buttons.configError();
buttons.valid();
buttons.hasSample();

buttons.valid(id);
buttons.hasSample(id);
buttons.sampleMs(id);
buttons.sequence(id);
buttons.changeSequence(id);
buttons.generation(id);
buttons.inputStatus(id);
buttons.status();
```

### `valid` is not the same as `hasSample`

`hasSample()` answers:

> Has this enabled input ever produced a successful acquisition?

`valid()` answers:

> Was the most recent acquisition successful?

A mature application often needs both.

---

## Debounced state vs interaction events

These are different data types and should be treated differently.

### Stable state

```cpp
buttons.isPressed(id);
buttons.isLongHeld(id);
```

These answer **what is true now**.

They are appropriate for:

- horn held state;
- hold-to-enable inputs;
- current button level;
- latest-state snapshots.

### Interaction events

```cpp
ButtonEvent event;
while (buttons.popEvent(event))
{
    ...
}
```

These answer **what happened**.

They are appropriate for:

- short press;
- double click;
- long threshold reached;
- long press released;
- UI actions that must not be silently overwritten.

Do not use a one-shot event as a substitute for a continuously required hold state.

For example, a hold-to-enable function should consume `isPressed()`/freshness, not `LongStarted`.

---

## Short, double and long semantics

### Short

A press must remain debounced/held for at least `short_press_ms` and be released before the long threshold.

A single short interaction is delayed until the double-click window has expired because the library cannot know immediately whether a second short press is coming.

### Double

Two completed short interactions whose release-to-release interval is within `double_click_ms` produce one `Double` interaction.

The first short is not emitted separately.

### LongStarted

When a debounced press reaches `long_press_ms` while still held, the detailed event queue receives:

```cpp
ButtonEventType::LongStarted
```

At the same time:

```cpp
buttons.isLongHeld(id) == true
```

### LongReleased

When that long-held button is released, the detailed queue receives:

```cpp
ButtonEventType::LongReleased
```

The compatibility API maps that completed interaction to:

```cpp
ButtonPressType::Long
```

So the old simple API retains the intuitive behavior:

> `Long` means the long press is complete and has been released.

The new detailed API adds the missing threshold-time information.

---

## The bounded event queue

v1.x stored only one pending event per button. If several interactions completed before a consumer read the slot, a newer event could replace an older one.

v2 uses a fixed-capacity queue.

Default capacity:

```cpp
UB_EVENT_QUEUE_CAPACITY == 16
```

You can override it before including the library:

```cpp
#define UB_EVENT_QUEUE_CAPACITY 8
#include <ButtonHandler.h>
```

No heap allocation is used.

### Reading events

```cpp
ButtonEvent event;
while (buttons.popEvent(event))
{
    Serial.print("Button: ");
    Serial.println(event.button_id);
}
```

Each event includes:

```cpp
event.button_id;
event.type;
event.timestamp_ms;
event.duration_ms;
event.sequence;
```

### Detecting overflow

The queue intentionally does not pretend it has infinite storage.

When full, the newest event is rejected and the library records the loss:

```cpp
buttons.eventOverflowed();
buttons.droppedEventCount();
buttons.eventSequence();
```

`eventSequence()` advances for every generated event, including dropped ones.

That allows larger systems to detect that event history is incomplete.

### `getPressType()` and `popEvent()` are alternate consumption styles

`getPressType(button)` is retained for simple sketches.

`popEvent()` is preferred when a project needs ordered, loss-detectable interactions.

They consume the same detailed event stream, so do not treat them as independent subscribers.

If multiple application components need reliable copies of every event, fan the events out at the application/event-bus layer.

---

## Timing configuration

The global timing object is:

```cpp
ButtonTimingConfig timing{
    30,   // debounce_ms
    200,  // short_press_ms
    1000, // long_press_ms
    400   // double_click_ms
};
```

v2 validates timing before activating runtime changes.

The resolved timing must satisfy:

```text
short_press_ms > 0
long_press_ms  > short_press_ms
debounce_ms    <= short_press_ms
double_click_ms >= debounce_ms
```

Runtime setters return `ButtonConfigResult`:

```cpp
const ButtonConfigResult result = buttons.setGlobalTiming(timing);

if (!result)
{
    // Existing valid configuration remains active.
}
```

Per-button overrides are validated after inheritance from the global values.

---

## Synchronization, reset and recovery

A reusable input library must distinguish “clear my software state” from “invent a released hardware state.”

### `sync()`

```cpp
buttons.sync();
```

Reads every enabled input and establishes the physical levels as the current baseline **without generating press/release events**.

Use it when:

- commissioning hardware;
- changing an external reader;
- changing electrical polarity;
- entering a new operating mode where event history should restart from current reality.

If a button is already held during synchronization, the stable level becomes pressed, but interaction classification is suppressed until that button is first released.

That prevents a held input from becoming a synthetic “new press.”

### `reconfigureAndSync()`

This is an explicit alias that makes intent clear in runtime configuration code:

```cpp
buttons.reconfigureAndSync();
```

### `resetAndSync()`

```cpp
buttons.resetAndSync();
```

This:

- clears queued interaction events;
- clears transient interaction timers;
- restores configured initial latch states, counting actual transitions in the durable latch sequence;
- clears consumable latch-change flags;
- reads the current hardware level;
- establishes an edge-free baseline.

### `reset()`

The interface-level `reset()` now delegates to `resetAndSync()`.

v2 deliberately does **not** assume that every physical button becomes released just because software was reset.

### `invalidate()`

Sometimes source health is known outside the reader callback—for example, a task refreshes an MCP23017 cache once and separately knows whether the I²C transaction succeeded.

Use:

```cpp
buttons.invalidate(ButtonReadError::AcquisitionFailed);
```

This:

- marks enabled inputs invalid;
- retains stable levels;
- cancels ambiguous interaction timing;
- forces edge-free rebaseline on the next successful update.

That is the right integration point for cached/health-supervised input providers.

---

## Per-button configuration

A `ButtonPerConfig` can override timing and behavior:

```cpp
ButtonPerConfig horn{};
horn.debounce_ms = 20;
horn.short_press_ms = 100;
horn.long_press_ms = 800;
horn.double_click_ms = 300;
horn.active_low = true;
horn.enabled = true;

buttons.setPerConfig(UB::config::ButtonIndex::Horn, horn);
```

A zero timing override means:

> inherit the global setting.

### Runtime configuration is synchronized

Changing a reader, timing configuration or electrical polarity during an interaction can otherwise create fake edges or reinterpret a held level.

v2 runtime configuration APIs therefore synchronize the affected input(s) before the change becomes active.

If synchronization fails, the previous configuration is retained where possible and the operation reports failure.

---

## Latching

Latching is optional and driven by **completed interactions**.

Example: toggle a latch after a double click.

```cpp
ButtonPerConfig mode{};
mode.latch_enabled = true;
mode.latch_mode = LatchMode::Toggle;
mode.latch_on = LatchTrigger::Double;

buttons.setPerConfig(UB::config::ButtonIndex::Mode, mode);
```

Read it with:

```cpp
buttons.isLatched(id);
buttons.latchedMask();
```

Manual control:

```cpp
buttons.setLatched(id, true);
buttons.clearAllLatched();
buttons.clearLatchedMask(mask);
```

For durable change detection:

```cpp
buttons.latchChangeSequence(id);
```

Each consumer can compare the sequence with its own saved value. It starts at zero on construction and advances once per actual latch transition, modulo 2^32. The same value is available in `inputStatus(id).latch_change_sequence`. Reset never clears this counter.

Disabling clears the latch; reset restores `latch_initial`. Both count actual changes even if subsequent synchronization fails. Unchanged values produce no transition. These controls do not synthesize interaction events: disable retains queued events, while reset clears the queue.

The legacy clear-on-read helper remains available:

```cpp
buttons.getAndClearLatchedChanged(id);
```

This flag reports changes since it was last consumed or cleared by lifecycle cleanup. Disable cleanup and reset clear it even when the durable sequence advances. Manual `setLatched()` and clear operations set the flag only for actual changes.

---

## Compile-time button mapping

### v2 requires an explicit mapping

When using the convenience umbrella header:

```cpp
#define BUTTON_LIST(X) \
    X(Accelerator, 4)  \
    X(Horn,        5)  \
    X(Left,        6)  \
    X(Right,       7)

#include <Universal_Button.h>
```

If `BUTTON_LIST` is missing, compilation stops with a clear error.

This replaces the old silent `TestButton = GPIO25` fallback.

### Normal v2 namespace

Generated configuration lives in:

```cpp
UB::config
```

For example:

```cpp
UB::config::ButtonIndex::Horn
UB::config::ButtonPins::Horn
UB::config::BUTTON_PINS
UB::config::NUM_BUTTONS
```

This prevents the library's generated configuration from depending on global names.

### v1.x compatibility aliases

For source compatibility during migration, the familiar names are still exported by default:

```cpp
ButtonIndex
ButtonPins
BUTTON_PINS
NUM_BUTTONS
```

To enforce the clean v2 namespace:

```cpp
#define UB_NO_LEGACY_CONFIG_GLOBALS
#define BUTTON_LIST(X) ...
#include <Universal_Button.h>
```

New library/application code should prefer `UB::config::*`.

### Explicit-pins path

You do not need `BUTTON_LIST` when you use the core directly:

```cpp
#include <ButtonHandler.h>

constexpr uint8_t pins[] = {4, 5};
ButtonHandler<2> buttons(pins);
```

This is useful for libraries/adapters that should not depend on a project-global mapping macro.

---

## Port expanders and cached input

Universal_Button intentionally does not depend on a specific expander library.

### Simple expander callback

```cpp
static bool readFromMcp(uint8_t key)
{
    ...
    return mcp.digitalRead(pin) == LOW;
}
```

Because that callback returns **logical pressed state**, use:

```cpp
makeButtonsWithReader(readFromMcp);
```

### Electrical-level expander callback

If your callback returns the raw level instead:

```cpp
static bool readMcpHigh(uint8_t key)
{
    return mcp.digitalRead(pin) == HIGH;
}
```

use:

```cpp
makeButtonsWithElectricalReader(readMcpHigh);
```

### Cached coherent reads

For a larger project, it is often better to acquire the expander once per cycle:

```text
I²C transaction
    ↓
coherent 16-bit cache
    ↓
button callbacks read the cache
    ↓
Universal_Button debounce
```

If cache refresh fails, call `invalidate()` or use a validity-aware callback rather than manufacturing released inputs.

Example 05 demonstrates the cached approach.

---

## API reference

### Package version macros

Use `UNIVERSAL_BUTTON_VERSION`, `UNIVERSAL_BUTTON_VERSION_MAJOR`, `UNIVERSAL_BUTTON_VERSION_MINOR`, and `UNIVERSAL_BUTTON_VERSION_PATCH`. Generic `LIBRARY_VERSION*` names are intentionally absent from this development baseline so version metadata cannot collide with another library.

### Core lifecycle

```cpp
update();
update(now_ms);

sync();
sync(now_ms);
reconfigureAndSync();
resetAndSync();
reset();
invalidate();
```

### Stable state

```cpp
isPressed(id);
isLongHeld(id);
pressedMask();
snapshot(bitset);
forEach(...);
```

### Completed interaction compatibility API

```cpp
getPressType(id);
peekPressType(id);
getLastPressDuration(id);
```

### Detailed event queue

```cpp
popEvent(event);
peekEvent(event);
pendingEventCount();
eventSequence();
droppedEventCount();
eventOverflowed();
clearEventOverflow();
```

### Input health

```cpp
configured();
configError();
valid();
hasSample();

valid(id);
hasSample(id);
sampleMs(id);
sequence(id);
changeSequence(id);
generation(id);
inputStatus(id);
status();
```

### Configuration

```cpp
setGlobalTiming(...);
setTiming(...);
setPerConfig(...);
enable(...);
setActiveLow(...);
setTimeFn(...);
```

### Reader replacement

Logical pressed-state:

```cpp
setReadPinFn(...);
setReadFn(...);
setReadResultPinFn(...);
setReadResultFn(...);
```

Electrical HIGH/LOW:

```cpp
setElectricalReadPinFn(...);
setElectricalReadFn(...);
setElectricalReadResultPinFn(...);
setElectricalReadResultFn(...);
```

### Latching

```cpp
isLatched(id);
setLatched(id, state);
clearAllLatched();
clearLatchedMask(mask);
latchedMask();
getAndClearLatchedChanged(id);
latchChangeSequence(id);
```

---

## Examples

The example progression remains intentionally small.

### 01_BasicButton

The shortest path from wiring to a debounced `isPressed()` result.

### 02_PressType

Short, long and double-click classification using the simple compatibility API.

### 03_LocalEnum

Shows how to use `ButtonHandler<N>` directly with an explicit local enum and pins array, without `BUTTON_LIST`.

### 04_PortExpander

Uses an MCP23017 callback as the input source.

### 05_CachedRead

Reads both MCP23017 ports once per loop and serves button reads from the coherent cache.

### 06_Latching

Shows toggle/set/reset-style latched behavior.

The examples are intended to teach the public API, not every advanced diagnostic feature. The remainder of this README is the technical reference.

---

## Testing

The repository has a dedicated `test/` directory rather than using an old `platformio.ci.ini` pattern.

Run the complete host-side validation suite with:

```bash
./test/run_host_checks.sh
```

The complete suite requires genuine GNU GCC, Clang, AddressSanitizer and UndefinedBehaviorSanitizer; unavailable capabilities fail the run. `GCC_CXX=/absolute/path/to/g++` selects GCC, with `GNU_CXX` retained as a fallback. `CLANG_CXX` selects Clang. The macOS Clang `g++` alias is not accepted as GCC. `sh test/run_host_checks.sh` is also supported.

Individual gates are also available:

```bash
./test/run_native_tests.sh
CXX=clang++ ./test/run_native_tests.sh
./test/run_sanitizers.sh
./test/check_examples_host.sh
./test/check_release_contracts.sh
```

The deterministic suite covers:

- the original callback-polarity defect;
- logical vs electrical reader truth tables;
- active-low and active-high handling;
- checked read failure and recovery;
- externally reported source invalidation;
- missing-reader behavior;
- bounce/debounce;
- exact short/long threshold boundaries;
- double-click timing;
- LongStarted vs LongReleased;
- held-state semantics;
- event preservation;
- queue overflow/loss diagnostics;
- reset/sync behavior;
- runtime reader reconfiguration;
- timing validation;
- per-button timing;
- latching and durable latch changes;
- `millis()` rollover;
- multiple-button independence;
- freshness/change sequences;
- malformed indices;
- application context callbacks;
- complete small state truth tables.

The GitHub Actions matrix separately compiles portable usage across the supported board families and compiles the public examples for ESP32-S3.

---

## Migrating from v1.7.x

v2.0.0 is intentionally a major release because two previously ambiguous behaviors are corrected rather than preserved forever.

### 1. Explicit `BUTTON_LIST` is mandatory with `Universal_Button.h`

**v1.x:** missing mapping silently created a GPIO25 `TestButton`.

**v2:** compilation fails with a clear message.

If you do not want a mapping macro, include `ButtonHandler.h` directly and pass an explicit pins array.

### 2. Callback readers now follow their documented contract

**v1.x documentation:** callback returns `true` when pressed.

**v1.x implementation:** that result was polarity-transformed again.

**v2:** `makeButtonsWithReader(...)` means exactly what it says—`true` is pressed.

If your old callback actually returned raw electrical HIGH/LOW, migrate it to:

```cpp
makeButtonsWithElectricalReader(...)
```

### 3. Events are queued instead of stored in one slot

`getPressType()` still works for simple sketches.

For production applications, prefer `popEvent()` and monitor overflow/sequence metadata.

### 4. Long press now has two explicit moments

- `LongStarted`: threshold reached while held;
- `LongReleased`: completed long interaction on release.

`ButtonPressType::Long` remains release-based for compatibility.

### 5. `reset()` synchronizes current hardware

v2 no longer assumes reset means every physical button is released.

For a known external hardware/source failure, use:

```cpp
invalidate();
```

and let the next valid acquisition rebaseline safely.

### 6. Canonical generated mapping is namespaced

Prefer:

```cpp
UB::config::ButtonIndex
UB::config::NUM_BUTTONS
```

Legacy global aliases remain available unless `UB_NO_LEGACY_CONFIG_GLOBALS` is defined.

---

## Application integration

Universal_Button separates input acquisition from application policy and transport.

A useful adapter shape is:

```text
MCP23017 / hardware cache
       ↓
source-health result
       ↓
Universal_Button
       ├── stable level
       ├── valid / sample_ms / sequence
       ├── change_sequence / generation
       └── bounded interaction events
                 ↓
application adapter / transport
                 ↓
application safety policy / UI logic
```

### Stable button state

A latest-state transport should carry stable debounced state and durable metadata.

It should **not** rely on a one-cycle event bit surviving scheduling delays.

### Interaction events

If every event must be delivered, consume Universal_Button's queue into an application event queue, notification, journal or monotonic counters.

### External MCP health

If a cache-refresh task knows the MCP23017 transaction failed, it can call:

```cpp
buttons.invalidate(ButtonReadError::AcquisitionFailed);
```

When the cache becomes healthy again, the next `update()` rebaselines without generating fake edges.

This keeps hardware health in the adapter and button semantics in the library.

---

## Limitations and design notes

### The event queue is bounded

This is intentional. Embedded systems should have explicit resource and overflow behavior.

Increase `UB_EVENT_QUEUE_CAPACITY` if your application legitimately allows a larger consumer delay, or drain events into an application-level queue.

### Plain bool callbacks cannot report hardware failure

Use a result reader or `invalidate()` when the hardware/source has an independent health signal.

### Button acquisition is per button

Universal_Button calls the configured reader once per enabled logical button during `update()`.

If several contacts must represent one coherent hardware snapshot, acquire/cache that snapshot outside the library first, as shown by Example 05.

### This is not a safety policy

Input validity helps a safety architecture make a correct decision, but the library does not decide whether motion/power is permitted.

### `ButtonPressType::Long` remains release-based

That is deliberate for compatibility. Use `isLongHeld()` or `ButtonEventType::LongStarted` for threshold-time behavior.

---

## Repository structure

```text
Universal_Button/
├── .github/workflows/ci.yml
├── examples/
│   ├── 01_BasicButton/
│   ├── 02_PressType/
│   ├── 03_LocalEnum/
│   ├── 04_PortExpander/
│   ├── 05_CachedRead/
│   └── 06_Latching/
├── src/
│   ├── ButtonCompatibility.h
│   ├── ButtonHandler.h
│   ├── ButtonHandler_Config.h
│   ├── ButtonTypes.h
│   ├── IButtonHandler.h
│   ├── Universal_Button.h
│   └── Universal_Button_Utils.h
├── test/
│   ├── host_stubs/
│   ├── portable_compile/
│   ├── test_button_handler.cpp
│   └── test scripts...
├── CHANGELOG.md
├── RELEASE_CHECKLIST.md
├── keywords.txt
├── library.json
├── library.properties
├── LICENSE
├── platformio.ini
└── README.md
```

---

## Version history

Current version:

```text
2.0.1
```

See [CHANGELOG.md](CHANGELOG.md) for detailed release history.

---

## License

Universal_Button is released under the **MIT License**. See [LICENSE](LICENSE).

Copyright © 2026 Little Man Builds.
