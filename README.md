# Universal_Button

Generic multi-button handler for Arduino/ESP platforms with solid debounce and short/long press events. The library is **device-agnostic** by default (works with plain GPIO), and can read buttons from external sources (e.g., MCP23017 port expanders) by plugging in a custom reader function. It includes enum-friendly helpers, per-button overrides, **exact last-press duration**, and a clean “easy” header with factories.

> **Author:** Little Man Builds  
> **License:** MIT  
> **Version:** `1.0.0`

---

## Highlights

- **Rock-solid debounce** using *raw* vs *committed* state with time-stable windows.
- **Short/Long press detection** (configurable timings).
- **Exact last press duration** via `getLastPressDuration(id)`; and **live hold time** via `heldMillis(id)`.
- **Pluggable readers**: read from GPIO *or* any external device via a callback.
- **Function-pointer fast path** for small code size, or `std::function` for flexibility.
- **Per-button overrides** (debounce/thresholds, polarity, enable/disable).
- **Convenience helpers**: enum overloads, `peekPressType`, `clearPressType`, `heldMillis`, `reset`, `getLastPressDuration`.
- **Header-only** usage for fast integration.
- **Examples included**: basic polling, press types, MCP23017 reader, **cached MCP snapshot**.

---

## Contents

- [Universal\_Button](#universal_button)
  - [Highlights](#highlights)
  - [Contents](#contents)
  - [Install](#install)
    - [Arduino IDE](#arduino-ide)
    - [PlatformIO](#platformio)
  - [Concepts](#concepts)
  - [Files \& Structure](#files--structure)
  - [Configuration Mapping](#configuration-mapping)
  - [Quick Use (Easy Header)](#quick-use-easy-header)
  - [API Reference](#api-reference)
    - [Types](#types)
    - [Interface: `IButtonHandler`](#interface-ibuttonhandler)
    - [Concrete: `ButtonHandler<N>`](#concrete-buttonhandlern)
    - [Factories (Easy Header)](#factories-easy-header)
    - [Utils](#utils)
  - [Examples](#examples)
    - [1) `Basic_Button`](#1-basic_button)
    - [2) `Press_Type`](#2-press_type)
    - [3) `Port_Expander` (MCP23017)](#3-port_expander-mcp23017)
    - [4) `Cached_Read` (MCP23017 snapshot)](#4-cached_read-mcp23017-snapshot)
    - [5) `Durations` (exact ms on release)](#5-durations-exact-ms-on-release)
    - [6) `Per-button overrides` (timing/polarity/enable)](#6-per-button-overrides-timingpolarityenable)
  - [Mixed Inputs (GPIO + Expander)](#mixed-inputs-gpio--expander)
  - [Performance Notes](#performance-notes)
  - [FAQ](#faq)
  - [License](#license)

---

## Install

### Arduino IDE
1. Copy the `src/` folder and library manifest files into your `Arduino/libraries/Universal_Button` directory.  
2. Restart the IDE.

### PlatformIO
```ini
; example platformio.ini
[env:esp32s3]
platform = espressif32
framework = arduino
board = esp32-s3-devkitc-1
lib_deps =
    ; If you use the MCP23017 examples:
    adafruit/Adafruit MCP23017 Arduino Library
```
> The core library **does not** force an MCP dependency. Only examples that use MCP include it.

---

## Concepts

- **Key**: the value stored in `BUTTON_PINS[i]` (usually an MCU GPIO number, but it can be any 8-bit token you choose).  
- **Logical index**: position of the button in your mapping (0..`NUM_BUTTONS`-1).  
- **Raw state**: the immediate (possibly bouncing) reading.  
- **Committed state**: the debounced, stable state after `debounce_ms` of stability.

Debounce uses a “raw/committed” split:
- We track the **last raw state** and the **time it changed**.  
- When the raw state stays unchanged for `debounce_ms`, we **commit** it and generate an event on release based on **press duration** (`short_press_ms`, `long_press_ms`), and **store the exact duration** for later retrieval with `getLastPressDuration()`.

---

## Files & Structure

```
Universal_Button/
├─ src/
│  ├─ ButtonTypes.h                # ButtonPressType, ButtonTimingConfig, ButtonPerConfig
│  ├─ IButtonHandler.h             # Minimal abstract interface (default no-op reset & duration)
│  ├─ ButtonHandler.h              # Template handler with debounce/events + helpers + last duration
│  ├─ ButtonHandler_Config.h       # Mapping via BUTTON_LIST macro
│  ├─ Universal_Button_Utils.h     # Small mapping helpers (device-agnostic)
│  └─ Universal_Button.h           # Easy header + factories
├─ examples/
│  ├─ 01_Basic_Button/01_Basic_Button.ino
│  ├─ 02_Press_Type/02_Press_Type.ino
│  ├─ 03_Local_Enum/03_Local_Enum.ino
│  ├─ 04_Port_Expander/04_Port_Expander.ino
│  └─ 05_Cached_Read/05_Cached_Read.ino
├─ library.properties
├─ library.json
└─ keywords.txt
```

---

## Configuration Mapping

Define your buttons once using `BUTTON_LIST(X)` **before including** `Universal_Button.h` (or any header that uses the mapping).

```cpp
#define BUTTON_LIST(X) \
  X(Start, 4)          \
  X(Stop,  5)

#include <Universal_Button.h>
```

This generates:
- `inline constexpr uint8_t BUTTON_PINS[] = {4, 5};`
- `inline constexpr size_t NUM_BUTTONS = 2;`
- `enum class ButtonIndex : uint8_t { Start, Stop, _COUNT };`
- `struct ButtonPins { static constexpr uint8_t Start=4; static constexpr uint8_t Stop=5; };`

A **default** mapping is provided (single `TestButton` on pin 25) if you do nothing.

---

## Quick Use (Easy Header)

```cpp
#define BUTTON_LIST(X) X(TestButton, 25)
#include <Universal_Button.h>

static Button btns = makeButtons();  // uses BUTTON_PINS/NUM_BUTTONS

void setup() { Serial.begin(115200); }
void loop() {
  btns.update();
  if (btns.isPressed(ButtonIndex::TestButton)) {
    Serial.println("Pressed!");
  }
  delay(10);
}
```

Custom timings:
```cpp
constexpr ButtonTimingConfig kTiming{50, 300, 1500};
static Button btns = makeButtons(kTiming);
```

External reader (e.g., expander):
```cpp
static bool readFunc(uint8_t key) { return digitalRead(key) == LOW; }
static Button btns = makeButtonsWithReader(readFunc, /*timing=*/{}, /*skipPinInit=*/true);
```

Explicit pins (no config):
```cpp
constexpr uint8_t MY_PINS[] = {4, 5, 18};
auto btns = makeButtonsWithPins(MY_PINS);
```

---

## API Reference

### Types
Declared in **`ButtonTypes.h`**:

```cpp
enum class ButtonPressType : uint8_t { None, Short, Long };

struct ButtonTimingConfig {
  uint32_t debounce_ms    = 30;
  uint32_t short_press_ms = 200;
  uint32_t long_press_ms  = 1000;
};

struct ButtonPerConfig {
  uint16_t debounce_ms    = 0;   // 0 = use global
  uint16_t short_press_ms = 0;
  uint16_t long_press_ms  = 0;
  bool     active_low     = true;
  bool     enabled        = true;
};
```

### Interface: `IButtonHandler`

```cpp
class IButtonHandler {
public:
  virtual ~IButtonHandler() noexcept = default;
  virtual void update() noexcept = 0;
  [[nodiscard]] virtual bool isPressed(uint8_t id) const noexcept = 0;
  virtual ButtonPressType getPressType(uint8_t id) noexcept = 0;
  // Optional: default no-op for compatibility
  virtual void reset() noexcept { }
  // Optional: default returns 0 (no duration info)
  [[nodiscard]] virtual uint32_t getLastPressDuration(uint8_t /*id*/) const noexcept { return 0U; }
};
```

### Concrete: `ButtonHandler<N>`

Constructors (in **`ButtonHandler.h`**):

```cpp
// GPIO by default (LOW=pressed), will pinMode(INPUT_PULLUP)
ButtonHandler(const uint8_t (&pins)[N],
              std::function<bool(uint8_t)> read = nullptr,
              ButtonTimingConfig timing = {});

// External reader + optional skip of pin init
ButtonHandler(const uint8_t (&pins)[N],
              std::function<bool(uint8_t)> read,
              ButtonTimingConfig timing,
              bool skipPinInit);
```

Key methods (superset of interface):
```cpp
void update() noexcept;

[[nodiscard]] bool isPressed(uint8_t id) const noexcept;
template <typename E, enable_if_enum> bool isPressed(E id) const noexcept;

ButtonPressType getPressType(uint8_t id) noexcept;
template <typename E, enable_if_enum> ButtonPressType getPressType(E id) noexcept;

[[nodiscard]] ButtonPressType peekPressType(uint8_t id) const noexcept;
void clearPressType(uint8_t id) noexcept;
[[nodiscard]] uint32_t heldMillis(uint8_t id) const noexcept;
void reset() noexcept;

// Exact duration of last completed press (ms)
[[nodiscard]] uint32_t getLastPressDuration(uint8_t id) const noexcept;
template <typename E, enable_if_enum>
[[nodiscard]] uint32_t getLastPressDuration(E id) const noexcept;
```

### Factories (Easy Header)

In **`Universal_Button.h`**:

- Config-driven alias:
  ```cpp
  using Button = ButtonHandler<NUM_BUTTONS>;
  Button makeButtons(ButtonTimingConfig t = {});
  Button makeButtonsWithReader(std::function<bool(uint8_t)> read,
                               ButtonTimingConfig t = {}, bool skipPinInit = true);
  Button makeButtonsWithReader(bool (*read)(uint8_t),
                               ButtonTimingConfig t = {}, bool skipPinInit = true);
  ```
- Explicit pins (no config):
  ```cpp
  template <size_t N>
  ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N],
                                       ButtonTimingConfig t = {});

  template <size_t N>
  ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                std::function<bool(uint8_t)> read,
                                                ButtonTimingConfig t = {}, bool skipPinInit = true);
  template <size_t N>
  ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                bool (*read)(uint8_t),
                                                ButtonTimingConfig t = {}, bool skipPinInit = true);
  ```

### Utils

In **`Universal_Button_Utils.h`** (device-agnostic):
```cpp
namespace UB::util {
  uint8_t indexFromKey(uint8_t key); // for config mapping (BUTTON_PINS/NUM_BUTTONS)

  template <size_t N>
  uint8_t indexFromKeyIn(const uint8_t (&pins)[N], uint8_t key); // for explicit pins
}
```

---

## Examples

### 1) `Basic_Button`
Polling `isPressed()` using the config mapping.

```cpp
#define BUTTON_LIST(X) X(TestButton, 25)
#include <Universal_Button.h>

static Button btns = makeButtons();
void loop() {
  btns.update();
  if (btns.isPressed(ButtonIndex::TestButton)) {
    Serial.println("Pressed!");
  }
  delay(10);
}
```

### 2) `Press_Type`
Short/long events with custom thresholds.

```cpp
#define BUTTON_LIST(X) X(TestButton, 25)
#include <Universal_Button.h>

constexpr ButtonTimingConfig kTiming{50, 300, 1500};
static Button btns = makeButtons(kTiming);

void loop() {
  btns.update();
  switch (btns.getPressType(ButtonIndex::TestButton)) {
    case ButtonPressType::Short: Serial.println("Short"); break;
    case ButtonPressType::Long:  Serial.println("Long");  break;
    default: break;
  }
  delay(10);
}
```

### 3) `Port_Expander` (MCP23017)
Use a simple reader function that routes to MCP or GPIO.

```cpp
#define BUTTON_LIST(X) X(Start, 25) X(Stop, 26) X(Mode, 27)
#include <Universal_Button.h>
#include <Universal_Button_Utils.h>
#include <Adafruit_MCP23017.h>

constexpr uint8_t MCP_PINS[NUM_BUTTONS] = {0, 1, 8};
Adafruit_MCP23017 mcp;

static bool readFromMcp(uint8_t key) {
  const uint8_t idx = UB::util::indexFromKey(key);
  if (idx < NUM_BUTTONS) return mcp.digitalRead(MCP_PINS[idx]) == LOW;
  return digitalRead(key) == LOW;
}

static Button btns = makeButtonsWithReader(readFromMcp, {}, true);
```

### 4) `Cached_Read` (MCP23017 snapshot)
Take one I²C snapshot per loop (GPIOA+GPIOB) and serve all reads from it for coherence and speed.

```cpp
struct Snap { uint8_t A{0xFF}, B{0xFF}; inline uint8_t getBit(uint8_t p) const {
  return (p<8) ? ((A>>p)&1) : ((B>>(p-8))&1);
}} snap;

static bool readFromSnapshot(uint8_t key) {
  const uint8_t idx = UB::util::indexFromKey(key);
  if (idx < NUM_BUTTONS) return snap.getBit(MCP_PINS[idx]) == 0; // LOW = pressed
  return digitalRead(key) == LOW;
}
```

### 5) `Durations` (exact ms on release)
Get the **exact** duration of the last completed press, alongside classification.

```cpp
#define BUTTON_LIST(X) X(TestButton, 25)
#include <Universal_Button.h>

constexpr ButtonTimingConfig kTiming{50, 300, 1500};
static Button btns = makeButtons(kTiming);

void loop() {
  btns.update();

  // Live holding time while pressed (0 if not pressed)
  if (btns.isPressed(ButtonIndex::TestButton)) {
    Serial.printf("Holding: %u ms\n", btns.heldMillis(ButtonIndex::TestButton));
  }

  // Released events + exact duration
  const auto evt = btns.getPressType(ButtonIndex::TestButton);
  if (evt != ButtonPressType::None) {
    const uint32_t ms = btns.getLastPressDuration(ButtonIndex::TestButton);
    Serial.printf("Released after %u ms (%s)\n",
                  ms,
                  evt == ButtonPressType::Long ? "Long" :
                  evt == ButtonPressType::Short ? "Short" : "None");
  }

  delay(10);
}
```

**Please note**: The library’s “exact ms” is exact with respect to the debounced model and the scheduling granularity. With a 1 ms update and/or micros() as the time base, it’s effectively exact for human interaction without adding complexity.

### 6) `Per-button overrides` (timing/polarity/enable)

Override **just one** button (e.g., `ButtonTest3`) with different timings; others keep the global defaults:

```cpp
// Global defaults
constexpr ButtonTimingConfig kTiming{50, 300, 1500};
static Button btns = makeButtons(kTiming);

void setup() {
  // ... construct/configure your reader here if needed ...

  ButtonPerConfig one{};
  one.debounce_ms    = 60;   // non-zero => override
  one.short_press_ms = 400;
  one.long_press_ms  = 1800;
  // one.active_low   = true;  // keep LOW=pressed (default)
  // one.enabled      = true;  // keep enabled (default)

  btns.setPerConfig(static_cast<uint8_t>(ButtonIndex::ButtonTest3), one);
}
```

Override **all Port-B** expander buttons (pins `8..15`) by checking your `MCP_PINS[]` mapping:

```cpp
// Example mapping for three buttons; ButtonTest3 is on Port B (pin 8)
constexpr uint8_t MCP_PINS[NUM_BUTTONS] = { 0, 1, 8 };

void setup() {
  ButtonPerConfig portB{};
  portB.debounce_ms    = 60;
  portB.short_press_ms = 400;
  portB.long_press_ms  = 1800;

  for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
    if (MCP_PINS[i] >= 8) {          // Port B
      btns.setPerConfig(i, portB);   // override those only
    }
  }
}
```

---

## Mixed Inputs (GPIO + Expander)

Build with a **composite reader** and set `skipPinInit=true`. Manually call `pinMode(INPUT_PULLUP)` for GPIO pins, and configure expander pins on the device. The library needs no changes—just route each key to the right source in your reader.

---

## Performance Notes

- **GPIO** reads are O(1) and cheap—no caching needed.  
- **I²C/SPI expanders**: consider cached snapshots to reduce bus traffic and get coherent “chord” readings (A+B together).  
- The debounce algorithm is independent of the reader; call `update()` at a steady cadence (e.g., every 5–10 ms).  
- Use `setReadPtr(bool(*)(uint8_t))` for a code-size-friendly reader on MCUs.

---

## FAQ

**Q: Why are my logs missing at startup?**  
A: If using native USB (e.g., ESP32-S3 USB-CDC), consider a bounded wait:  
```cpp
Serial.begin(115200);
unsigned long t0 = millis();
while (!Serial && millis() - t0 < 2000) {}
```

**Q: Do I need to modify the interface to use `reset()` or `getLastPressDuration()`?**  
A: No. The base interface provides default implementations so polymorphic code remains source-compatible; `ButtonHandler<N>` implements full behavior.

**Q: Can I invert polarity per button?**  
A: Yes — `ButtonPerConfig.active_low` (default `true`). Use `setPerConfig(id, cfg)` to override per button.

---

## License

MIT © Little Man Builds

Contributions welcome! Please keep Doxygen comments consistent with the interface and follow the established naming conventions.
