/**
 * MIT License
 *
 * @brief Abstract base class for button event handling.
 *
 * @file IButtonHandler.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-05
 * @copyright © 2025 Little Man Builds
 */

#pragma once

#include <ButtonTypes.h>
#include <cstdint>
#include <bitset>

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
     * @return ButtonPressType Event type: Short, Long, or None.
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
        const uint8_t n = size() > 32 ? 32 : size();
        for (uint8_t i = 0; i < n; ++i)
            if (isPressed(i))
                m |= (1u << i);
        return m;
    }

    /**
     * @brief Write the current debounced state into a bitset (bit i == pressed).
     * @tparam N Size of the destination bitset.
     * @param out Destination bitset; bits beyond size() remain unchanged/false.
     */
    template <size_t N>
    void snapshot(std::bitset<N> &out) const noexcept
    {
        const uint8_t n = static_cast<uint8_t>(N < size() ? N : size());
        for (uint8_t i = 0; i < n; ++i)
            out.set(i, isPressed(i));
        // Any remaining bits (if N > size()) stay default-initialized (false).
    }
};