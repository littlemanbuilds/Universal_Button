/**
 * @file 04_Port_Expander.ino
 *
 * @brief Use MCP23017 port expander as the button source (short/long events).
 */

// Explicit button mapping (compile-time). MUST be BEFORE any header includes.
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
constexpr uint8_t MCP_ADDR = 0x20; ///< I2C address (A2..A0 = 000 -> 0x20). Adjust if you strap address pins.

// Map each logical button (by enum order) to MCP pin 0..15 (0..7=A, 8..15=B).
constexpr uint8_t MCP_PINS[NUM_BUTTONS] = {
    0, ///< ButtonTest1 -> GPA0.
    1, ///< ButtonTest2 -> GPA1.
    8  ///< ButtonTest3 -> GPB0.
};
static_assert(NUM_BUTTONS == (sizeof(MCP_PINS) / sizeof(MCP_PINS[0])),
              "MCP_PINS size must match NUM_BUTTONS");

// Custom timing (debounce, short, long) in milliseconds.
constexpr ButtonTimingConfig kTiming{50, 300, 1500};

static Adafruit_MCP23X17 mcp;

/**
 * Reader: return true if pressed (LOW).
 * If the key matches our configured buttons, read from MCP; else fall back to MCU GPIO.
 */
static bool readFromMcp(uint8_t key)
{
    const uint8_t idx = UB::util::indexFromKey(key);
    if (idx < NUM_BUTTONS)
    {
        return mcp.digitalRead(MCP_PINS[idx]) == LOW;
    }
    return digitalRead(key) == LOW; ///< Fallback if reusing this sketch with real GPIOs.
}

// Build handler with external reader; skip MCU pin init.
static Button btns = makeButtonsWithReader(readFromMcp, kTiming, /*skipPinInit=*/true);

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
}

void loop()
{
    btns.update();

    // Read/consume events for each button.
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