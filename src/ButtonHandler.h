/**
 * MIT License
 *
 * @brief Generic multi-button handler with debounce and press duration detection.
 *
 * @file ButtonHandler.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2025-08-05
 * @copyright © 2025 Little Man Builds
 */

#pragma once

#include <Arduino.h>
#include <ButtonTypes.h>
#include <IButtonHandler.h>
#include <type_traits>
#include <bitset>
#include <cstdint>
#include <cstddef>

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
     * @param id Logical key/pin for the button.
     * @return true if the button is currently pressed (active).
     */
    using ReadPinFn = bool (*)(uint8_t);

    /**
     * @brief Context-aware reader callback.
     * @param ctx Opaque user context pointer supplied at construction/setter.
     * @param id Logical key/pin for the button.
     * @return true if the button is currently pressed (active).
     */
    using ReadFn = bool (*)(void *ctx, uint8_t id);

    /**
     * @brief Time function to use for update(); nullptr → uses ::millis().
     * @note Signature is uint32_t() for easier cross-RTOS integration; millis() is implicitly narrowed.
     */
    using TimeFn = uint32_t (*)();

public:
    // ---- Construction ---- //

    /**
     * @brief Construct a Button handler using native GPIO reads.
     * @param buttonPins Reference to an array of length N with pin IDs.
     * @param timing Global debounce/press-duration configuration.
     * @param skipPinInit If true, GPIO mode is not configured here. Set to true when pins are configured elsewhere.
     * @param timeFn Optional time source (ms). If nullptr, uses millis().
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = false,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, time_fn_{timeFn}
    {
        const uint32_t t0 = time_now();
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
            pending_short_[i] = false;
            pending_since_[i] = 0;
        }
    }

    /**
     * @brief Construct with a per-pin fast reader function.
     * @param buttonPins Reference to an array of length N with pin IDs.
     * @param readPin Fast reader: bool(uint8_t id) returns pressed.
     * @param timing Global debounce/press-duration configuration.
     * @param skipPinInit If false, pins are set to INPUT_PULLUP here.
     * @param timeFn Optional time source (ms). If nullptr, uses millis().
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadPinFn readPin,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, read_pin_fn_{readPin}, time_fn_{timeFn}
    {
        const uint32_t t0 = time_now();
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
            pending_short_[i] = false;
            pending_since_[i] = 0;
        }
    }

    /**
     * @brief Construct with a context-aware reader callback.
     * @param buttonPins Reference to an array of length N with pin IDs.
     * @param readCb Reader: bool(void* ctx, uint8_t id) returns pressed.
     * @param ctx Pointer passed to readCb on each call.
     * @param timing Global debounce/press-duration configuration.
     * @param skipPinInit If false, pins are set to INPUT_PULLUP here.
     * @param timeFn Optional time source (ms). If nullptr, uses millis().
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadFn readCb, void *ctx,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, read_fn_{readCb}, read_ctx_{ctx}, time_fn_{timeFn}
    {
        const uint32_t t0 = time_now();
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
            pending_short_[i] = false;
            pending_since_[i] = 0;
        }
    }

    // ---- Configuration setters ---- //

    /**
     * @brief Set a fast per-pin reader.
     * @param fn Function pointer: bool(uint8_t id) => pressed?
     * @note If not set, the handler falls back to context reader or native GPIO.
     */
    void setReadPinFn(ReadPinFn fn) noexcept { read_pin_fn_ = fn; }

    /**
     * @brief Set a context-aware reader.
     * @param fn Function pointer: bool(void* ctx, uint8_t id) => pressed?
     * @param ctx Opaque pointer passed back to fn on each read.
     * @note If neither reader is set, native GPIO (active-low) is used.
     */
    void setReadFn(ReadFn fn, void *ctx) noexcept
    {
        read_fn_ = fn;
        read_ctx_ = ctx;
    }

    /**
     * @brief Inject a time source (milliseconds).
     * @param fn Function pointer: uint32_t() returning current time in ms.
     * @note If nullptr, uses Arduino ::millis() implicitly via time_now().
     */
    void setTimeFn(TimeFn fn) noexcept { time_fn_ = fn; }

    /**
     * @brief Override the global debounce/press-duration timings.
     * @param t New global timing configuration.
     * @note Per-button overrides (non-zero fields) still take precedence.
     */
    void setGlobalTiming(const ButtonTimingConfig &t) noexcept { timing_ = t; }

    /**
     * @brief Backward-compatible alias for setGlobalTiming().
     */
    void setTiming(const ButtonTimingConfig &t) noexcept { setGlobalTiming(t); }

    /**
     * @brief Apply per-button overrides/flags by numeric index.
     * @param id Button index [0..N-1].
     * @param c Overrides (0 => use global for timing fields).
     * @note Silently ignored if id is out of range.
     */
    void setPerConfig(uint8_t id, const ButtonPerConfig &c) noexcept
    {
        if (id < N)
            per_[id] = c;
    }

    /**
     * @brief Enable/disable a button at runtime.
     * @param id Button index [0..N-1].
     * @param en true to enable; false to disable.
     * @note Silently ignored if id is out of range.
     */
    void enable(uint8_t id, bool en) noexcept
    {
        if (id < N)
            per_[id].enabled = en;
    }

    /**
     * @brief Set active level (polarity) for a button at runtime.
     * @param id Button index [0..N-1].
     * @param activeLow true => LOW is pressed; false => HIGH is pressed.
     * @note Silently ignored if id is out of range.
     */
    void setActiveLow(uint8_t id, bool activeLow) noexcept
    {
        if (id < N)
            per_[id].active_low = activeLow;
    }

    /**
     * @brief Enum-friendly overload of setPerConfig().
     * @tparam E  Enum type (e.g., ButtonIndex) with underlying uint8_t.
     * @param e Enumerated button identifier.
     * @param c Per-button overrides (0 => use global for timing fields).
     * @note Inlines to the uint8_t overload; avoids casts at call sites.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    void setPerConfig(E e, const ButtonPerConfig &c) noexcept
    {
        setPerConfig(static_cast<uint8_t>(e), c);
    }

    /**
     * @brief Enum-friendly overload of enable().
     * @tparam E Enum type (e.g., ButtonIndex) with underlying uint8_t.
     * @param e Enumerated button identifier.
     * @param en true to enable; false to disable.
     * @note Inlines to the uint8_t overload; avoids casts at call sites.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    void enable(E e, bool en) noexcept
    {
        enable(static_cast<uint8_t>(e), en);
    }

    /**
     * @brief Enum-friendly overload of setActiveLow().
     * @tparam E Enum type (e.g., ButtonIndex) with underlying uint8_t.
     * @param e Enumerated button identifier.
     * @param activeLow true => LOW is pressed; false => HIGH is pressed.
     * @note Inlines to the uint8_t overload; avoids casts at call sites.
     */
    template <typename E, typename std::enable_if<std::is_enum<E>::value, int>::type = 0>
    void setActiveLow(E e, bool activeLow) noexcept
    {
        setActiveLow(static_cast<uint8_t>(e), activeLow);
    }

    // ---- IButtonHandler overrides (core per-button API) ---- //

    /**
     * @brief Number of logical buttons handled by this instance.
     * @return Compile-time constant (N) as uint8_t.
     */
    [[nodiscard]] uint8_t size() const noexcept override { return static_cast<uint8_t>(N); }

    /**
     * @brief Scan and process button states using the configured time source.
     */
    void update() noexcept override { update(time_now()); }

    /**
     * @brief Scan and process button states using a provided timestamp.
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
            const uint32_t dcms = per_[i].double_click_ms ? static_cast<uint32_t>(per_[i].double_click_ms) : timing_.double_click_ms;

            // Flush pending single if window elapsed and no event queued yet.
            if (pending_short_[i] && event_[i] == ButtonPressType::None)
            {
                const uint32_t dt = now - pending_since_[i];
                if (dt >= dcms)
                {
                    event_[i] = ButtonPressType::Short;
                    pending_short_[i] = false;
                }
            }

            // Read raw physical level: prefer fast per-pin fn, else ctx-callback, else native.
            const bool pressed_default =
                (read_pin_fn_) ? read_pin_fn_(pins_[i])
                               : (read_fn_ ? read_fn_(read_ctx_, pins_[i]) : (digitalRead(pins_[i]) == LOW));

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
                    // Transition: released -> pressed (commit).
                    press_start_[i] = now;
                }
                else
                {
                    // Transition: pressed -> released (commit).
                    const uint32_t duration = press_start_[i] ? (now - press_start_[i]) : 0U;
                    last_duration_[i] = duration; ///< Record exact duration for retrieval.

                    if (duration >= lms)
                        event_[i] = ButtonPressType::Long;
                    else if (duration >= sms)
                    {
                        // Short press: either completes a double or starts a pending single.
                        if (pending_short_[i] && (now - pending_since_[i]) <= dcms)
                        {
                            event_[i] = ButtonPressType::Double;
                            pending_short_[i] = false;
                        }
                        else
                        {
                            // Defer emitting Short; it will fire if no 2nd press arrives within dcms.
                            pending_short_[i] = true;
                            pending_since_[i] = now;
                        }
                    }
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
     * @return ButtonPressType::Short, ::Long, ::None if no new event.
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
     * @return Milliseconds of the most recent *completed* press for buttonId.
     * @note Non-consuming: value persists until the next completed press or reset().
     */
    [[nodiscard]] uint32_t getLastPressDuration(uint8_t buttonId) const noexcept override
    {
        return (buttonId < N) ? last_duration_[buttonId] : 0U;
    }

    /**
     * @brief Clear all pending events and re-initialize debounced state.
     */
    void reset() noexcept override
    {
        const uint32_t t0 = time_now();
        for (size_t i = 0; i < N; ++i)
        {
            last_state_[i] = false;      ///< Committed (debounced).
            last_state_read_[i] = false; ///< Last raw (post-polarity) state.
            last_state_change_[i] = t0;  ///< Restart debounce window.
            press_start_[i] = 0;
            event_[i] = ButtonPressType::None;
            last_duration_[i] = 0;
            pending_short_[i] = false;
            pending_since_[i] = 0;
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
     * @brief Enum-friendly wrapper for querying the exact duration (ms) of the last completed press.
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

private:
    // ---- Storage ---- //

    uint8_t pins_[N]{};             ///< Pin or logical IDs.
    bool last_state_[N];            ///< Last committed (debounced) state.
    bool last_state_read_[N]{};     ///< Most recent raw state (after polarity).
    uint32_t last_state_change_[N]; ///< Timestamp (ms) when raw state last changed.
    uint32_t press_start_[N];       ///< Timestamp (ms) when press started (committed).
    ButtonPressType event_[N];      ///< Pending event (short/long/double) per button.
    bool pending_short_[N]{};       ///< Pending single waiting for possible double.
    uint32_t pending_since_[N]{};   ///< Timestamp (ms) of first short release.
    ButtonPerConfig per_[N];        ///< Per-button overrides (timing, polarity, enable).
    ButtonTimingConfig timing_;     ///< Global debounce and press-duration configuration.
    uint32_t last_duration_[N];     ///< Last measured press duration (ms), set on release.

    // ---- Readers ---- //

    ReadPinFn read_pin_fn_{nullptr}; ///< Optional fast-path reader (per-pin).
    ReadFn read_fn_{nullptr};        ///< Optional context-aware reader.
    void *read_ctx_{nullptr};        ///< Opaque context for @c read_fn_.

    // ---- Time source ---- //

    TimeFn time_fn_{nullptr};

    /**
     * @brief Resolve the current time in ms. Uses injected TimeFn when provided, otherwise Arduino millis().
     */
    inline uint32_t time_now() const noexcept
    {
        return time_fn_ ? time_fn_() : millis();
    }
};