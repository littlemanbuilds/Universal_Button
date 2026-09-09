#define BUTTON_LIST(X) X(Start, 4) X(Stop, 5)
#include <Universal_Button.h>

const uint8_t* otherPins()
{
#ifdef UB_NO_LEGACY_CONFIG_GLOBALS
    return UB::config::BUTTON_PINS;
#else
    return BUTTON_PINS;
#endif
}

bool otherAliasesMatch()
{
#ifdef UB_NO_LEGACY_CONFIG_GLOBALS
    return UB::config::NUM_BUTTONS == 2;
#else
    return &BUTTON_PINS == &UB::config::BUTTON_PINS && NUM_BUTTONS == 2;
#endif
}
