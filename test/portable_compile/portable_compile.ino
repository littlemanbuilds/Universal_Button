/**
 * @brief Portable compile smoke test using the explicit-pins core API.
 */
#include <Arduino.h>
#include <ButtonHandler.h>

constexpr uint8_t kPins[] = {4};
static ButtonHandler<1> buttons(kPins);

void setup()
{
    buttons.update();
}

void loop()
{
    buttons.update();
    (void)buttons.isPressed(0);
}
