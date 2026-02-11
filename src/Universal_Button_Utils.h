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
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include <ButtonCompatibility.h>

#ifndef UB_UTIL_NO_CONFIG_MAP
#include <ButtonHandler_Config.h>
#endif

namespace UB::util
{
#ifndef UB_UTIL_NO_CONFIG_MAP
    /**
     * @brief Map a BUTTON_PINS "key" (the configured pin value) to its logical index.
     * @note Define UB_UTIL_NO_CONFIG_MAP before including this header to omit this helper
     *       when only generic array-based mapping is needed.
     * @return 0..NUM_BUTTONS-1 on success, 0xFF if not found.
     */
    inline uint8_t indexFromKey(uint8_t key)
    {
        static_assert(NUM_BUTTONS <= 255, "indexFromKey() supports up to 255 mapped keys.");

        for (size_t i = 0; i < NUM_BUTTONS; ++i)
        {
            if (BUTTON_PINS[i] == key)
                return static_cast<uint8_t>(i);
        }
        return 0xFF;
    }
#endif

    /**
     * @brief Generic variant: map a key within an explicit pins array.
     * @tparam N Array length deduced from @p pins.
     * @return 0..N-1 on success, 0xFF if not found.
     */
    template <size_t N>
    inline uint8_t indexFromKeyIn(const uint8_t (&pins)[N], uint8_t key)
    {
        static_assert(N <= 255, "indexFromKeyIn() supports arrays up to 255 entries.");

        for (size_t i = 0; i < N; ++i)
        {
            if (pins[i] == key)
                return static_cast<uint8_t>(i);
        }
        return 0xFF;
    }
} ///< namespace UB::util
