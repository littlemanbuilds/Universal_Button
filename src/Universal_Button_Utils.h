/**
 * MIT License
 *
 * @brief Small, device-agnostic helpers for Universal_Button sketches and adapters:
 *        map configured BUTTON_PINS “keys” to logical indices, plus a generic
 *        variant for explicit pin arrays.
 *
 * @file Universal_Button_Utils.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-30
 * @copyright © 2025 Little Man Builds
 */

#pragma once

#include <ButtonHandler_Config.h>
#include <cstdint>
#include <cstddef>

namespace UB
{
    namespace util
    {
        /**
         * @brief Map a BUTTON_PINS "key" (the configured pin value) to its logical index.
         * @return 0..NUM_BUTTONS-1 on success, 0xFF if not found.
         */
        inline uint8_t indexFromKey(uint8_t key)
        {
            for (uint8_t i = 0; i < NUM_BUTTONS; ++i)
            {
                if (BUTTON_PINS[i] == key)
                    return i;
            }
            return 0xFF;
        }

        /**
         * @brief Generic variant: map a key within an explicit pins array.
         * @tparam N Array length deduced from @p pins.
         * @return 0..N-1 on success, 0xFF if not found.
         */
        template <size_t N>
        inline uint8_t indexFromKeyIn(const uint8_t (&pins)[N], uint8_t key)
        {
            for (uint8_t i = 0; i < N; ++i)
            {
                if (pins[i] == key)
                    return i;
            }
            return 0xFF;
        }

    }
} ///< namespace UB::util