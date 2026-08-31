/**
 * MIT License
 *
 * @brief Device-agnostic mapping helpers for Universal_Button adapters and sketches.
 *
 * @file Universal_Button_Utils.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include "ButtonCompatibility.h"

#ifndef UB_UTIL_NO_CONFIG_MAP
#include "ButtonHandler_Config.h"
#endif

namespace UB
{
    namespace util
    {
#ifndef UB_UTIL_NO_CONFIG_MAP
        /**
         * @brief Map a configured BUTTON_LIST key/pin value to its logical index.
         * @param key Configured key/pin value.
         * @return 0..NUM_BUTTONS-1 on success, or 0xFF when the key is not mapped.
         */
        inline uint8_t indexFromKey(uint8_t key) noexcept
        {
            static_assert(config::NUM_BUTTONS <= 255u, "indexFromKey() supports at most 255 keys.");
            for (size_t i = 0; i < config::NUM_BUTTONS; ++i)
                if (config::BUTTON_PINS[i] == key)
                    return static_cast<uint8_t>(i);
            return 0xFFu;
        }
#endif

        /**
         * @brief Map a key/pin value within an explicit array to its logical index.
         * @tparam N Array length.
         * @param pins Explicit key/pin array.
         * @param key Key/pin value to find.
         * @return 0..N-1 on success, or 0xFF when the key is not present.
         */
        template <size_t N>
        inline uint8_t indexFromKeyIn(const uint8_t (&pins)[N], uint8_t key) noexcept
        {
            static_assert(N <= 255u, "indexFromKeyIn() supports arrays up to 255 entries.");
            for (size_t i = 0; i < N; ++i)
                if (pins[i] == key)
                    return static_cast<uint8_t>(i);
            return 0xFFu;
        }
    } ///< namespace util
} ///< namespace UB
