/**
 * MIT License
 *
 * @brief Abstract base class for button event handling.
 *
 * @file IButtonHandler.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-05
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include <ButtonTypes.h>
#include <ButtonCompatibility.h>

/**
 * @brief Abstract interface for button event handlers.
 */
class IButtonHandler
{
public:
    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    virtual ~IButtonHandler() noexcept = default;

    /**
     * @brief Scan and process button states (debounce and press timing).
     */
    virtual void update() noexcept = 0;

    /**
     * @brief Scan and process button states using a provided timestamp (ms).
     */
    virtual void update(uint32_t now_ms) noexcept = 0;

    /**
     * @brief Get debounced state of button.
     * @param buttonId Index of button.
     * @return True if button is pressed.
     */
    [[nodiscard]] virtual bool isPressed(uint8_t buttonId) const noexcept = 0;

    /**
     * @brief Get and consume press event for a button.
     * @param buttonId Index of button.
     * @return ButtonPressType Event type: Short, Long, Double, or None.
     */
    virtual ButtonPressType getPressType(uint8_t buttonId) noexcept = 0;

    /**
     * @brief Exact duration (ms) of the most recent completed press.
     */
    [[nodiscard]] virtual uint32_t getLastPressDuration(uint8_t /*buttonId*/) const noexcept { return 0U; }

    /**
     * @brief Reset internal debouncer state and clear pending events.
     */
    virtual void reset() noexcept { /* no-op by default */ }

    /**
     * @brief Number of logical buttons managed by this handler.
     * @return Count of buttons (0–255).
     */
    [[nodiscard]] virtual uint8_t size() const noexcept = 0;

    /**
     * @brief Build a 32-bit pressed mask (bit i == 1 iff button i is pressed).
     */
    [[nodiscard]] virtual uint32_t pressedMask() const noexcept
    {
        uint32_t m = 0;
        for (uint8_t i = 0; i < size() && i < 32; ++i)
            if (isPressed(i))
                m |= (1u << i);
        return m;
    }

    /**
     * @brief Write the current debounced state into a compat bitset (bit i == pressed).
     * @tparam N Size of the destination bitset.
     * @param out Destination bitset; bits beyond size() remain unchanged/false.
     */
    template <size_t N>
    void snapshot(UB::compat::bitset<N> &out) const noexcept
    {
        out.reset();
        const uint8_t n = static_cast<uint8_t>(N < size() ? N : size());
        for (uint8_t i = 0; i < n; ++i)
            out.set(i, isPressed(i));
    }

#if UB_HAS_STD_BITSET
    /**
     * @brief Write the current debounced state into a std::bitset (bit i == pressed).
     * @tparam N Size of the destination bitset.
     * @param out Destination bitset; bits beyond size() remain unchanged/false.
     */
    template <size_t N>
    void snapshot(std::bitset<N> &out) const noexcept
    {
        out.reset();
        const uint8_t n = static_cast<uint8_t>(N < size() ? N : size());
        for (uint8_t i = 0; i < n; ++i)
            out.set(i, isPressed(i));
    }
#endif

    // ---- Enum-friendly overloads (no cast needed in call sites) ---- //

    /**
     * @brief Query whether a button is currently pressed using an enum identifier.
     * @tparam E Enum type representing button IDs.
     * @param buttonId Button identifier (enum value).
     * @return True if the button is currently pressed; false otherwise.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    [[nodiscard]] bool isPressed(E buttonId) const noexcept
    {
        return isPressed(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Get the most recent press type of a button using an enum identifier.
     * @tparam E Enum type representing button IDs.
     * @param buttonId Button identifier (enum value).
     * @return Last detected press type (short, long, double, etc.).
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    ButtonPressType getPressType(E buttonId) noexcept
    {
        return getPressType(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Get the duration of the most recent button press using an enum identifier.
     * @tparam E Enum type representing button IDs.
     * @param buttonId Button identifier (enum value).
     * @return Duration of the last press in milliseconds.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    [[nodiscard]] uint32_t getLastPressDuration(E buttonId) const noexcept
    {
        return getLastPressDuration(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Query the current latched state of a button using an enum identifier.
     * @tparam E Enum type representing button IDs.
     * @param buttonId Button identifier (enum value).
     * @return True if the button is latched ON; false otherwise.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    [[nodiscard]] bool isLatched(E buttonId) const noexcept
    {
        return isLatched(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Query and clear the latched-change flag using an enum identifier.
     * @tparam E Enum type representing button IDs.
     * @param buttonId Button identifier (enum value).
     * @return True if the latched state changed since last query; false otherwise.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    bool getAndClearLatchedChanged(E buttonId) noexcept
    {
        return getAndClearLatchedChanged(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Query the current latched state of a button.
     * @param buttonId Index of button.
     * @return True if latched ON; false otherwise.
     */
    [[nodiscard]] virtual bool isLatched(uint8_t /*buttonId*/) const noexcept { return false; }

    /**
     * @brief Force the latched state for a button.
     * @param b  Button index.
     * @param on Desired latched state (true = ON, false = OFF).
     */
    virtual void setLatched(uint8_t /*b*/, bool /*on*/) noexcept {}

    /**
     * @brief Clear all latched states.
     */
    virtual void clearAllLatched() noexcept {}

    /**
     * @brief Clear a subset of latched states using a bitmask.
     * @param mask Bitmask of button indices to clear (bit0 = button 0, etc.).
     */
    virtual void clearLatchedMask(uint32_t /*mask*/) noexcept {}

    /**
     * @brief Build a 32-bit latched mask.
     * @return Bitmask where bit i is set when button i is latched ON (up to 32 buttons).
     */
    [[nodiscard]] virtual uint32_t latchedMask() const noexcept
    {
        uint32_t m = 0;
        for (uint8_t i = 0; i < size() && i < 32; ++i)
            if (isLatched(i))
                m |= (1u << i);
        return m;
    }

    /**
     * @brief Edge flag for latching: true if latched state changed since the last clear.
     * @param buttonId Index of button.
     * @return True if the latched state changed since the previous call.
     */
    virtual bool getAndClearLatchedChanged(uint8_t /*buttonId*/) noexcept { return false; }
};