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
#include <stdint.h>
#include <type_traits>
#include <cstddef>
#include <bitset>

/**
 * @brief Generic multi-button handler (adaptable to any digital input source).
 *
 * Uses a template parameter for compile-time button count, performs
 * debouncing and classifies press events (short/long). Optimized for ESP32,
 * supporting both a fast per-pin reader and a context-aware reader callback.
 *
 * @tparam N Number of logical buttons handled by this instance.
 */
template <size_t N> ///< Sets the number of buttons at compile time.
class ButtonHandler : public IButtonHandler
{
    static_assert(N > 0, "Button<N>: N must be greater than 0.");

public:
    // ---- Types ---- //

    /**
     * @brief Function pointer that reads a single button by pin/key.
     *
     * @param id Logical key/pin for the button.
     * @return true if the button is currently pressed (active).
     */
    using ReadPinFn = bool (*)(uint8_t);

    /**
     * @brief Context-aware reader callback.
     *
     * @param ctx Opaque user context pointer supplied at construction/setter.
     * @param id Logical key/pin for the button.
     * @return true if the button is currently pressed (active).
     */
    using ReadFn = bool (*)(void *ctx, uint8_t);

    // ---- Constructors ---- //

    /**
     * @brief Construct a Button handler using native GPIO reads.
     *
     * Initializes pins to INPUT_PULLUP by default and uses
     * digitalRead(pin) == LOW as "pressed" (active-low).
     *
     * @param buttonPins  Reference to an array of length N with pin IDs.
     * @param timing      Global debounce/press-duration configuration.
     * @param skipPinInit If true, GPIO mode is not configured here.
     *                    Set to true when pins are configured elsewhere.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = false) noexcept
        : timing_{timing}
    {
        const uint32_t t0 = millis();
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            if (!skipPinInit)
                pinMode(pins_[i], INPUT_PULLUP);
            last_state_[i] = false;
            last_state_read_[i] = false;
            last_state_change_[i] = t0;
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            per_[i] = ButtonPerConfig{};
            last_duration_[i] = 0;
        }
        // Default fast-path reader uses native GPIO (active-low).
        read_pin_fn_ = [](uint8_t pin) -> bool
        { return digitalRead(pin) == LOW; };
    }

    /**
     * @brief Construct with a per-pin fast reader function.
     *
     * Use this overload to provide a light-weight function pointer that
     * returns the pressed state for a given key/pin.
     *
     * @param buttonPins Reference to an array of length N with pin IDs.
     * @param readPin Fast reader: bool(uint8_t id) returns pressed.
     * @param timing Global debounce/press-duration configuration.
     * @param skipPinInit If false, pins are set to INPUT_PULLUP here.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadPinFn readPin,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true) noexcept
        : timing_{timing}, read_pin_fn_{readPin}
    {
        const uint32_t t0 = millis();
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            if (!skipPinInit)
                pinMode(pins_[i], INPUT_PULLUP);
            last_state_[i] = false;
            last_state_read_[i] = false;
            last_state_change_[i] = t0;
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            per_[i] = ButtonPerConfig{};
            last_duration_[i] = 0;
        }
        if (!read_pin_fn_)
        {
            // Fallback to native GPIO (active-low).
            read_pin_fn_ = [](uint8_t pin) -> bool
            { return digitalRead(pin) == LOW; };
        }
    }

    /**
     * @brief Construct with a context-aware reader callback.
     *
     * Use this when button states come from an external device (e.g., port
     * expander) or another subsystem that needs ctx.
     *
     * @param buttonPins Reference to an array of length N with key IDs.
     * @param readCb Reader callback: bool(void* ctx, uint8_t id).
     * @param ctx Opaque pointer provided back to readCb on each read.
     * @param timing Global debounce/press-duration configuration.
     * @param skipPinInit If false, pins are set to INPUT_PULLUP here.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadFn readCb, void *ctx,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true) noexcept
        : timing_{timing}, read_fn_{readCb}, read_ctx_{ctx}
    {
        const uint32_t t0 = millis();
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            if (!skipPinInit)
                pinMode(pins_[i], INPUT_PULLUP);
            last_state_[i] = false;
            last_state_read_[i] = false;
            last_state_change_[i] = t0;
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            per_[i] = ButtonPerConfig{};
            last_duration_[i] = 0;
        }
    }

    // ---- IButtonHandler overrides (core per-button API) ---- //

    /**
     * @brief Scan and process button states using current millis().
     *
     * Debounces edges, updates committed states, and classifies completed
     * presses into Short or Long events based on configured thresholds.
     */
    void update() noexcept override { update(millis()); }

    /**
     * @brief Scan and process button states using a provided timestamp.
     *
     * @param now Millisecond timestamp to use (e.g., from xTaskGetTickCount in RTOS).
     */
    void update(uint32_t now) noexcept override
    {
        for (size_t i = 0; i < N; ++i)
        {
            // Skip disabled buttons entirely.
            if (!per_[i].enabled)
                continue;

            // Resolve timing with per-button overrides (0 => fall back to global).
            const uint32_t dms = per_[i].debounce_ms ? static_cast<uint32_t>(per_[i].debounce_ms) : timing_.debounce_ms;
            const uint32_t sms = per_[i].short_press_ms ? static_cast<uint32_t>(per_[i].short_press_ms) : timing_.short_press_ms;
            const uint32_t lms = per_[i].long_press_ms ? static_cast<uint32_t>(per_[i].long_press_ms) : timing_.long_press_ms;

            // Read raw physical level: prefer fast per-pin fn, else ctx-callback, else native.
            const bool pressed_default =
                (read_pin_fn_) ? read_pin_fn_(pins_[i])
                               : (read_fn_ ? read_fn_(read_ctx_, pins_[i])
                                           : (digitalRead(pins_[i]) == LOW));

            // Apply active level (default: active-low => pressed when LOW).
            const bool raw = per_[i].active_low ? pressed_default : !pressed_default;

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
                        event_[i] = ButtonPressType::Long;
                    else if (duration >= sms)
                        event_[i] = ButtonPressType::Short;
                    else
                        event_[i] = ButtonPressType::None;

                    press_start_[i] = 0;
                }
            }
        }
    }

    /**
     * @brief Get debounced state of a button.
     * @param buttonId Index of button.
     * @return true if the button is currently pressed (committed/debounced).
     */
    [[nodiscard]] bool isPressed(uint8_t buttonId) const noexcept override
    {
        return (buttonId < N) ? last_state_[buttonId] : false;
    }

    /**
     * @brief Get and consume the press event type for a button.
     * @param buttonId Index of button.
     * @return ButtonPressType::Short, ::Long, or ::None if no new event.
     */
    ButtonPressType getPressType(uint8_t buttonId) noexcept override
    {
        if (buttonId >= N)
            return ButtonPressType::None;
        const ButtonPressType e = event_[buttonId];
        event_[buttonId] = ButtonPressType::None; // consume
        return e;
    }

    /**
     * @brief Exact duration (ms) of the most recent completed press (on release).
     * @param buttonId Index of button.
     * @return Milliseconds of the last press; 0 if none recorded or out-of-range.
     * @note Non-consuming: value persists until the next completed press or reset().
     */
    [[nodiscard]] uint32_t getLastPressDuration(uint8_t buttonId) const noexcept override
    {
        return (buttonId < N) ? last_duration_[buttonId] : 0U;
    }

    /**
     * @brief Clear all pending events and re-initialize debounced state.
     *
     * Sets every button to "not pressed", clears the pending event flags,
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
     * @return true if the debounced state is pressed; false otherwise.
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
     * @return ButtonPressType event: Short, Long, or None.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    ButtonPressType getPressType(E buttonId) noexcept
    {
        return getPressType(static_cast<uint8_t>(buttonId));
    }

    /**
     * @brief Enum-friendly wrapper for querying the exact duration (ms) of the
     *        last completed press.
     * @tparam E Enum type (must satisfy std::is_enum<E>::value).
     * @param buttonId Enumerated button identifier.
     * @return Milliseconds of the most recent *completed* press for buttonId.
     *         Returns 0 if no press has been recorded yet or if buttonId is out of range.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    [[nodiscard]] uint32_t getLastPressDuration(E buttonId) const noexcept
    {
        return getLastPressDuration(static_cast<uint8_t>(buttonId));
    }

    // ---- Runtime setters / helpers ---- //

    /**
     * @brief Update global timing configuration.
     * @param t New debounce/press-duration config for all buttons (defaults are ok).
     */
    void setTiming(ButtonTimingConfig t) noexcept { timing_ = t; }

    /**
     * @brief Override per-button configuration (timing, polarity, enable).
     * @param id  Button index (0..N-1).
     * @param cfg Per-button overrides. Zero values in timings = use global.
     */
    void setPerConfig(uint8_t id, const ButtonPerConfig &cfg) noexcept
    {
        if (id < N)
            per_[id] = cfg;
    }

    /**
     * @brief Enable or disable an individual button at runtime.
     * @param id Button index (0..N-1).
     * @param en true to enable; false to disable.
     */
    void enable(uint8_t id, bool en) noexcept
    {
        if (id < N)
            per_[id].enabled = en;
    }

    /**
     * @brief Set a context-aware reader callback and context pointer.
     * @param cb  Reader callback: bool(void* ctx, uint8_t id).
     * @param ctx Opaque pointer passed back to cb on each read.
     * @note This disables any previously set ReadPinFn fast-path.
     */
    void setReadFn(ReadFn cb, void *ctx) noexcept
    {
        read_fn_ = cb;
        read_ctx_ = ctx;
        read_pin_fn_ = nullptr;
    }

    /**
     * @brief Set a per-pin fast-path reader.
     * @param cb Reader: bool(uint8_t id) returns pressed.
     * @note This disables any previously set context-aware reader.
     */
    void setReadPinFn(ReadPinFn cb) noexcept
    {
        read_pin_fn_ = cb;
        read_fn_ = nullptr;
        read_ctx_ = nullptr;
    }

    /**
     * @brief Compile-time button count as a utility for templates/static contexts.
     */
    static constexpr uint8_t sizeStatic() noexcept { return static_cast<uint8_t>(N); }

    /**
     * @brief Number of logical buttons managed (runtime virtual override).
     */
    [[nodiscard]] uint8_t size() const noexcept override { return static_cast<uint8_t>(N); }

    /**
     * @brief Build a 32-bit pressed mask (bit i == 1 iff button i is pressed).
     *
     * @return Bitmask of currently pressed buttons. Requires N <= 32.
     */
    [[nodiscard]] uint32_t pressedMask() const noexcept override
    {
        static_assert(N <= 32, "pressedMask() requires N <= 32; use snapshot(bitset) for larger N.");
        uint32_t m = 0;
        for (size_t i = 0; i < N; ++i)
            if (last_state_[i])
                m |= (1u << i);
        return m;
    }

    /**
     * @brief Write the current debounced state into a bitset (bit i == pressed).
     *
     * @tparam M Size of the destination bitset; if M < N, extra buttons are ignored.
     * @param out Destination bitset receiving the current debounced states.
     */
    template <size_t M>
    inline void snapshot(std::bitset<M> &out) const noexcept
    {
        const size_t n = (M < N) ? M : N;
        for (size_t i = 0; i < n; ++i)
            out.set(i, last_state_[i]);
    }

    /**
     * @brief Apply a functor to each button ID and its debounced state.
     *
     * @tparam F Callable type: void(uint8_t id, bool pressed).
     * @param f  Functor/lambda invoked for each button.
     */
    template <typename F>
    inline void forEach(F &&f) const noexcept
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(N); ++i)
            f(i, last_state_[i]);
    }

private:
    uint8_t pins_[N];               ///< Pin numbers / keys for each button.
    bool last_state_[N];            ///< Last committed (debounced) state.
    bool last_state_read_[N]{};     ///< Most recent raw state (after polarity).
    uint32_t last_state_change_[N]; ///< Timestamp (ms) when raw state last changed.
    uint32_t press_start_[N];       ///< Timestamp (ms) when press started (committed).
    ButtonPressType event_[N];      ///< Pending event (short/long) per button.
    ButtonPerConfig per_[N];        ///< Per-button overrides (timing, polarity, enable).
    ButtonTimingConfig timing_;     ///< Global debounce and press-duration configuration.
    uint32_t last_duration_[N];     ///< Last measured press duration (ms), set on release.

    ReadPinFn read_pin_fn_{nullptr}; ///< Optional fast-path reader (per-pin).
    ReadFn read_fn_{nullptr};        ///< Optional context-aware reader.
    void *read_ctx_{nullptr};        ///< Opaque context for @c read_fn_.
};