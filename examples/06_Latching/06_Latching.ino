/**
 * @file 06_Latching.ino
 *
 * @brief Simple latching demo (toggle / set) + global reset.
 */

// Explicit button mapping (compile-time). MUST be BEFORE <Universal_Button> include.
#define BUTTON_LIST(X) \
    X(BtnMomentary, 6) \
    X(BtnSet, 7)       \
    X(BtnReset, 8)     \
    X(BtnToggle, 9) ///< INPUT_PULLUP (pressed == LOW).

#include <Arduino.h>
#include <Universal_Button.h>

/**
 * Timing (ms): debounce, short, long, double-click-gap.
 */
constexpr ButtonTimingConfig kTiming{
    20,  ///< Debounce_ms
    100, ///< Short_press_ms
    900, ///< Long_press_ms
    400  ///< Double_click_ms
};

// Create handler sized to NUM_BUTTONS from mapping above, with custom timing.
static Button btns = makeButtons(kTiming);

static const __FlashStringHelper *toStr(ButtonPressType e)
{
    switch (e)
    {
    case ButtonPressType::Short:
        return F("Short");
    case ButtonPressType::Long:
        return F("Long");
    case ButtonPressType::Double:
        return F("Double");
    default:
        return F("None");
    }
}

// Event-only print helper.
static void printEventOnly(const __FlashStringHelper *name, ButtonPressType e)
{
    Serial.print(name);
    Serial.print(F(" event: "));
    Serial.println(toStr(e));
}

// Full print helper.
static void printLatchLine(const __FlashStringHelper *name, ButtonPressType e, bool latchState)
{
    Serial.print(name);
    Serial.print(F(" event: "));
    Serial.print(toStr(e));
    Serial.print(F(", latched => "));
    Serial.print(latchState ? F("ON") : F("OFF"));
    Serial.print(F(", LatchedMask: 0b"));
    Serial.println(btns.latchedMask(), BIN);
}

void setup()
{
    Serial.begin(115200);
    delay(50);

    // BtnMomentary: events only.

    // BtnSet: Short => set latch ON.
    {
        ButtonPerConfig pc{};
        pc.latch_enabled = true;
        pc.latch_mode = LatchMode::Set;
        pc.latch_on = LatchTrigger::Short;
        pc.latch_initial = false;
        btns.setPerConfig(ButtonIndex::BtnSet, pc);
    }

    // BtnReset: command button. Long => clear all latches.

    // BtnToggle: Double-click => toggle latch.
    {
        ButtonPerConfig pc{};
        pc.latch_enabled = true;
        pc.latch_mode = LatchMode::Toggle;
        pc.latch_on = LatchTrigger::Double;
        pc.latch_initial = false;
        btns.setPerConfig(ButtonIndex::BtnToggle, pc);
    }

    btns.reset();
}

void loop()
{
    btns.update();

    // Read events (consume-on-read).
    const ButtonPressType eMom = btns.getPressType(ButtonIndex::BtnMomentary);
    const ButtonPressType eSet = btns.getPressType(ButtonIndex::BtnSet);
    const ButtonPressType eReset = btns.getPressType(ButtonIndex::BtnReset);
    const ButtonPressType eToggle = btns.getPressType(ButtonIndex::BtnToggle);

    // Momentary.
    if (eMom != ButtonPressType::None)
    {
        printEventOnly(F("BtnMomentary"), eMom);
    }

    // Set behavior.
    if (eSet == ButtonPressType::Short)
    {
        printLatchLine(F("BtnSet"), eSet, btns.isLatched(ButtonIndex::BtnSet)); ///< Short triggers latch ON; show full line.
    }
    else if (eSet != ButtonPressType::None)
    {
        printEventOnly(F("BtnSet"), eSet); ///< Any other set event.
    }

    // Reset behavior.
    if (eReset == ButtonPressType::Long)
    {
        btns.clearAllLatched();                       ///< Long press clears all latches.
        printLatchLine(F("BtnReset"), eReset, false); ///< After clear, latch state is OFF.
    }
    else if (eReset != ButtonPressType::None)
    {
        printEventOnly(F("BtnReset"), eReset); ///< Any other reset event.
    }

    // Toggle behavior.
    if (eToggle == ButtonPressType::Double)
    {
        printLatchLine(F("BtnToggle"), eToggle, btns.isLatched(ButtonIndex::BtnToggle)); ///< Double toggles latch.
    }
    else if (eToggle != ButtonPressType::None)
    {
        printEventOnly(F("BtnToggle"), eToggle); // Any other toggle event.
    }

    delay(10);
}