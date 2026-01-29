/**
 * MIT License
 *
 * @brief Common button types (events and timing/config structs).
 *
 * @file ButtonTypes.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-30
 * @copyright Copyright © 2026 Little Man Builds
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
    Long,  ///< Long press event.
    Double ///< Two short presses within a configured gap.
};

/**
 * @brief Latching behavior applied when a configured trigger event occurs.
 */
enum class LatchMode : uint8_t
{
    Toggle, ///< Flip latched state (OFF ↔ ON).
    Set,    ///< Force latched state ON.
    Reset   ///< Force latched state OFF.
};

/**
 * @brief Which press event should drive latching.
 */
enum class LatchTrigger : uint8_t
{
    Short, ///< Trigger latching on ButtonPressType::Short.
    Long,  ///< Trigger latching on ButtonPressType::Long.
    Double ///< Trigger latching on ButtonPressType::Double.
};

/**
 * @brief Configuration for debounce and press-duration timings.
 */
struct ButtonTimingConfig
{
    uint32_t debounce_ms;     ///< Minimum time to confirm a press/release.
    uint32_t short_press_ms;  ///< Minimum time for a short press.
    uint32_t long_press_ms;   ///< Minimum time for a long press.
    uint32_t double_click_ms; ///< Max gap between two short presses to count as a double.

    constexpr ButtonTimingConfig(uint32_t debounce = 30,
                                 uint32_t short_press = 200,
                                 uint32_t long_press = 1000,
                                 uint32_t double_click = 400)
        : debounce_ms(debounce), short_press_ms(short_press), long_press_ms(long_press), double_click_ms(double_click) {}
};

/**
 * @brief Optional per-button overrides (library feature, not required by the interface).
 * Zero values fall back to global timings; active_low=true means LOW=pressed.
 */
struct ButtonPerConfig
{
    uint16_t debounce_ms{0};     ///< 0 => use global timing_.debounce_ms.
    uint16_t short_press_ms{0};  ///< 0 => use global timing_.short_press_ms.
    uint16_t long_press_ms{0};   ///< 0 => use global timing_.long_press_ms.
    uint16_t double_click_ms{0}; ///< 0 => use global timing_.double_click_ms.
    bool active_low{true};       ///< true = LOW means pressed (default pull-up wiring).
    bool enabled{true};          ///< false = ignore this button in update().

    // Latching.
    bool latch_enabled{false};                  ///< true = maintain a latched state for this button.
    LatchMode latch_mode{LatchMode::Toggle};    ///< Toggle / Set / Reset behavior when triggered.
    LatchTrigger latch_on{LatchTrigger::Short}; ///< Which press event drives latching for this button.
    bool latch_initial{false};                  ///< Initial latched state applied on construction and reset().
};