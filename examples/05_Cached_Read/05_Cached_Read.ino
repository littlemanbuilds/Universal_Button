/**
 * @file 05_Cached_Read.ino
 *
 * @brief MCP23017 cached read: take one snapshot per loop (GPIOA+GPIOB),
 *        then feed all button reads from that snapshot. Coherent and fast.
 */

// Explicit button mapping (compile-time). MUST be BEFORE <Universal_Button> header include.
// When using a port expander, ButtonTest numbers are irrelevant e.g. 60, 61, 62... as long as they're different.
#define BUTTON_LIST(X) \
    X(ButtonTest1, 7)  \
    X(ButtonTest2, 8)  \
    X(ButtonTest3, 9)

#include <Arduino.h>
#include <Universal_Button.h>
#include <Universal_Button_Utils.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

// MCP23017 wiring/config.
constexpr uint8_t MCP_ADDR = 0x20;

// Map each logical button (by enum order) to MCP pin 0..15 (0..7=A, 8..15=B).
constexpr uint8_t MCP_PINS[NUM_BUTTONS] = {
    0, ///< ButtonTest1 -> GPA0.
    1, ///< ButtonTest2 -> GPA1.
    8  ///< ButtonTest3 -> GPB0.
};
static_assert(NUM_BUTTONS == (sizeof(MCP_PINS) / sizeof(MCP_PINS[0])),
              "MCP_PINS size must match NUM_BUTTONS");

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
    if (idx < NUM_BUTTONS)
    {
        return snap.isLow(MCP_PINS[idx]); // renamed call
    }
    return (digitalRead(key) == LOW);
}

// Build handler with external reader; skip MCU pin init.
static Button btns = makeButtonsWithReader(readFromSnapshot, kTiming, /*skipPinInit=*/true);

// Configure MCP button pins once.
static void configureMcpPins()
{
    for (uint8_t i = 0; i < NUM_BUTTONS; ++i)
    {
        const uint8_t p = MCP_PINS[i];
        mcp.pinMode(p, INPUT_PULLUP);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(50);

    // I2C.
    Wire.begin(/* SDA = */ 8, /* SCL = */ 9); ///< For ESP32-S3 DevKitC-1.
    mcp.begin_I2C(MCP_ADDR);                  ///< Adafruit MCP23017 v2.x uses begin_I2C; v1.x uses begin(addr).

    configureMcpPins();

    // ButtonTest3 timing override.
    ButtonPerConfig bTest3{};
    bTest3.debounce_ms = 60;     ///< Custom debounce.
    bTest3.short_press_ms = 300; ///< Custom short threshold.
    bTest3.long_press_ms = 1800; ///< Custom long threshold.
    // bTest3.active_low = true; ///< Default remains LOW=pressed.
    // bTest3.enabled = true; ///< Default remains enabled.

    btns.setPerConfig(static_cast<uint8_t>(ButtonIndex::ButtonTest3), bTest3);
}

void loop()
{
    // v2 API: read both ports in one 16-bit read.
    uint16_t ab = mcp.readGPIOAB(); ///< [low byte]=A, [high byte]=B.
    snap.A = static_cast<uint8_t>(ab & 0x00FF);
    snap.B = static_cast<uint8_t>((ab >> 8) & 0x00FF);

    btns.update();

    const ButtonPressType btn1 = btns.getPressType(ButtonIndex::ButtonTest1);
    const ButtonPressType btn2 = btns.getPressType(ButtonIndex::ButtonTest2);
    const ButtonPressType btn3 = btns.getPressType(ButtonIndex::ButtonTest3);

    if (btn1 == ButtonPressType::Short)
        Serial.println("ButtonTest1: Short press...");
    else if (btn1 == ButtonPressType::Long)
        Serial.println("ButtonTest1: Long press...");

    if (btn2 == ButtonPressType::Short)
        Serial.println("ButtonTest2: Short press...");
    else if (btn2 == ButtonPressType::Long)
        Serial.println("ButtonTest2: Long press...");

    if (btn3 == ButtonPressType::Short)
        Serial.println("ButtonTest3: Short press...");
    else if (btn3 == ButtonPressType::Long)
        Serial.println("ButtonTest3: Long press...");

    delay(10);
}