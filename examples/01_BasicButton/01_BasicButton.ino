/**
 * MIT License
 *
 * @brief Read one ESP32-S3 button with the simplest debounced level API.
 *
 * @file 01_BasicButton.ino
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

// Create handler sized to ubcfg::NUM_BUTTONS from mapping above.
static Button btns = makeButtons();

void setup()
{
    Serial.begin(115200);
    delay(50);
}

void loop()
{
    btns.update();

    if (btns.isPressed(ubcfg::ButtonIndex::TestButton))
    {
        Serial.println("TestButton is pressed!");
    }
    else
    {
        Serial.println("No input detected.");
    }

    delay(100);
}
