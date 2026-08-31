/**
 * MIT License
 *
 * @brief Classify short, long, and double-click button interactions.
 *
 * @file 02_PressType.ino
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

// Define the logical button name before including <Universal_Button.h>.
#define BUTTON_LIST(X) \
    X(TestButton, 6) ///< TestButton == GPIO6. INPUT_PULLUP (pressed == LOW).

#include <Universal_Button.h>

#include <Arduino.h>

namespace ubcfg = UB::config; ///< Shorter name for the button list above.

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

// Create handler sized to ubcfg::NUM_BUTTONS from mapping above, with custom timing.
static Button btns = makeButtons(kTiming);

void setup()
{
    Serial.begin(115200);
    delay(50);

    // (Optional) Per-button override example:
    // ButtonPerConfig pc{};
    // pc.double_click_ms = 250; ///< Faster double-click just for this button.
    // btns.setPerConfig(ubcfg::ButtonIndex::TestButton, pc);
}

void loop()
{
    btns.update();

    // Get-and-consume the event for TestButton.
    const ButtonPressType evt = btns.getPressType(ubcfg::ButtonIndex::TestButton);

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
        Serial.println("Long press completed (released)!");
        break;

    default:
        // No event this iteration.
        break;
    }

    delay(10);
}
