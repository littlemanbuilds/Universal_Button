/**
 * MIT License
 *
 * @brief Serve multiple button reads from one coherent MCP23017 cache refresh.
 *
 * @file 05_CachedRead.ino
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

// Define the logical button names before including <Universal_Button.h>.
// With a port expander, these numbers are just unique keys used by the callback.
#define BUTTON_LIST(X) \
    X(TestButton1, 6)  \
    X(TestButton2, 7)  \
    X(TestButton3, 8)

// Use the namespaced mapping without duplicate legacy global names.
#define UB_NO_LEGACY_CONFIG_GLOBALS
#include <Universal_Button.h>
#include <Universal_Button_Utils.h>

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

using namespace UB::config;

// MCP23017 wiring/config.
constexpr uint8_t MCP_ADDR = 0x20;

// Map each logical button (by enum order) to MCP pin 0..15 (0..7=A, 8..15=B).
constexpr uint8_t MCP_PINS[NUM_BUTTONS] = {
    0, ///< TestButton1 -> GPA0.
    1, ///< TestButton2 -> GPA1.
    8  ///< TestButton3 -> GPB0.
};
static_assert(NUM_BUTTONS == (sizeof(MCP_PINS) / sizeof(MCP_PINS[0])),
              "MCP_PINS size must match NUM_BUTTONS");

// Custom timings (debounce, short, long) in milliseconds.
constexpr ButtonTimingConfig kTiming{50, 300, 1500};

static Adafruit_MCP23X17 mcp;

// Snapshot of both MCP ports, refreshed once per loop.
struct McpSnapshot
{
    uint16_t gpioAB{0xFFFF}; ///< GPIOA in bits 0..7, GPIOB in bits 8..15.

    // True if the mapped MCP pin reads LOW (pressed with pull-ups).
    bool isLow(uint8_t pin) const { return (gpioAB & (1u << pin)) == 0u; }
} snap;

// Reader that uses the cached snapshot (pressed == LOW).
static bool readFromSnapshot(uint8_t key)
{
    const uint8_t idx = UB::util::indexFromKey(key);
    if (idx < NUM_BUTTONS)
    {
        return snap.isLow(MCP_PINS[idx]);
    }
    return (digitalRead(key) == LOW);
}

// Build handler with external reader; skip MCU pin init.
static Button btns = makeButtonsWithReader(readFromSnapshot, kTiming, /*skipPinInit=*/true);

// Configure MCP button pins once.
static void configureMcpPins()
{
    for (size_t i = 0; i < NUM_BUTTONS; ++i)
    {
        const uint8_t p = MCP_PINS[i];
        mcp.pinMode(p, INPUT_PULLUP);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(50);

    // Initialize I2C.
    Wire.begin();

    // Initialize the MCP23017.
    if (!mcp.begin_I2C(MCP_ADDR))
    {
        Serial.println("MCP23017 not found!");
        while (true)
        {
            delay(1000);
        }
    }

    configureMcpPins();

    // TestButton3 timing override.
    ButtonPerConfig tBtn3{};
    tBtn3.debounce_ms = 60;     ///< Custom debounce.
    tBtn3.short_press_ms = 300; ///< Custom short threshold.
    tBtn3.long_press_ms = 1800; ///< Custom long threshold.
    // tBtn3.active_low = true; ///< Default remains LOW=pressed.
    // tBtn3.enabled = true; ///< Default remains enabled.

    btns.setPerConfig(ButtonIndex::TestButton3, tBtn3);
}

void loop()
{
    // One transaction keeps all button reads in this update on the same snapshot.
    // This bool reader cannot detect bus failure; use a result reader when
    // the acquisition layer can report whether its cached data is trustworthy.
    snap.gpioAB = mcp.readGPIOAB();

    btns.update();

    const ButtonPressType btn1 = btns.getPressType(ButtonIndex::TestButton1);
    const ButtonPressType btn2 = btns.getPressType(ButtonIndex::TestButton2);
    const ButtonPressType btn3 = btns.getPressType(ButtonIndex::TestButton3);

    if (btn1 == ButtonPressType::Short)
        Serial.println("TestButton1: Short press...");
    else if (btn1 == ButtonPressType::Long)
        Serial.println("TestButton1: Long press...");

    if (btn2 == ButtonPressType::Short)
        Serial.println("TestButton2: Short press...");
    else if (btn2 == ButtonPressType::Long)
        Serial.println("TestButton2: Long press...");

    if (btn3 == ButtonPressType::Short)
        Serial.println("TestButton3: Short press...");
    else if (btn3 == ButtonPressType::Long)
        Serial.println("TestButton3: Long press...");

    delay(10);
}
