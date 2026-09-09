#define BUTTON_LIST(X) X(Start, 4) X(Stop, 5)
#include <Universal_Button.h>
#include <type_traits>

#ifdef UB_NO_LEGACY_CONFIG_GLOBALS
// These names must remain available to the application in opt-out mode.
int BUTTON_PINS = 0;
int NUM_BUTTONS = 0;
struct ButtonPins {};
struct ButtonIndex {};
#else
static_assert(std::is_same<decltype(BUTTON_PINS), const uint8_t (&)[2]>::value,
              "legacy mapping remains an array reference");
static_assert(BUTTON_PINS[0] == 4 && BUTTON_PINS[1] == 5, "constexpr legacy values");
static_assert(&BUTTON_PINS == &UB::config::BUTTON_PINS, "alias refers to local array");
static_assert(NUM_BUTTONS == 2 && ButtonPins::Stop == 5, "legacy constants");
static_assert(static_cast<uint8_t>(ButtonIndex::Stop) == 1, "legacy index");
#endif

const uint8_t* otherPins();
bool otherAliasesMatch();

int main()
{
    const uint8_t* local = UB::config::BUTTON_PINS;
#ifndef UB_NO_LEGACY_CONFIG_GLOBALS
    if (BUTTON_PINS != local) return 1;
#endif
    const uint8_t* other = otherPins();
    // Each translation unit owns its configuration array; aliases do not copy it.
    return otherAliasesMatch() && local != other &&
           local[0] == 4 && local[1] == 5 && other[0] == 4 && other[1] == 5 ? 0 : 1;
}
