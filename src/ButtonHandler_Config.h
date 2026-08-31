/**
 * MIT License
 *
 * @brief Compile-time button mapping used by the Universal_Button convenience factories.
 *
 * @file ButtonHandler_Config.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include "ButtonCompatibility.h"

#ifndef BUTTON_LIST
#error "Universal_Button v2: Define BUTTON_LIST(X) before including <Universal_Button.h>, or include <ButtonHandler.h> and provide an explicit pins array."
#endif

namespace UB
{
    namespace config
    {
        /**
         * @brief Named pin/key constants generated from BUTTON_LIST(X).
         */
        struct ButtonPins
        {
#define X(name, pin) static constexpr uint8_t name = pin;
            BUTTON_LIST(X)
#undef X
        };

        /** @brief Pin/key array generated from BUTTON_LIST(X). */
        constexpr uint8_t BUTTON_PINS[] = {
#define X(name, pin) pin,
            BUTTON_LIST(X)
#undef X
        };

        /** @brief Number of configured buttons. */
        constexpr size_t NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

        static_assert(NUM_BUTTONS > 0u, "Universal_Button: BUTTON_LIST(X) must contain at least one button.");
        static_assert(NUM_BUTTONS <= 255u, "Universal_Button: BUTTON_LIST(X) supports at most 255 buttons.");

        /**
         * @brief Logical button indices generated in BUTTON_LIST(X) order.
         */
        enum class ButtonIndex : uint8_t
        {
#define X(name, pin) name,
            BUTTON_LIST(X)
#undef X
                _COUNT
        };
    } ///< namespace config
} ///< namespace UB

/**
 * @brief v1.x global configuration aliases.
 *
 * @details v2 uses UB::config as the normal namespace. Existing projects can
 *          keep the familiar global names during migration. Define
 *          UB_NO_LEGACY_CONFIG_GLOBALS before including Universal_Button.h to
 *          enforce the namespaced API and avoid global configuration symbols.
 */
#ifndef UB_NO_LEGACY_CONFIG_GLOBALS
using ButtonPins = UB::config::ButtonPins;
using ButtonIndex = UB::config::ButtonIndex;
constexpr const uint8_t (&BUTTON_PINS)[UB::config::NUM_BUTTONS] = UB::config::BUTTON_PINS;
constexpr size_t NUM_BUTTONS = UB::config::NUM_BUTTONS;
#endif
