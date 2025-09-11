/**
 * @file 01_Basic_Button.ino
 *
 * @brief Basic demo using easy header + isPressed() polling.
 */

// Explicit button mapping (compile-time). MUST be BEFORE <Universal_Button> header include.
#define BUTTON_LIST(X) \
    X(TestButton, 7) ///< INPUT_PULLUP (pressed == LOW).

#include <Arduino.h>
#include <Universal_Button.h>

// Create handler sized to NUM_BUTTONS from mapping above.
static Button btns = makeButtons();

void setup()
{
    Serial.begin(115200);
    delay(50);
}

void loop()
{
    btns.update();

    if (btns.isPressed(ButtonIndex::TestButton))
    {
        Serial.println("ButtonTest is pressed!");
    }
    else
    {
        Serial.println("No input detected.");
    }

    delay(100);
}