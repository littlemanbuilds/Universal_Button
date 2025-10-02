# Universal_Button

Generic multi‑button handler for Arduino/ESP platforms with solid debounce and short/long press events. The library is **device‑agnostic** (works with plain GPIO) and can also read buttons from external sources (e.g., MCP23017 port expanders) by plugging in a custom reader function. It includes enum‑friendly helpers, per‑button overrides, **exact last‑press duration**, and a clean “easy header” with factories.  
New in **v1.4.0**: optional **time source injection (`TimeFn`)** and Arduino‑free factories so you can use RTOS clocks (e.g., FreeRTOS ticks) or any millisecond source without modifying your sketch structure.

> **Author:** Little Man Builds  
> **License:** MIT

---

## Highlights

- **Rock‑solid debounce**
- **Short/Long press detection**
- **Exact last press duration**
- **Convenience helpers**: enum overloads, `pressedMask()`, `snapshot()`, `forEach()`
- **Pluggable readers** (GPIO, MCP, custom; pointer or context‑aware)
- **Header‑only** integration
- **Optional TimeFn** (inject custom millisecond clock; falls back to `millis()`)

---

## Contents

- [Universal\_Button](#universal_button)
  - [Highlights](#highlights)
  - [Contents](#contents)
  - [Installation](#installation)
  - [Concepts](#concepts)
  - [Configuration Mapping](#configuration-mapping)
  - [Quick Use (Easy Header)](#quick-use-easy-header)
  - [Timing Model \& TimeFn (NEW)](#timing-model--timefn-new)
    - [Ways to supply time](#ways-to-supply-time)
  - [API Reference](#api-reference)
    - [Types](#types)
    - [Interface: `IButtonHandler`](#interface-ibuttonhandler)
    - [Concrete: `ButtonHandler<N>`](#concrete-buttonhandlern)
    - [Factories (Easy Header)](#factories-easy-header)
    - [Utils](#utils)
  - [Examples](#examples)
  - [Mixed Inputs (GPIO + Expander)](#mixed-inputs-gpio--expander)
  - [Performance Notes](#performance-notes)
  - [FAQ](#faq)
  - [License](#license)

---

## Installation

**Arduino IDE:** search **UNIVERSAL_BUTTON** in Library Manager.  
**Manual:** download the release ZIP → *Sketch → Include Library → Add .ZIP Library…*  
**PlatformIO:** Search libraries and add to `lib_deps` or install from ZIP.  
> The core library **does not** force an MCP dependency. Only examples that use MCP include it.

---

## Concepts

- **Key**: the value stored in `BUTTON_PINS[i]` (usually an MCU GPIO number, but it can be any 8‑bit token you choose).  
- **Logical index**: position of the button in your mapping (0..`NUM_BUTTONS`‑1).  
- **Raw state**: the immediate (possibly bouncing) reading.  
- **Committed state**: the debounced, stable state after `debounce_ms` of stability.

Debounce uses a “raw/committed” split:
- Track the **last raw state** and the **time it changed**.  
- When the raw state stays unchanged for `debounce_ms`, we **commit** it and generate an event on release based on **press duration** (`short_press_ms`, `long_press_ms`), and **store the exact duration** for later retrieval with `getLastPressDuration()`.

---

## Configuration Mapping

Define your buttons once using `BUTTON_LIST(X)` **before including** `Universal_Button.h` (or any header that uses the mapping).

```cpp
#define BUTTON_LIST(X) \
  X(Start, 4) \
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

## Timing Model & TimeFn (NEW)

By default, the library timestamps with **`millis()`**. In v1.4.0 you can inject your own millisecond **time source** (e.g., FreeRTOS ticks) without changing the rest of your sketch.

### Ways to supply time
1. **Setter (works with any factory):**
```cpp
btns.setTimeFn([]() -> uint32_t {
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
});
```

2. **Factory overloads (one‑liner):**
```cpp
static Button btns = makeButtons(
  /*timing=*/{},
  /*skipPinInit=*/false,
  /*timeFn=*/[]() -> uint32_t { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); });
```

> If no `TimeFn` is provided, the handler uses `millis()` internally. All debounce and press durations are measured on the chosen time base.

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

Per‑button overrides (ButtonPerConfig) are `uint16_t` for compact storage; global timings are `uint32_t`.

### Interface: `IButtonHandler`
```cpp
class IButtonHandler {
public:
  virtual ~IButtonHandler() noexcept = default;
  virtual void update() noexcept = 0;
  [[nodiscard]] virtual bool isPressed(uint8_t id) const noexcept = 0;
  virtual ButtonPressType getPressType(uint8_t id) noexcept = 0;
  virtual void reset() noexcept { }
  [[nodiscard]] virtual uint32_t getLastPressDuration(uint8_t) const noexcept { return 0U; }
  // Optional helpers may be provided by concrete types
};
```

### Concrete: `ButtonHandler<N>`

**Constructors (pointers, not std::function):**
```cpp
// GPIO by default (LOW=pressed). Optionally skip pin init. Optional TimeFn.
ButtonHandler(const uint8_t (&pins)[N],
              ButtonTimingConfig timing = {},
              bool skipPinInit = false,
              uint32_t (*TimeFn)() = nullptr);

// External per‑pin reader (bool(uint8_t)), optional TimeFn.
ButtonHandler(const uint8_t (&pins)[N],
              bool (*readPin)(uint8_t),
              ButtonTimingConfig timing = {},
              bool skipPinInit = true,
              uint32_t (*TimeFn)() = nullptr);

// Context‑aware reader (bool(void*, uint8_t)), optional TimeFn.
ButtonHandler(const uint8_t (&pins)[N],
              bool (*read)(void*, uint8_t), void* ctx,
              ButtonTimingConfig timing = {},
              bool skipPinInit = true,
              uint32_t (*TimeFn)() = nullptr);
```

**Key methods:**
```cpp
void update() noexcept;                  // calls update(now) using TimeFn or millis()
void update(uint32_t now) noexcept;      // explicit timestamp

[[nodiscard]] bool isPressed(uint8_t id) const noexcept;
template <typename E> bool isPressed(E id) const noexcept;  // enum‑friendly

ButtonPressType getPressType(uint8_t id) noexcept;
template <typename E> ButtonPressType getPressType(E id) noexcept;

[[nodiscard]] uint32_t getLastPressDuration(uint8_t id) const noexcept;
template <typename E> [[nodiscard]] uint32_t getLastPressDuration(E id) const noexcept;

void reset() noexcept;

// Runtime configuration
void setTiming(ButtonTimingConfig);
void setPerConfig(uint8_t id, const ButtonPerConfig&);
void enable(uint8_t id, bool en);
void setReadPinFn(bool (*readPin)(uint8_t));
void setReadFn(bool (*read)(void*, uint8_t), void* ctx);
void setTimeFn(uint32_t (*TimeFn)());
```

**Helpers:**
```cpp
static constexpr uint8_t sizeStatic() noexcept;
[[nodiscard]] uint8_t size() const noexcept;

[[nodiscard]] uint32_t pressedMask() const noexcept;  // N <= 32
template <size_t M> void snapshot(std::bitset<M>& out) const noexcept;
template <typename F> void forEach(F&& f) const noexcept;   // f(index, pressed)
```

### Factories (Easy Header)

Declared in **`Universal_Button.h`** (original factories kept; new overloads add `timeFn` as last arg):

- **Config‑driven alias:**
  ```cpp
  using Button = ButtonHandler<NUM_BUTTONS>;
  Button makeButtons(ButtonTimingConfig t = {}, bool skipPinInit = false);
  Button makeButtons(ButtonTimingConfig t, bool skipPinInit, Button::TimeFn timeFn); // NEW overload
  ```

- **External reader (fast pointer):**
  ```cpp
  Button makeButtonsWithReader(bool (*read)(uint8_t), ButtonTimingConfig t = {}, bool skipPinInit = true);
  Button makeButtonsWithReader(bool (*read)(uint8_t), ButtonTimingConfig t, bool skipPinInit, Button::TimeFn timeFn); // NEW
  ```

- **Context‑aware reader:**
  ```cpp
  Button makeButtonsWithReaderCtx(bool (*read)(void*, uint8_t), void* ctx, ButtonTimingConfig t = {}, bool skipPinInit = true);
  Button makeButtonsWithReaderCtx(bool (*read)(void*, uint8_t), void* ctx, ButtonTimingConfig t, bool skipPinInit, Button::TimeFn timeFn); // NEW
  ```

- **Explicit pins:**
  ```cpp
  template <size_t N>
  ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N], ButtonTimingConfig t = {}, bool skipPinInit = false);
  template <size_t N>
  ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N], ButtonTimingConfig t, bool skipPinInit, typename ButtonHandler<N>::TimeFn timeFn); // NEW

  template <size_t N>
  ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N], bool (*read)(uint8_t), ButtonTimingConfig t = {}, bool skipPinInit = true);
  template <size_t N>
  ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N], bool (*read)(uint8_t), ButtonTimingConfig t, bool skipPinInit, typename ButtonHandler<N>::TimeFn timeFn); // NEW

  template <size_t N>
  ButtonHandler<N> makeButtonsWithPinsAndReaderCtx(const uint8_t (&pins)[N], bool (*read)(void*, uint8_t), void* ctx, ButtonTimingConfig t = {}, bool skipPinInit = true);
  template <size_t N>
  ButtonHandler<N> makeButtonsWithPinsAndReaderCtx(const uint8_t (&pins)[N], bool (*read)(void*, uint8_t), void* ctx, ButtonTimingConfig t, bool skipPinInit, typename ButtonHandler<N>::TimeFn timeFn); // NEW
  ```

### Utils

In **`Universal_Button_Utils.h`** (device‑agnostic):
```cpp
namespace UB::util {
  uint8_t indexFromKey(uint8_t key); // for config mapping (BUTTON_PINS/NUM_BUTTONS)
  template <size_t N>
  uint8_t indexFromKeyIn(const uint8_t (&pins)[N], uint8_t key); // for explicit pins
}
```

---

## Examples

Existing examples require **no change**. To demonstrate TimeFn you may optionally add:

**Inject FreeRTOS ticks as time:**
```cpp
btns.setTimeFn([]() -> uint32_t {
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
});
```

**Or use a factory overload with `timeFn`:**
```cpp
static Button btns = makeButtons(
  /*timing=*/{},
  /*skipPinInit=*/false,
  /*timeFn=*/[]() -> uint32_t { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); });
```

---

## Mixed Inputs (GPIO + Expander)

Build a **composite reader** and set `skipPinInit=true`. Manually call `pinMode(INPUT_PULLUP)` for GPIO pins, and configure expander pins on the device. The library needs no changes—just route each key to the right source in your reader.

---

## Performance Notes

- **GPIO** reads are O(1) and cheap—no caching needed.
- **I²C/SPI expanders**: consider cached snapshots to reduce bus traffic and get coherent “chord” readings (A+B together).
- The debounce algorithm is independent of the reader; call `update()` at a steady cadence (e.g., every 5–10 ms).
- Use pointer readers (`bool(*)(uint8_t)` / `bool(*)(void*,uint8_t)`) for low overhead.

---

## FAQ

**Q: Why are my logs missing at startup?**  
If using native USB (e.g., ESP32‑S3 USB‑CDC), consider a bounded wait:
```cpp
Serial.begin(115200);
unsigned long t0 = millis();
while (!Serial && millis() - t0 < 2000) {}
```

**Q: Do I need to modify the interface to use `reset()` or `getLastPressDuration()`?**  
No. The base interface provides defaults for compatibility; `ButtonHandler<N>` implements full behavior.

**Q: Can I invert polarity per button?**  
Yes — `ButtonPerConfig.active_low` (default `true`). Use `setPerConfig(id, cfg)` to override per button.

---

## License

MIT © Little Man Builds

Contributions welcome! Please keep Doxygen comments consistent with the interface and follow the established naming conventions.
