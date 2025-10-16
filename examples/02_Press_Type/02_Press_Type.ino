/**
 * @file 02_Press_Type.ino
 * @brief Press-type events (short/long/double) with custom debounce/thresholds.
 */

// Explicit button mapping (compile-time). MUST be BEFORE <Universal_Button> header include.
#define BUTTON_LIST(X) \
    X(TestButton, 7) ///< INPUT_PULLUP (pressed == LOW).

#include <Arduino.h>
#include <Universal_Button.h>

/**
 * Custom timings (ms): debounce, short, long, double-click-gap.
 * - double_click_ms: max gap BETWEEN two short presses to count as a Double event.
 */
constexpr ButtonTimingConfig kTiming{
    30,   ///< Debounce_ms: filter bounce / noisy edges.
    100,  ///< Short_press_ms: >= 100ms counts as short (quick tap).
    1500, ///< Long_press_ms:  >= 1500ms counts as long.
    400   ///< Double_click_ms: second short press within 400ms -> Double.
};

// Create handler sized to NUM_BUTTONS from mapping above, with custom timing.
static Button btns = makeButtons(kTiming);

void setup()
{
    Serial.begin(115200);
    delay(50);

    // (Optional) Per-button override example:
    // ButtonPerConfig pc{};
    // pc.double_click_ms = 250; ///< Faster double-click just for this button.
    // btns.setPerConfig(ButtonIndex::TestButton, pc);
}

void loop()
{
    btns.update();

    // Get-and-consume the event for TestButton.
    const ButtonPressType evt = btns.getPressType(ButtonIndex::TestButton);

    switch (evt)
    {
    case ButtonPressType::Short:
        // Note: Short presses are deferred briefly to allow for a possible double click.
        // If no second press arrives within double_click_ms, this fires.
        Serial.println("Short press detected!");
        break;

    case ButtonPressType::Double:
        // Two short presses with gap <= double_click_ms.
        Serial.println("Double-click detected!");
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