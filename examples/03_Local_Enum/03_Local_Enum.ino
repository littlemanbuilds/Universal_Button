/**
 * @file 03_Local_Enum.ino
 * @brief Minimal example using a local enum and an explicit pins array (no list/factory helpers).
 */

#include <Arduino.h>
#include <ButtonHandler.h>

enum class LocalButton : uint8_t
{
    TestButton1,
    TestButton2,
    _COUNT
};
constexpr size_t kNumButtons = static_cast<size_t>(LocalButton::_COUNT);
constexpr uint8_t BTN_PINS[kNumButtons] = {6, 7}; ///< TestButton1 == GPIO6, TestButton2 = GPIO7.

using Buttons = ButtonHandler<kNumButtons>;
static Buttons btns(BTN_PINS); ///< Constructed without any factory or BUTTON_LIST.

void setup()
{
    Serial.begin(115200);
    delay(50);
}

void loop()
{
    btns.update();

    // Current debounced states.
    const bool b1 = btns.isPressed(LocalButton::TestButton1);
    const bool b2 = btns.isPressed(LocalButton::TestButton2);

    // Track edges, only printing on change (no serial spam).
    static bool prev1 = false;
    static bool prev2 = false;

    if (b1 != prev1)
    {
        prev1 = b1;
        if (b1)
        {
            Serial.println("TestButton1 pressed!");
        }
        else
        {
            const uint32_t ms = btns.getLastPressDuration(LocalButton::TestButton1);
            Serial.printf("TestButton1 released after %u ms\n!", ms);
        }
    }

    if (b2 != prev2)
    {
        prev2 = b2;
        if (b2)
        {
            Serial.println("TestButton2 pressed!");
        }
        else
        {
            const uint32_t ms = btns.getLastPressDuration(LocalButton::TestButton2);
            Serial.printf("TestButton2 released after %u ms\n!", ms);
        }
    }

    delay(10);
}