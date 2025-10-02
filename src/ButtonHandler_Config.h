/**
 * MIT License
 *
 * @brief Config for ButtonHandler button mappings.
 *
 * @file ButtonHandler_Config.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-30
 * @copyright © 2025 Little Man Builds
 */

#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

/**
 * @brief Button pin assignments/index.
 *
 * Define BUTTON_LIST(X) before including this header to set mapping, e.g.:
 *
 *   #define BUTTON_LIST(X) \
 *       X(Start, 4)        \
 *       X(Stop,  5)
 *
 * If not defined, default provided (TestButton on GPIO 25).
 */
#ifndef BUTTON_LIST
#define BUTTON_LIST(X) \
    X(TestButton, 25) ///< INPUT_PULLUP; pressed == LOW.
#endif

struct ButtonPins
{
#define X(name, pin) static constexpr uint8_t name = pin;
    BUTTON_LIST(X)
#undef X
};

constexpr uint8_t BUTTON_PINS[] = {
#define X(name, pin) pin,
    BUTTON_LIST(X)
#undef X
};

constexpr size_t NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

enum class ButtonIndex : uint8_t
{
#define X(name, pin) name,
    BUTTON_LIST(X)
#undef X
        _COUNT
};