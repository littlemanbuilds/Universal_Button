/**
 * @file 02_Press_Type.ino
 * @brief Press-type events (short/long) with custom debounce/thresholds.
 */

// Explicit button mapping (compile-time). MUST be BEFORE any header includes.
#define BUTTON_LIST(X) \
    X(TestButton, 7) ///< INPUT_PULLUP (pressed == LOW).

#include <Arduino.h>
#include <Universal_Button.h>

// Custom timings: debounce, short, long (ms).
constexpr ButtonTimingConfig kTiming{
    50,  ///< Debounce_ms: filter bounce / noisy edges.
    300, ///< Short_press_ms: >= 300ms counts as short.
    1500 ///< Long_press_ms:  >= 1500ms counts as long.
};

// Create handler sized to NUM_BUTTONS from mapping above, with custom timing.
static Button btns = makeButtons(kTiming);

void setup()
{
    Serial.begin(115200);
    delay(50);
}

void loop()
{
    btns.update();

    // Get-and-consume the event for TestButton.
    const ButtonPressType evt = btns.getPressType(ButtonIndex::TestButton);

    switch (evt)
    {
    case ButtonPressType::Short:
        Serial.println("Short press detected!");
        break;
    case ButtonPressType::Long:
        Serial.println("Long press detected!");
        break;
    default:
        // No event this iteration.
        break;
    }

    delay(10);
}