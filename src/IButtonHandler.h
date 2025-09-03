/**
 * MIT License
 *
 * @brief Abstract base class for button event handling.
 *
 * @file IButtonHandler.h
 * @author Little Man Builds
 * @date 2025-08-05
 * @copyright Copyright (c) 2025 Little Man Builds
 */

#pragma once

#include <ButtonTypes.h>
#include <stdint.h>

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
};