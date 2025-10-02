/**
 * MIT License
 *
 * @brief Common button types (events and timing/config structs).
 *
 * @file ButtonTypes.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-30
 * @copyright © 2025 Little Man Builds
 */

#pragma once

#include <stdint.h>

/**
 * @brief Enum for type of button press event.
 */
enum class ButtonPressType : uint8_t
{
    None,  ///< No event.
    Short, ///< Short press event.
    Long   ///< Long press event.
};

/**
 * @brief Configuration for debounce and press-duration timings.
 */
struct ButtonTimingConfig
{
    uint32_t debounce_ms;    ///< Minimum time to confirm a press/release.
    uint32_t short_press_ms; ///< Minimum time for a short press.
    uint32_t long_press_ms;  ///< Minimum time for a long press.

    constexpr ButtonTimingConfig(
        uint32_t debounce = 30,
        uint32_t short_press = 200,
        uint32_t long_press = 1000)
        : debounce_ms(debounce), short_press_ms(short_press), long_press_ms(long_press) {}
};

/**
 * @brief Optional per-button overrides (library feature, not required by the interface).
 * Zero values fall back to global timings; active_low=true means LOW=pressed.
 */
struct ButtonPerConfig
{
    uint16_t debounce_ms{0};    ///< 0 => use global timing_.debounce_ms
    uint16_t short_press_ms{0}; ///< 0 => use global timing_.short_press_ms
    uint16_t long_press_ms{0};  ///< 0 => use global timing_.long_press_ms
    bool active_low{true};      ///< true = LOW means pressed (default pull-up wiring)
    bool enabled{true};         ///< false = ignore this button in update()
};