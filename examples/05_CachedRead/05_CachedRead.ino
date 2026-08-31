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

#include <Universal_Button.h>
#include <Universal_Button_Utils.h>

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

namespace ubcfg = UB::config; ///< Shorter name for the button list above.

// MCP23017 wiring/config.
constexpr uint8_t MCP_ADDR = 0x20;

// Map each logical button (by enum order) to MCP pin 0..15 (0..7=A, 8..15=B).
constexpr uint8_t MCP_PINS[ubcfg::NUM_BUTTONS] = {
    0, ///< TestButton1 -> GPA0.
    1, ///< TestButton2 -> GPA1.
    8  ///< TestButton3 -> GPB0.
};
static_assert(ubcfg::NUM_BUTTONS == (sizeof(MCP_PINS) / sizeof(MCP_PINS[0])),
              "MCP_PINS size must match ubcfg::NUM_BUTTONS");

// Custom timings (debounce, short, long) in milliseconds.
constexpr ButtonTimingConfig kTiming{50, 300, 1500};

static Adafruit_MCP23X17 mcp;

// Snapshot of both MCP ports, refreshed once per loop.
struct McpSnapshot
{
    uint8_t A{0xFF};
    uint8_t B{0xFF};

    inline uint8_t getBit(uint8_t pin) const
    {
        return (pin < 8) ? ((A >> pin) & 0x01) : ((B >> (pin - 8)) & 0x01);
    }

    // Convenience: true if the pin reads LOW (pressed with pull-ups).
    inline bool isLow(uint8_t pin) const { return getBit(pin) == 0; }
} snap;

// Reader that uses the cached snapshot (pressed == LOW).
static bool readFromSnapshot(uint8_t key)
{
    const uint8_t idx = UB::util::indexFromKey(key);
    if (idx < ubcfg::NUM_BUTTONS)
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
    for (uint8_t i = 0; i < ubcfg::NUM_BUTTONS; ++i)
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

    btns.setPerConfig(static_cast<uint8_t>(ubcfg::ButtonIndex::TestButton3), tBtn3);
}

void loop()
{
    // v2 API: read both ports in one 16-bit read.
    uint16_t ab = mcp.readGPIOAB(); ///< [low byte]=A, [high byte]=B.
    snap.A = static_cast<uint8_t>(ab & 0x00FF);
    snap.B = static_cast<uint8_t>((ab >> 8) & 0x00FF);

    btns.update();

    const ButtonPressType btn1 = btns.getPressType(ubcfg::ButtonIndex::TestButton1);
    const ButtonPressType btn2 = btns.getPressType(ubcfg::ButtonIndex::TestButton2);
    const ButtonPressType btn3 = btns.getPressType(ubcfg::ButtonIndex::TestButton3);

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
