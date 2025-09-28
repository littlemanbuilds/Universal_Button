/**
 * MIT License
 *
 * @brief Generic multi-button handler with debounce and press duration detection.
 *
 * @file ButtonHandler.h
 * @author Little Man Builds
 * @date 2025-08-05
 */

#pragma once

#include <Arduino.h>
#include <IButtonHandler.h>
#include <ButtonTypes.h>
#include <functional>
#include <stdint.h>
#include <type_traits>
#include <cstddef>
#include <bitset>

/**
 * @brief Generic multi-button handler (adaptable to any digital input source).
 *
 * Uses a template parameter N for compile-time button count.
 */
template <size_t N> ///< Sets the number of buttons at compile time.
class ButtonHandler : public IButtonHandler
{
    static_assert(N > 0, "Button<N>: N must be greater than 0.");

public:
    // ---- Types ---- //

    /**
     * @brief Callable that returns true if the button identified by uint8_t is pressed.
     */
    using ReadFunc = std::function<bool(uint8_t)>;

    /**
     * @brief Optional function-pointer fast path (smaller than std::function).
     */
    using ReadFnPtr = bool (*)(uint8_t);

    // ---- Constructors ---- //

    /**
     * @brief Construct a Button handler.
     * @param buttonPins Array of button pin numbers (length N).
     * @param readFunc Function to read button state: takes key, returns true if pressed.
     *                 Defaults to native digitalRead with LOW=pressed (internal pullup).
     * @param timing Debounce and press duration configuration.
     */
    ButtonHandler(const uint8_t buttonPins[N],
                  ReadFunc readFunc = nullptr,
                  ButtonTimingConfig timing = {})
        : read_func_{readFunc}, timing_{timing}, read_ptr_{nullptr}
    {
        const uint32_t t0 = millis();
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            pinMode(pins_[i], INPUT_PULLUP);
            last_state_[i] = false;      // committed (debounced)
            last_state_read_[i] = false; // last raw read
            last_state_change_[i] = t0;  // time raw state last changed
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            per_[i] = ButtonPerConfig{};
            last_duration_[i] = 0;
        }
        if (!read_func_)
        {
            read_func_ = [](uint8_t pin)
            { return digitalRead(pin) == LOW; };
        }
    }

    /**
     * @brief Construct a Button handler with an optional external reader and
     *        the ability to skip MCU GPIO initialization.
     *
     * Use this overload when button states come from an external device
     * (e.g., an MCP23017 port expander) or when GPIOs are configured elsewhere.
     *
     * @param buttonPins Array of N key identifiers (typically MCU GPIO numbers).
     * @param readFunc   Callable that returns true if the button is pressed.
     *                   Signature: bool(uint8_t key). If nullptr, native fallback is used.
     * @param timing     Debounce and press-duration configuration applied to all buttons.
     * @param skipPinInit When true, pinMode(key, INPUT_PULLUP) is NOT called for each key.
     *                    Set true for external readers or externally configured GPIOs.
     */
    ButtonHandler(const uint8_t buttonPins[N],
                  ReadFunc readFunc,
                  ButtonTimingConfig timing,
                  bool skipPinInit)
        : read_func_{readFunc}, timing_{timing}, read_ptr_{nullptr}
    {
        const uint32_t t0 = millis();
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            if (!skipPinInit)
            {
                pinMode(pins_[i], INPUT_PULLUP);
            }
            last_state_[i] = false;
            last_state_read_[i] = false;
            last_state_change_[i] = t0;
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            per_[i] = ButtonPerConfig{};
            last_duration_[i] = 0;
        }
        if (!read_func_)
        {
            read_func_ = [](uint8_t pin)
            { return digitalRead(pin) == LOW; };
        }
    }

    // ---- IButtonHandler overrides (core per-button API) ---- //

    /**
     * @brief Scan and process button states (debounce and press timing).
     */
    void update() noexcept override
    {
        const uint32_t now = millis();

        for (size_t i = 0; i < N; ++i)
        {
            // Skip disabled buttons entirely.
            if (!per_[i].enabled)
            {
                continue;
            }

            // Resolve timing with per-button overrides (0 => fall back to global).
            const uint32_t dms = per_[i].debounce_ms ? per_[i].debounce_ms : timing_.debounce_ms;
            const uint32_t sms = per_[i].short_press_ms ? per_[i].short_press_ms : timing_.short_press_ms;
            const uint32_t lms = per_[i].long_press_ms ? per_[i].long_press_ms : timing_.long_press_ms;

            // Read raw physical level using pointer fast-path if set, else std::function, else native.
            const bool raw_physical = read_ptr_
                                          ? read_ptr_(pins_[i])
                                          : (read_func_ ? read_func_(pins_[i])
                                                        : (digitalRead(pins_[i]) == LOW));
            // Apply active level (default: LOW=pressed).
            const bool raw = per_[i].active_low ? raw_physical : !raw_physical;

            // Debounce: restart window on any raw edge.
            if (raw != last_state_read_[i])
            {
                last_state_read_[i] = raw;
                last_state_change_[i] = now;
            }

            // If raw has been stable long enough and differs from committed, commit it.
            if ((now - last_state_change_[i]) >= dms && last_state_[i] != last_state_read_[i])
            {
                last_state_[i] = last_state_read_[i];

                if (last_state_[i])
                {
                    // Committed press.
                    press_start_[i] = now;
                }
                else
                {
                    // Committed release -> classify.
                    const uint32_t duration = press_start_[i] ? (now - press_start_[i]) : 0U;
                    last_duration_[i] = duration; ///< Record exact duration for retrieval.
                    if (duration >= lms)
                    {
                        event_[i] = ButtonPressType::Long;
                    }
                    else if (duration >= sms)
                    {
                        event_[i] = ButtonPressType::Short;
                    }
                    else
                    {
                        event_[i] = ButtonPressType::None;
                    }
                    press_start_[i] = 0;
                }
            }
        }
    }

    /**
     * @brief Get debounced state of button.
     * @param buttonId Index of button.
     * @return True if button is pressed.
     */
    [[nodiscard]] bool isPressed(uint8_t buttonId) const noexcept override
    {
        if (buttonId >= N)
        {
            return false;
        }
        return last_state_[buttonId];
    }

    /**
     * @brief Get and consume the press event type for a button.
     * @param buttonId Index of button.
     * @return ButtonPressType Event type: Short, Long, or None.
     */
    ButtonPressType getPressType(uint8_t buttonId) noexcept override
    {
        if (buttonId >= N)
        {
            return ButtonPressType::None;
        }
        const ButtonPressType e = event_[buttonId];
        event_[buttonId] = ButtonPressType::None; // consume
        return e;
    }

    /**
     * @brief Exact duration (ms) of the most recent completed press (i.e., on last release).
     * @param buttonId Index of button.
     * @return Milliseconds of the last press; 0 if none recorded or out-of-range.
     * @note Non-consuming: value persists until next completed press or reset().
     */
    [[nodiscard]] uint32_t getLastPressDuration(uint8_t buttonId) const noexcept
    {
        return (buttonId < N) ? last_duration_[buttonId] : 0U;
    }

    /**
     * @brief Clear all pending events and re-initialize debounced state.
     *
     * Sets every button to "not pressed", clears the pending event queue,
     * and re-bases timestamps using millis(). Per-button and global
     * configuration (timings, polarity, enabled) are left unchanged.
     */
    void reset() noexcept override
    {
        const uint32_t t0 = millis();
        for (size_t i = 0; i < N; ++i)
        {
            last_state_[i] = false;      ///< Committed (debounced).
            last_state_read_[i] = false; ///< Last raw (post-polarity) state.
            last_state_change_[i] = t0;  ///< Restart debounce window.
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            last_duration_[i] = 0;
        }
    }

    // ---- Enum-friendly overloads (no cast needed in sketches) ---- //

    /**
     * @brief Enum-friendly overload of isPressed().
     * @tparam E Enum type (must satisfy std::is_enum<E>::value).
     * @param buttonId Enumerated button identifier.
     * @return True if the debounced state is pressed; false otherwise.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    [[nodiscard]] bool isPressed(E buttonId) const noexcept
    {
        return isPressed(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Enum-friendly overload of getPressType().
     * @tparam E Enum type (must satisfy std::is_enum<E>::value).
     * @param buttonId Enumerated button identifier.
     * @return ButtonPressType Event type: Short, Long, or None.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    ButtonPressType getPressType(E buttonId) noexcept
    {
        return getPressType(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Enum-friendly wrapper for querying the exact duration of the last
     *        completed press (in milliseconds).
     * @tparam E An enumeration type (SFINAE-constrained) representing a button ID,
     *           e.g. your generated `ButtonIndex` enum.
     * @param buttonId Enumerated button identifier (e.g., `ButtonIndex::Start`).
     * @return Milliseconds of the most recent *completed* press for @p buttonId.
     *         Returns `0` if no press has been recorded yet or if @p buttonId is
     *         out of range.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    [[nodiscard]] uint32_t getLastPressDuration(E buttonId) const noexcept
    {
        return getLastPressDuration(static_cast<uint8_t>(buttonId));
    }

    // ---- Runtime setters / helpers ---- //

    /// @brief Swap the std::function reader at runtime.
    void setReadFunc(ReadFunc f) noexcept { read_func_ = f; }

    /// @brief Set a function-pointer reader (fast path on MCU).
    void setReadPtr(ReadFnPtr f) noexcept { read_ptr_ = f; }

    /// @brief Update global timing configuration.
    void setTiming(ButtonTimingConfig t) noexcept { timing_ = t; }

    /// @brief Override per-button configuration.
    void setPerConfig(uint8_t id, const ButtonPerConfig &cfg) noexcept
    {
        if (id < N)
        {
            per_[id] = cfg;
        }
    }

    /// @brief Enable or disable an individual button at runtime.
    void enable(uint8_t id, bool en) noexcept
    {
        if (id < N)
        {
            per_[id].enabled = en;
        }
    }

    /// @brief Peek the pending event without consuming it.
    [[nodiscard]] ButtonPressType peekPressType(uint8_t buttonId) const noexcept
    {
        return (buttonId < N) ? event_[buttonId] : ButtonPressType::None;
    }

    /// @brief Clear the pending event explicitly.
    void clearPressType(uint8_t buttonId) noexcept
    {
        if (buttonId < N)
        {
            event_[buttonId] = ButtonPressType::None;
        }
    }

    /// @brief Milliseconds the button has been held (0 if not currently pressed).
    [[nodiscard]] uint32_t heldMillis(uint8_t buttonId) const noexcept
    {
        if (buttonId >= N)
            return 0;
        const uint32_t now = millis();
        return (last_state_[buttonId] && press_start_[buttonId]) ? (now - press_start_[buttonId]) : 0U;
    }

    // ----Convenience API ---- //

    /// @brief Compile-time button count as a utility for templates/static contexts.
    static constexpr uint8_t sizeStatic() noexcept { return static_cast<uint8_t>(N); }

    /// @brief Number of logical buttons managed (runtime virtual override).
    [[nodiscard]] uint8_t size() const noexcept override { return static_cast<uint8_t>(N); }

    /// @brief Build a 32-bit pressed mask (bit i == 1 iff button i is pressed).
    [[nodiscard]] uint32_t pressedMask() const noexcept override
    {
        static_assert(N <= 32, "pressedMask() requires N <= 32; use bitset snapshot for larger N.");
        uint32_t m = 0;
        for (size_t i = 0; i < N; ++i)
            if (isPressed(static_cast<uint8_t>(i)))
                m |= (1u << i);
        return m;
    }

    /// @brief Write the current debounced state into a bitset (bit i == pressed).
    inline void snapshot(std::bitset<N> &out) const noexcept
    {
        for (size_t i = 0; i < N; ++i)
            out.set(i, isPressed(static_cast<uint8_t>(i)));
    }

    /// @brief Return a bitset containing the current debounced state.
    [[nodiscard]] inline std::bitset<N> snapshot() const noexcept
    {
        std::bitset<N> b;
        snapshot(b);
        return b;
    }

    /// @brief Apply a functor to each button ID and its debounced state.
    template <typename F>
    inline void forEach(F &&f) const noexcept
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(N); ++i)
            f(i, isPressed(i));
    }

private:
    uint8_t pins_[N];               ///< Pin numbers / keys for each button.
    bool last_state_[N];            ///< Last committed (debounced) state.
    bool last_state_read_[N]{};     ///< Most recent raw state.
    uint32_t last_state_change_[N]; ///< Timestamp (ms) when raw state last changed.
    uint32_t press_start_[N];       ///< Timestamp (ms) when press started (committed).
    ButtonPressType event_[N];      ///< Pending event (short/long) per button.
    ReadFunc read_func_;            ///< std::function reader (flexible).
    ReadFnPtr read_ptr_{nullptr};   ///< Function pointer reader (fast path).
    ButtonPerConfig per_[N];        ///< Per-button overrides (timing, polarity, enable).
    ButtonTimingConfig timing_;     ///< Global debounce and press-duration configuration.
    uint32_t last_duration_[N];     ///< Last measured press duration (ms), set on release.
};