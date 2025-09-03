/**
 * MIT License
 *
 * @brief Easy entry-point for the Universal_Button library.
 *
 * @file Universal_Button.h
 * @author Little Man Builds
 * @date 2025-08-30
 */

#pragma once

#include <Arduino.h>
#include <functional>
#include <ButtonHandler_Config.h>
#include <ButtonHandler.h>

// Version macro (keep aligned with library.properties / library.json).
#define UNIVERSAL_BUTTON_VERSION "1.0.0"

/**
 * @brief Zero-macro factory: pass pins explicitly and optional timing.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N],
                                            ButtonTimingConfig timing = {})
{
    return ButtonHandler<N>(pins, nullptr, timing);
}

/**
 * @brief Config-driven alias & factory:
 *        Uses the mapping provided by ButtonHandler_Config.h
 *        (either user-defined via BUTTON_LIST, or the default TestButton=7).
 */
using Button = ButtonHandler<NUM_BUTTONS>;

inline Button makeButtons(ButtonTimingConfig timing = {})
{
    return Button(BUTTON_PINS, nullptr, timing);
}

/**
 * @brief Create a Button (ButtonHandler<NUM_BUTTONS>) that reads states from an
 *        external source (e.g., MCP23017), while using the config-defined mapping
 *        from ButtonHandler_Config.h.
 */
inline Button makeButtonsWithReader(std::function<bool(uint8_t)> read,
                                    ButtonTimingConfig timing = {},
                                    bool skipPinInit = true)
{
    return Button(BUTTON_PINS, read, timing, skipPinInit);
}

/**
 * @brief Create a ButtonHandler with an explicit pins array and an external
 *        reader (e.g., port expander), independent of the config mapping.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                     std::function<bool(uint8_t)> read,
                                                     ButtonTimingConfig timing = {},
                                                     bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit);
}

/**
 * @brief Create a Button (ButtonHandler<NUM_BUTTONS>) that reads states from an
 *        external source (e.g., MCP23017) using a function-pointer reader,
 *        while using the config-defined mapping from ButtonHandler_Config.h
 */
inline Button makeButtonsWithReader(bool (*read)(uint8_t),
                                    ButtonTimingConfig timing = {},
                                    bool skipPinInit = true)
{
    return Button(BUTTON_PINS, read, timing, skipPinInit);
}

/**
 * @brief Create a ButtonHandler with an explicit pins array and a function-pointer
 *        external reader (e.g., a port expander), independent of the config mapping.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                     bool (*read)(uint8_t),
                                                     ButtonTimingConfig timing = {},
                                                     bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit);
}