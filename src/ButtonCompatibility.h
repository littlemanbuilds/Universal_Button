/**
 * MIT License
 *
 * @brief Small compatibility layer for toolchains without full C++ STL headers.
 *
 * @file Universal_Button_Compat.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-01-29
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__has_include)
#if __has_include(<type_traits>)
#include <type_traits>
#define UB_HAS_TYPE_TRAITS 1
#else
#define UB_HAS_TYPE_TRAITS 0
#endif

#if __has_include(<bitset>)
#include <bitset>
#define UB_HAS_STD_BITSET 1
#else
#define UB_HAS_STD_BITSET 0
#endif
#else
#define UB_HAS_TYPE_TRAITS 0
#define UB_HAS_STD_BITSET 0
#endif

namespace UB::compat
{
    // ---- Minimal type traits (fallbacks if <type_traits> isn't available) ---- //

#if UB_HAS_TYPE_TRAITS

    /**
     * @brief Alias to std::enable_if when available.
     * @tparam B Compile-time boolean condition.
     * @tparam T Resulting type if condition is true.
     */
    template <bool B, typename T = int>
    using enable_if_t = typename std::enable_if<B, T>::type;

    /**
     * @brief Alias to std::is_enum when available.
     * @tparam T Type to query.
     */
    template <typename T>
    using is_enum = std::is_enum<T>;

#else

    /**
     * @brief Minimal enable_if implementation for toolchains without <type_traits>.
     * @tparam B Compile-time boolean condition.
     * @tparam T Resulting type if condition is true.
     */
    template <bool B, typename T = int>
    struct enable_if
    {
    };

    /**
     * @brief enable_if specialization for true conditions.
     * @tparam T Resulting type when condition is satisfied.
     */
    template <typename T>
    struct enable_if<true, T>
    {
        using type = T;
    };

    template <bool B, typename T = int>
    using enable_if_t = typename enable_if<B, T>::type;

#if defined(__clang__) || defined(__GNUC__)

    /**
     * @brief Minimal enum detection using a compiler intrinsic.
     * @tparam T Type to query.
     */
    template <typename T>
    struct is_enum
    {
        static constexpr bool value = __is_enum(T);
    };

#else

    /**
     * @brief Fallback enum detection for unsupported toolchains.
     * @note Returns false on unsupported compilers.
     * @tparam T Type to query.
     */
    template <typename T>
    struct is_enum
    {
        static constexpr bool value = false;
    };
#endif

#endif // UB_HAS_TYPE_TRAITS

    /**
     * @brief Lightweight fixed-size bitset replacement.
     *
     * @details Provides a minimal subset of std::bitset functionality
     *          (reset, set, test) for environments where <bitset> is unavailable.
     *
     * Storage is implemented as a packed byte array.
     *
     * @tparam N Number of bits.
     */
    template <size_t N>
    class bitset
    {
    public:
        /**
         * @brief Construct an empty bitset (all bits cleared).
         */
        constexpr bitset() noexcept : data_{0}
        {
        }

        /**
         * @brief Clear all bits.
         */
        void reset() noexcept
        {
            for (size_t i = 0; i < kBytes; ++i)
                data_[i] = 0;
        }

        /**
         * @brief Set or clear an individual bit.
         * @param i Bit index.
         * @param v New bit value.
         */
        void set(size_t i, bool v) noexcept
        {
            if (i >= N)
                return;

            const size_t byte = (i >> 3);
            const uint8_t mask = static_cast<uint8_t>(1u << (i & 7u));

            if (v)
                data_[byte] = static_cast<uint8_t>(data_[byte] | mask);
            else
                data_[byte] = static_cast<uint8_t>(data_[byte] & static_cast<uint8_t>(~mask));
        }

        /**
         * @brief Test whether a bit is set.
         * @param i Bit index.
         * @return True if bit is set; false otherwise.
         */
        [[nodiscard]] bool test(size_t i) const noexcept
        {
            if (i >= N)
                return false;

            const size_t byte = (i >> 3);
            const uint8_t mask = static_cast<uint8_t>(1u << (i & 7u));
            return (data_[byte] & mask) != 0u;
        }

        /**
         * @brief Get the number of bits.
         * @return Number of bits in this bitset.
         */
        static constexpr size_t size() noexcept { return N; }

    private:
        static constexpr size_t kBytes = (N + 7u) / 8u; ///< Number of bytes required to store N bits (ceil(N / 8)).
        uint8_t data_[kBytes];                          ///< Packed bit storage.
    };
} ///< namespace UB::compat
