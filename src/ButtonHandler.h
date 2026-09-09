/**
 * MIT License
 *
 * @brief Fixed-size multi-button handler with debouncing, interaction events, latching, and input health.
 *
 * @file ButtonHandler.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include "ButtonCompatibility.h"
#include "ButtonTypes.h"
#include "IButtonHandler.h"

#ifndef UB_EVENT_QUEUE_CAPACITY
#define UB_EVENT_QUEUE_CAPACITY 16u
#endif

static_assert(UB_EVENT_QUEUE_CAPACITY > 0u, "Universal_Button: UB_EVENT_QUEUE_CAPACITY must be greater than zero.");
static_assert(UB_EVENT_QUEUE_CAPACITY <= 255u, "Universal_Button: UB_EVENT_QUEUE_CAPACITY must fit in uint8_t.");

/**
 * @brief Tag used by constructors/factories whose bool reader returns an electrical HIGH/LOW level.
 */
struct ButtonElectricalReaderTag
{
};

/** @brief Constant tag for electrical-level reader construction. */
constexpr ButtonElectricalReaderTag BUTTON_ELECTRICAL_READER{};

/**
 * @brief Generic fixed-size button handler.
 *
 * @details The handler separates four concerns:
 *          - acquisition: obtain a trustworthy logical/electrical state;
 *          - state: debounce into a stable pressed/released level;
 *          - interaction: classify short/double/long behavior;
 *          - events: preserve finalized interactions in a bounded queue.
 *
 * Logical pressed-state callbacks return `true` when pressed and are never
 * polarity-transformed. Electrical-level callbacks return HIGH/LOW and use
 * ButtonPerConfig::active_low to derive the logical state.
 *
 * @tparam N Number of logical buttons managed by the instance.
 */
template <size_t N>
class ButtonHandler : public IButtonHandler
{
    static_assert(N > 0u, "ButtonHandler<N>: N must be greater than zero.");
    static_assert(N <= 255u, "ButtonHandler<N>: N must fit the uint8_t public API.");

public:
    // ---- Reader and time types ---- //

    /** @brief Legacy/documented callback: returns logical pressed state. */
    using ReadPinFn = bool (*)(uint8_t id);

    /** @brief Legacy/documented context callback: returns logical pressed state. */
    using ReadFn = bool (*)(void *ctx, uint8_t id);

    /** @brief Validity-aware logical pressed-state callback. */
    using ReadResultPinFn = ButtonPressedResult (*)(uint8_t id);

    /** @brief Validity-aware logical pressed-state context callback. */
    using ReadResultFn = ButtonPressedResult (*)(void *ctx, uint8_t id);

    /** @brief Electrical-level callback: true means HIGH, false means LOW. */
    using LevelReadPinFn = bool (*)(uint8_t id);

    /** @brief Electrical-level context callback: true means HIGH, false means LOW. */
    using LevelReadFn = bool (*)(void *ctx, uint8_t id);

    /** @brief Validity-aware electrical-level callback. */
    using LevelReadResultPinFn = ButtonLevelResult (*)(uint8_t id);

    /** @brief Validity-aware electrical-level context callback. */
    using LevelReadResultFn = ButtonLevelResult (*)(void *ctx, uint8_t id);

    /** @brief Millisecond time source. */
    using TimeFn = uint32_t (*)();

public:
    // ---- Construction ---- //

    /**
     * @brief Construct with native Arduino GPIO input.
     *
     * @param buttonPins Array of N physical pin IDs.
     * @param timing Global timing configuration.
     * @param skipPinInit True when pin configuration is owned elsewhere.
     * @param timeFn Optional millisecond time source.
     * @note Native GPIO is an electrical-level source and defaults to INPUT_PULLUP/active-low.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = false,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::NativeElectrical}, time_fn_{timeFn}
    {
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a logical pressed-state callback.
     *
     * @param buttonPins Array of N pin/key IDs passed to @p readPin.
     * @param readPin Reader returning true when the button is pressed.
     * @param timing Global timing configuration.
     * @param skipPinInit True to skip native pin initialization.
     * @param timeFn Optional millisecond time source.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadPinFn readPin,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::PressedPin}, read_pin_fn_{readPin}, time_fn_{timeFn}
    {
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a context-aware logical pressed-state callback.
     *
     * @param buttonPins Array of N pin/key IDs passed to @p readCb.
     * @param readCb Reader returning true when the button is pressed.
     * @param ctx Opaque callback context.
     * @param timing Global timing configuration.
     * @param skipPinInit True to skip native pin initialization.
     * @param timeFn Optional millisecond time source.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadFn readCb,
                  void *ctx,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::PressedCtx}, read_fn_{readCb}, read_ctx_{ctx}, time_fn_{timeFn}
    {
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a validity-aware logical pressed-state callback.
     *
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param readPin Reader reporting acquisition validity and logical pressed state.
     * @param timing Global debounce and interaction timings in milliseconds.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     * @param timeFn Optional borrowed millisecond clock callback; null uses Arduino millis() or zero on host.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadResultPinFn readPin,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::PressedResultPin}, read_result_pin_fn_{readPin}, time_fn_{timeFn}
    {
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a validity-aware context logical pressed-state callback.
     *
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param readCb Reader reporting acquisition validity and logical pressed state.
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @param timing Global debounce and interaction timings in milliseconds.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     * @param timeFn Optional borrowed millisecond clock callback; null uses Arduino millis() or zero on host.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ReadResultFn readCb,
                  void *ctx,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::PressedResultCtx}, read_result_fn_{readCb}, read_ctx_{ctx}, time_fn_{timeFn}
    {
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with an electrical-level callback.
     *
     * @param tag Must be BUTTON_ELECTRICAL_READER; distinguishes electrical HIGH/LOW from logical pressed callbacks.
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param readPin Reader returning true for electrical HIGH (false for LOW).
     * @param timing Global debounce and interaction timings in milliseconds.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     * @param timeFn Optional borrowed millisecond clock callback; null uses Arduino millis() or zero on host.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonElectricalReaderTag tag,
                  LevelReadPinFn readPin,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::ElectricalPin}, level_read_pin_fn_{readPin}, time_fn_{timeFn}
    {
        (void)tag;
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a context-aware electrical-level callback.
     *
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param tag BUTTON_ELECTRICAL_READER, selecting HIGH/LOW interpretation with per-button polarity.
     * @param readCb Reader returning true for electrical HIGH (false for LOW).
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @param timing Global debounce and interaction timings in milliseconds.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     * @param timeFn Optional borrowed millisecond clock callback; null uses Arduino millis() or zero on host.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonElectricalReaderTag tag,
                  LevelReadFn readCb,
                  void *ctx,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::ElectricalCtx}, level_read_fn_{readCb}, read_ctx_{ctx}, time_fn_{timeFn}
    {
        (void)tag;
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a validity-aware electrical-level callback.
     *
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param tag BUTTON_ELECTRICAL_READER, selecting HIGH/LOW interpretation with per-button polarity.
     * @param readPin Reader reporting acquisition validity and electrical HIGH/LOW.
     * @param timing Global debounce and interaction timings in milliseconds.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     * @param timeFn Optional borrowed millisecond clock callback; null uses Arduino millis() or zero on host.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonElectricalReaderTag tag,
                  LevelReadResultPinFn readPin,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::ElectricalResultPin}, level_result_pin_fn_{readPin}, time_fn_{timeFn}
    {
        (void)tag;
        initialize_(buttonPins, skipPinInit);
    }

    /**
     * @brief Construct with a validity-aware context electrical-level callback.
     *
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param tag BUTTON_ELECTRICAL_READER, selecting HIGH/LOW interpretation with per-button polarity.
     * @param readCb Reader reporting acquisition validity and electrical HIGH/LOW.
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @param timing Global debounce and interaction timings in milliseconds.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     * @param timeFn Optional borrowed millisecond clock callback; null uses Arduino millis() or zero on host.
     */
    ButtonHandler(const uint8_t (&buttonPins)[N],
                  ButtonElectricalReaderTag tag,
                  LevelReadResultFn readCb,
                  void *ctx,
                  ButtonTimingConfig timing = {},
                  bool skipPinInit = true,
                  TimeFn timeFn = nullptr) noexcept
        : timing_{timing}, reader_kind_{ReaderKind::ElectricalResultCtx}, level_result_fn_{readCb}, read_ctx_{ctx}, time_fn_{timeFn}
    {
        (void)tag;
        initialize_(buttonPins, skipPinInit);
    }

    // ---- Configuration ---- //

    /**
     * @brief Replace the reader with a logical pressed-state callback and synchronize without events.
     *
     * @return True when the new reader was installed and all enabled buttons synchronized successfully.
     * @param fn Reader returning true for logically pressed (no polarity transform). Null is rejected; failed synchronization restores the prior reader.
     */
    bool setReadPinFn(ReadPinFn fn) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        reader_kind_ = ReaderKind::PressedPin;
        read_pin_fn_ = fn;
        clearOtherReaders_();
        read_pin_fn_ = fn;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Replace the reader with a context-aware logical pressed-state callback and synchronize.
     *
     * @param fn Reader returning true for logically pressed (no polarity transform). Null is rejected; failed synchronization restores the prior reader.
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setReadFn(ReadFn fn, void *ctx) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::PressedCtx;
        read_fn_ = fn;
        read_ctx_ = ctx;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Install a validity-aware logical pressed-state callback and synchronize.
     *
     * @param fn Reader reporting acquisition validity and logical pressed state. Null is rejected; failed synchronization restores the prior reader.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setReadResultPinFn(ReadResultPinFn fn) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::PressedResultPin;
        read_result_pin_fn_ = fn;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Install a validity-aware context logical pressed-state callback and synchronize.
     *
     * @param fn Reader reporting acquisition validity and logical pressed state. Null is rejected; failed synchronization restores the prior reader.
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setReadResultFn(ReadResultFn fn, void *ctx) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::PressedResultCtx;
        read_result_fn_ = fn;
        read_ctx_ = ctx;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Install an electrical-level callback and synchronize.
     *
     * @param fn Reader returning true for electrical HIGH (false for LOW). Null is rejected; failed synchronization restores the prior reader.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setElectricalReadPinFn(LevelReadPinFn fn) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::ElectricalPin;
        level_read_pin_fn_ = fn;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Install a context-aware electrical-level callback and synchronize.
     *
     * @param fn Reader returning true for electrical HIGH (false for LOW). Null is rejected; failed synchronization restores the prior reader.
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setElectricalReadFn(LevelReadFn fn, void *ctx) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::ElectricalCtx;
        level_read_fn_ = fn;
        read_ctx_ = ctx;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Install a validity-aware electrical-level callback and synchronize.
     *
     * @param fn Reader reporting acquisition validity and electrical HIGH/LOW. Null is rejected; failed synchronization restores the prior reader.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setElectricalReadResultPinFn(LevelReadResultPinFn fn) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::ElectricalResultPin;
        level_result_pin_fn_ = fn;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Install a validity-aware context electrical-level callback and synchronize.
     *
     * @param fn Reader reporting acquisition validity and electrical HIGH/LOW. Null is rejected; failed synchronization restores the prior reader.
     * @param ctx Borrowed reader context; must remain valid while the reader is installed.
     * @return True if the reader was installed and all enabled inputs synchronized; false after rejection or rollback.
     */
    bool setElectricalReadResultFn(LevelReadResultFn fn, void *ctx) noexcept
    {
        const ReaderSnapshot old = readerSnapshot_();
        clearOtherReaders_();
        reader_kind_ = ReaderKind::ElectricalResultCtx;
        level_result_fn_ = fn;
        read_ctx_ = ctx;
        if (sync(time_now()))
            return true;
        restoreReader_(old);
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Replace the millisecond time source and rebaseline interaction timers.
     *
     * @param fn New time source. nullptr uses Arduino millis() when available.
     * @return True when synchronization in the new time domain succeeded.
     */
    bool setTimeFn(TimeFn fn) noexcept
    {
        const TimeFn old = time_fn_;
        time_fn_ = fn;
        if (sync(time_now()))
            return true;
        time_fn_ = old;
        (void)sync(time_now());
        return false;
    }

    /**
     * @brief Validate and apply global timings, then synchronize all enabled inputs.
     *
     * @param t New global timing configuration.
     * @return Detailed validation/synchronization result.
     */
    ButtonConfigResult setGlobalTiming(const ButtonTimingConfig &t) noexcept
    {
        ButtonConfigResult result = validateAllWithGlobal_(t);
        if (!result)
            return result;

        const ButtonTimingConfig old = timing_;
        const bool old_config_valid = config_valid_;
        const ButtonConfigError old_config_error = config_error_;
        timing_ = t;
        config_valid_ = true;
        config_error_ = ButtonConfigError::None;
        if (!sync(time_now()))
        {
            timing_ = old;
            config_valid_ = old_config_valid;
            config_error_ = old_config_error;
            (void)sync(time_now());
            return ButtonConfigResult{false, ButtonConfigError::SynchronizationFailed, 0xFFu};
        }
        return ButtonConfigResult{true, ButtonConfigError::None, 0xFFu};
    }

    /**
     * @brief Backward-compatible alias for setGlobalTiming().
     *
     * @param t Global timing proposal in milliseconds.
     * @return Validation/synchronization result from setGlobalTiming().
     */
    ButtonConfigResult setTiming(const ButtonTimingConfig &t) noexcept { return setGlobalTiming(t); }

    /**
     * @brief Validate and apply per-button configuration.
     *
     * Disabling clears the latch and legacy change flag while counting any actual
     * latch transition. Queued interactions remain available.
     *
     * @param id Button index.
     * @param c New per-button configuration.
     * @return Detailed validation/synchronization result.
     */
    ButtonConfigResult setPerConfig(uint8_t id, const ButtonPerConfig &c) noexcept
    {
        if (id >= N)
            return ButtonConfigResult{false, ButtonConfigError::InvalidButton, id};

        const ButtonConfigResult validation = validateResolved_(timing_, c, id);
        if (!validation)
            return validation;

        const ButtonPerConfig old = per_[id];
        per_[id] = c;

        if (!c.enabled)
        {
            resetDisabledButton_(id, time_now());
            return ButtonConfigResult{true, ButtonConfigError::None, id};
        }

        if (!syncButton_(id, time_now()))
        {
            per_[id] = old;
            (void)syncButton_(id, time_now());
            return ButtonConfigResult{false, ButtonConfigError::SynchronizationFailed, id};
        }

        return ButtonConfigResult{true, ButtonConfigError::None, id};
    }

    /**
     * @brief Enable or disable one button.
     *
     * Disable and failed re-enable cleanup clear the latch and legacy change flag,
     * retaining durable transition counts and already queued interactions.
     *
     * @param id Button index.
     * @param en Desired enable state.
     * @return Configuration/synchronization result.
     */
    ButtonConfigResult enable(uint8_t id, bool en) noexcept
    {
        if (id >= N)
            return ButtonConfigResult{false, ButtonConfigError::InvalidButton, id};

        if (per_[id].enabled == en)
            return ButtonConfigResult{true, ButtonConfigError::None, id};

        if (!en)
        {
            per_[id].enabled = false;
            resetDisabledButton_(id, time_now());
            return ButtonConfigResult{true, ButtonConfigError::None, id};
        }

        per_[id].enabled = true;
        if (!syncButton_(id, time_now()))
        {
            per_[id].enabled = false;
            resetDisabledButton_(id, time_now());
            return ButtonConfigResult{false, ButtonConfigError::SynchronizationFailed, id};
        }
        return ButtonConfigResult{true, ButtonConfigError::None, id};
    }

    /**
     * @brief Change electrical polarity and rebaseline without synthetic edges.
     *
     * @param id Button index.
     * @param activeLow True when LOW means pressed for native/electrical readers.
     * @return Configuration/synchronization result.
     * @note Logical pressed-state callbacks are already polarity-corrected; this setting has no effect on them.
     */
    ButtonConfigResult setActiveLow(uint8_t id, bool activeLow) noexcept
    {
        if (id >= N)
            return ButtonConfigResult{false, ButtonConfigError::InvalidButton, id};

        const bool old = per_[id].active_low;
        per_[id].active_low = activeLow;

        // Logical readers already return the final pressed state, so polarity
        // metadata has no effect and should not disturb an in-progress interaction.
        if (!readerUsesElectricalPolarity_())
            return ButtonConfigResult{true, ButtonConfigError::None, id};

        if (per_[id].enabled && !syncButton_(id, time_now()))
        {
            per_[id].active_low = old;
            (void)syncButton_(id, time_now());
            return ButtonConfigResult{false, ButtonConfigError::SynchronizationFailed, id};
        }
        return ButtonConfigResult{true, ButtonConfigError::None, id};
    }

    // ---- Enum-friendly configuration ---- //

    /**
     * @brief Apply per-button configuration using an enum identifier.
     *
     * @param e Enum button identifier, converted to uint8_t.
     * @param c Proposed per-button configuration.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Validation/synchronization result; invalid indices leave configuration unchanged.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    ButtonConfigResult setPerConfig(E e, const ButtonPerConfig &c) noexcept
    {
        return setPerConfig(static_cast<uint8_t>(e), c);
    }

    /**
     * @brief Enable or disable a button using an enum identifier.
     *
     * @param e Enum button identifier, converted to uint8_t.
     * @param en Desired enabled state.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Success for an applied change or no-op; otherwise invalid-index or synchronization failure.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    ButtonConfigResult enable(E e, bool en) noexcept
    {
        return enable(static_cast<uint8_t>(e), en);
    }

    /**
     * @brief Set electrical polarity using an enum identifier.
     *
     * @param e Enum button identifier, converted to uint8_t.
     * @param activeLow True when an electrical LOW means pressed.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Configuration result; logical readers retain their pressed-state interpretation.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    ButtonConfigResult setActiveLow(E e, bool activeLow) noexcept
    {
        return setActiveLow(static_cast<uint8_t>(e), activeLow);
    }

    // ---- State and update ---- //

    /**
     * @brief Number of logical buttons managed by this handler.
     *
     * @return Number of logical buttons, from 1 to 255.
     */
    UB_NODISCARD uint8_t size() const noexcept override { return static_cast<uint8_t>(N); }

    /**
     * @brief Compile-time number of logical buttons.
     *
     * @return Number of logical buttons, from 1 to 255.
     */
    UB_NODISCARD static constexpr uint8_t sizeStatic() noexcept { return static_cast<uint8_t>(N); }

    /**
     * @brief Iterate current debounced button levels.
     *
     * @tparam F Callable accepting `(uint8_t index, bool pressed)`.
     * @param f Callback invoked with each index and debounced pressed level.
     */
    template <typename F>
    void forEach(F &&f) const noexcept
    {
        for (size_t i = 0; i < N; ++i)
            f(static_cast<uint8_t>(i), last_state_[i]);
    }

    /** @brief Acquire/process all enabled buttons using the configured time source. */
    void update() noexcept override { update(time_now()); }

    /**
     * @brief Acquire/process all enabled buttons using an explicit millisecond timestamp.
     *
     * @param now Current time in milliseconds. Unsigned subtraction preserves millis() wraparound behavior.
     */
    void update(uint32_t now) noexcept override
    {
        if (!config_valid_)
            return;

        for (size_t i = 0; i < N; ++i)
        {
            if (!per_[i].enabled)
                continue;

            const ButtonPressedResult sample = acquire_(i);
            attempt_ms_[i] = now;

            if (!sample.valid)
            {
                recordFailure_(i, normalizeReadError_(sample.error), now);
                continue;
            }

            recordSuccess_(i, now);

            // A hardware observation gap makes the interaction history ambiguous.
            // Rebaseline this button without emitting synthetic edges/events.
            if (acquisition_gap_[i])
            {
                synchronizeFromSample_(i, sample.pressed, now);
                continue;
            }

            const ResolvedTiming t = resolvedTiming_(timing_, per_[i]);

            // If a previous short can no longer become a double, finalize it
            // before processing a newer interaction at this timestamp.
            if (pending_short_[i] && (now - pending_since_[i]) > t.double_click_ms)
            {
                finalizeInteraction_(i, ButtonPressType::Short, ButtonEventType::Short, now, pending_duration_[i]);
                pending_short_[i] = false;
            }

            // Debounce: restart the candidate timer whenever the observed logical level changes.
            if (sample.pressed != last_state_read_[i])
            {
                last_state_read_[i] = sample.pressed;
                last_state_change_ms_[i] = now;
            }

            // Commit a level only after the candidate has remained continuously stable.
            if (last_state_[i] != last_state_read_[i] && (now - last_state_change_ms_[i]) >= t.debounce_ms)
            {
                last_state_[i] = last_state_read_[i];
                ++change_sequence_[i];

                if (last_state_[i])
                {
                    press_start_ms_[i] = now;
                    has_press_start_[i] = true;
                    long_started_[i] = false;
                    suppress_until_release_[i] = false;
                }
                else
                {
                    processRelease_(i, now, t);
                }
            }

            // LongStarted is a threshold event while the stable level is still held.
            if (last_state_[i] && has_press_start_[i] && !long_started_[i] &&
                (now - press_start_ms_[i]) >= t.long_press_ms)
            {
                long_started_[i] = true;
                pushEvent_(static_cast<uint8_t>(i), ButtonEventType::LongStarted, now,
                           now - press_start_ms_[i]);
            }

            // A pending short is safe to finalize once the complete double-click
            // window has expired and no release at this timestamp completed a double.
            if (pending_short_[i] && (now - pending_since_[i]) > t.double_click_ms)
            {
                finalizeInteraction_(i, ButtonPressType::Short, ButtonEventType::Short, now, pending_duration_[i]);
                pending_short_[i] = false;
            }
        }
    }

    /**
     * @brief Current debounced logical pressed state.
     *
     * @param buttonId Logical button index.
     * @return True for a debounced pressed level; false for an invalid index.
     */
    UB_NODISCARD bool isPressed(uint8_t buttonId) const noexcept override
    {
        return (buttonId < N) ? last_state_[buttonId] : false;
    }

    /**
     * @brief True once the long threshold has been reached while the button remains held.
     *
     * @note Use this level for hold-to-enable behavior rather than a one-shot event.
     * @param buttonId Logical button index.
     * @return True for an observed held press past its long threshold; false for an invalid index.
     */
    UB_NODISCARD bool isLongHeld(uint8_t buttonId) const noexcept
    {
        return (buttonId < N) ? (last_state_[buttonId] && long_started_[buttonId]) : false;
    }

    // ---- Detailed event queue ---- //

    /**
     * @brief Pop the oldest detailed event across all buttons.
     *
     * @param out Receives the event on success; remains unchanged when the queue is empty.
     * @return True when an event was returned.
     */
    bool popEvent(ButtonEvent &out) noexcept
    {
        if (event_count_ == 0u)
            return false;
        out = event_queue_[0];
        removeEventAt_(0u);
        return true;
    }

    /**
     * @brief Peek at the oldest detailed event without consuming it.
     *
     * @param out Receives the oldest event when available; otherwise remains unchanged.
     * @return True when out receives an event; false if the queue is empty.
     */
    UB_NODISCARD bool peekEvent(ButtonEvent &out) const noexcept
    {
        if (event_count_ == 0u)
            return false;
        out = event_queue_[0];
        return true;
    }

    /**
     * @brief Number of detailed events currently waiting in the bounded queue.
     *
     * @return Number of currently queued detailed events.
     */
    UB_NODISCARD uint8_t pendingEventCount() const noexcept { return event_count_; }

    /**
     * @brief Monotonic count of detailed events generated, including any dropped because the queue was full.
     *
     * @return Generated event count modulo 2^32, including dropped events.
     */
    UB_NODISCARD uint32_t eventSequence() const noexcept { return event_sequence_; }

    /**
     * @brief Number of detailed events dropped because the bounded queue was full.
     *
     * @return Dropped event count modulo 2^32.
     */
    UB_NODISCARD uint32_t droppedEventCount() const noexcept { return dropped_events_; }

    /**
     * @brief True when at least one event has been dropped since the flag was cleared.
     *
     * @return True if an event has been dropped since clearEventOverflow().
     */
    UB_NODISCARD bool eventOverflowed() const noexcept { return event_overflow_; }

    /** @brief Clear the sticky overflow indication without altering the dropped-event counter. */
    void clearEventOverflow() noexcept { event_overflow_ = false; }

    // ---- Legacy completed-interaction API ---- //

    /**
     * @brief Get and consume the oldest completed interaction for one button.
     *
     * @return Short, Long, Double, or None.
     * @note LongStarted records for this button are consumed/ignored by the legacy API.
     *       Do not mix popEvent() and getPressType() as independent consumers of the same queue.
     * @param buttonId Logical button index.
     */
    ButtonPressType getPressType(uint8_t buttonId) noexcept override
    {
        if (buttonId >= N)
            return ButtonPressType::None;

        uint8_t i = 0u;
        while (i < event_count_)
        {
            const ButtonEvent &evt = event_queue_[i];
            if (evt.button_id != buttonId)
            {
                ++i;
                continue;
            }

            if (evt.type == ButtonEventType::LongStarted)
            {
                removeEventAt_(i);
                continue;
            }

            const ButtonPressType mapped = toPressType_(evt.type);
            removeEventAt_(i);
            return mapped;
        }
        return ButtonPressType::None;
    }

    /**
     * @brief Peek at the oldest completed interaction for one button without consuming it.
     *
     * @param buttonId Logical button index.
     * @return Oldest completed Short, Long or Double; None if absent or the index is invalid.
     */
    UB_NODISCARD ButtonPressType peekPressType(uint8_t buttonId) const noexcept override
    {
        if (buttonId >= N)
            return ButtonPressType::None;
        for (uint8_t i = 0u; i < event_count_; ++i)
        {
            const ButtonEvent &evt = event_queue_[i];
            if (evt.button_id == buttonId && evt.type != ButtonEventType::LongStarted)
                return toPressType_(evt.type);
        }
        return ButtonPressType::None;
    }

    /**
     * @brief Exact observed duration of the most recent completed press.
     *
     * @param buttonId Logical button index.
     * @return Observed duration in milliseconds, or zero before completion or for an invalid index.
     */
    UB_NODISCARD uint32_t getLastPressDuration(uint8_t buttonId) const noexcept override
    {
        return (buttonId < N) ? last_duration_ms_[buttonId] : 0u;
    }

    // ---- Input health/freshness ---- //

    /**
     * @brief True when a valid timing configuration and usable reader are installed.
     *
     * @return True when timing is valid and the selected reader is usable.
     */
    UB_NODISCARD bool configured() const noexcept { return config_valid_ && readerConfigured_(); }

    /**
     * @brief Constructor/global timing validation error, or None when the active timing is valid.
     *
     * @return Active constructor/global timing error, or None.
     */
    UB_NODISCARD ButtonConfigError configError() const noexcept { return config_error_; }

    /**
     * @brief True when every enabled button's most recent acquisition succeeded.
     *
     * @return True if every enabled button has a valid current acquisition and the handler is configured.
     */
    UB_NODISCARD bool valid() const noexcept
    {
        if (!configured())
            return false;
        for (size_t i = 0; i < N; ++i)
            if (per_[i].enabled && !valid_[i])
                return false;
        return true;
    }

    /**
     * @brief True when every enabled button has at least one successful acquisition.
     *
     * @return True if at least one button is enabled and all enabled buttons have a successful sample.
     */
    UB_NODISCARD bool hasSample() const noexcept
    {
        bool any = false;
        for (size_t i = 0; i < N; ++i)
        {
            if (!per_[i].enabled)
                continue;
            any = true;
            if (!has_sample_[i])
                return false;
        }
        return any;
    }

    /**
     * @brief Per-button current acquisition validity.
     *
     * @param buttonId Logical button index.
     * @return True if the most recent acquisition succeeded; false for an invalid index.
     */
    UB_NODISCARD bool valid(uint8_t buttonId) const noexcept { return (buttonId < N) ? valid_[buttonId] : false; }

    /**
     * @brief Per-button successful-sample presence.
     *
     * @param buttonId Logical button index.
     * @return True if a successful sample has been acquired since lifecycle cleanup; false for an invalid index.
     */
    UB_NODISCARD bool hasSample(uint8_t buttonId) const noexcept { return (buttonId < N) ? has_sample_[buttonId] : false; }

    /**
     * @brief Timestamp of the last successful acquisition for one button.
     *
     * @param buttonId Logical button index.
     * @return Last successful sample time in milliseconds; zero when absent or the index is invalid.
     */
    UB_NODISCARD uint32_t sampleMs(uint8_t buttonId) const noexcept { return (buttonId < N) ? sample_ms_[buttonId] : 0u; }

    /**
     * @brief Successful acquisition sequence for one button.
     *
     * @param buttonId Logical button index.
     * @return Successful acquisition count modulo 2^32; zero for an invalid index.
     */
    UB_NODISCARD uint32_t sequence(uint8_t buttonId) const noexcept { return (buttonId < N) ? sample_sequence_[buttonId] : 0u; }

    /**
     * @brief Debounced level-transition sequence for one button.
     *
     * @param buttonId Logical button index.
     * @return Debounced level-transition count modulo 2^32; zero for an invalid index.
     */
    UB_NODISCARD uint32_t changeSequence(uint8_t buttonId) const noexcept { return (buttonId < N) ? change_sequence_[buttonId] : 0u; }

    /**
     * @brief Edge-free synchronization generation for one button.
     *
     * @param buttonId Logical button index.
     * @return Edge-free synchronization count modulo 2^32; zero for an invalid index.
     */
    UB_NODISCARD uint32_t generation(uint8_t buttonId) const noexcept { return (buttonId < N) ? generation_[buttonId] : 0u; }

    /**
     * @brief Snapshot of per-button acquisition/state metadata.
     *
     * @param buttonId Logical button index.
     * @return Metadata snapshot, or a default disabled/invalid snapshot for an invalid index.
     */
    UB_NODISCARD ButtonInputStatus inputStatus(uint8_t buttonId) const noexcept
    {
        ButtonInputStatus s{};
        if (buttonId >= N)
            return s;
        s.enabled = per_[buttonId].enabled;
        s.valid = valid_[buttonId];
        s.has_sample = has_sample_[buttonId];
        s.pressed = last_state_[buttonId];
        s.long_held = isLongHeld(buttonId);
        s.last_error = last_error_[buttonId];
        s.sample_ms = sample_ms_[buttonId];
        s.attempt_ms = attempt_ms_[buttonId];
        s.error_ms = error_ms_[buttonId];
        s.sample_sequence = sample_sequence_[buttonId];
        s.change_sequence = change_sequence_[buttonId];
        s.generation = generation_[buttonId];
        s.latch_change_sequence = latch_change_sequence_[buttonId];
        return s;
    }

    /**
     * @brief Snapshot of handler-wide configuration, health, and event-loss metadata.
     *
     * @return Current configuration/acquisition health and durable event-loss metadata.
     */
    UB_NODISCARD ButtonHandlerStatus status() const noexcept
    {
        ButtonHandlerStatus s{};
        s.configured = configured();
        s.valid = valid();
        s.has_sample = hasSample();
        s.event_overflow = event_overflow_;
        s.config_error = config_error_;
        s.event_sequence = event_sequence_;
        s.dropped_events = dropped_events_;
        return s;
    }

    // ---- Synchronization/reset ---- //

    /**
     * @brief Establish current physical levels as an edge-free baseline.
     *
     * @return True when every enabled button was acquired successfully.
     * @note Existing completed events and latch states are retained.
     */
    bool sync() noexcept { return sync(time_now()); }

    /**
     * @brief Timestamped form of sync().
     *
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     * @return True when all enabled inputs are successfully acquired and rebaselined.
     */
    bool sync(uint32_t now) noexcept
    {
        if (!config_valid_ || !readerConfigured_())
        {
            markAllMissingReader_(now);
            return false;
        }

        ButtonPressedResult samples[N];
        bool ok = true;
        for (size_t i = 0; i < N; ++i)
        {
            if (!per_[i].enabled)
            {
                samples[i] = ButtonPressedResult::success(false);
                continue;
            }
            samples[i] = acquire_(i);
            if (!samples[i].valid)
                ok = false;
        }

        if (!ok)
        {
            for (size_t i = 0; i < N; ++i)
            {
                if (!per_[i].enabled)
                    continue;
                attempt_ms_[i] = now;
                if (!samples[i].valid)
                    recordFailure_(i, normalizeReadError_(samples[i].error), now);
            }
            return false;
        }

        for (size_t i = 0; i < N; ++i)
        {
            if (!per_[i].enabled)
                continue;
            attempt_ms_[i] = now;
            recordSuccess_(i, now);
            synchronizeFromSample_(i, samples[i].pressed, now);
        }
        return true;
    }

    /**
     * @brief Explicit name for the safe runtime reconfiguration baseline operation.
     *
     * @return True when sync() establishes the current input baseline.
     */
    bool reconfigureAndSync() noexcept { return sync(); }

    /**
     * @brief Mark all enabled inputs invalid when source health is known outside the reader callback.
     *
     * @param error Failure reason to expose in inputStatus().
     * @note Stable levels are retained, interaction classification is cancelled, and the
     *       next successful update rebaselines each button without synthetic events.
     */
    void invalidate(ButtonReadError error = ButtonReadError::AcquisitionFailed) noexcept
    {
        const uint32_t now = time_now();
        const ButtonReadError normalized = normalizeReadError_(error);
        for (size_t i = 0; i < N; ++i)
        {
            if (!per_[i].enabled)
                continue;
            attempt_ms_[i] = now;
            recordFailure_(i, normalized, now);
        }
    }

    /**
     * @brief Mark one input invalid when source health is known outside the reader callback.
     *
     * @param buttonId Button index.
     * @param error Failure reason to expose in inputStatus().
     */
    void invalidate(uint8_t buttonId, ButtonReadError error = ButtonReadError::AcquisitionFailed) noexcept
    {
        if (buttonId >= N || !per_[buttonId].enabled)
            return;
        const uint32_t now = time_now();
        attempt_ms_[buttonId] = now;
        recordFailure_(buttonId, normalizeReadError_(error), now);
    }

    /**
     * @brief Clear queued interactions/runtime state, restore initial latch values, and synchronize current levels.
     *
     * @return True when the new baseline was acquired successfully.
     * @note Actual latch restoration advances its durable sequence even on failure;
     *       legacy latch-change flags are cleared and counters are never reset.
     */
    bool resetAndSync() noexcept
    {
        const uint32_t now = time_now();
        clearEventQueue_();
        for (size_t i = 0; i < N; ++i)
            resetRuntime_(i, now, true);
        return sync(now);
    }

    /**
     * @brief Reset and synchronize current physical levels.
     *
     * @note v2 no longer resets every button to an assumed released state.
     */
    void reset() noexcept override { (void)resetAndSync(); }

    // ---- Latching ---- //

    /**
     * @brief Query the current latched state.
     *
     * @param buttonId Logical button index.
     * @return True for an ON latch; false for OFF or an invalid index.
     */
    UB_NODISCARD bool isLatched(uint8_t buttonId) const noexcept override
    {
        return (buttonId < N) ? latched_.test(buttonId) : false;
    }

    /**
     * @brief Set a latch, counting actual changes without generating interaction events.
     *
     * @param id Logical button index.
     * @param on Desired latched state; unchanged values are no-ops and invalid indices are ignored.
     */
    void setLatched(uint8_t id, bool on) noexcept override
    {
        if (id >= N)
            return;
        const bool before = latched_.test(id);
        if (before == on)
            return;
        latched_.set(id, on);
        latched_changed_.set(id, true);
        ++latch_change_sequence_[id];
    }

    /**
     * @brief Clear all latches, counting only actual changes and setting their change flags.
     *
     */
    void clearAllLatched() noexcept override
    {
        for (size_t i = 0; i < N; ++i)
            if (latched_.test(i))
                setLatched(static_cast<uint8_t>(i), false);
    }

    /**
     * @brief Clear selected latches among buttons 0..31, counting only actual changes.
     *
     * @param mask Bit i selects button i for clearing; only buttons 0..31 are represented.
     */
    void clearLatchedMask(uint32_t mask) noexcept override
    {
        const size_t n = (N < 32u) ? N : 32u;
        for (size_t i = 0; i < n; ++i)
            if ((mask & (static_cast<uint32_t>(1u) << i)) != 0u && latched_.test(i))
                setLatched(static_cast<uint8_t>(i), false);
    }

    /**
     * @brief Read latched states for buttons 0..31 as a bitmask.
     *
     * @return Mask with bit i set for each ON latch among buttons 0..31.
     */
    UB_NODISCARD uint32_t latchedMask() const noexcept override
    {
        uint32_t mask = 0u;
        const size_t n = (N < 32u) ? N : 32u;
        for (size_t i = 0; i < n; ++i)
            if (latched_.test(i))
                mask |= (static_cast<uint32_t>(1u) << i);
        return mask;
    }

    /**
     * @brief Consume the latch-change flag; reset and disable cleanup also clear this flag.
     *
     * @param buttonId Logical button index.
     * @return Previous flag value, or false for an invalid index; durable sequence is unchanged.
     */
    bool getAndClearLatchedChanged(uint8_t buttonId) noexcept override
    {
        if (buttonId >= N)
            return false;
        const bool changed = latched_changed_.test(buttonId);
        latched_changed_.set(buttonId, false);
        return changed;
    }

    /**
     * @brief Durable latched-state transition sequence for one button.
     *
     * Includes actual reset/disable latch changes even if synchronization fails.
     * Reading this counter or consuming the legacy change flag does not clear it.
     *
     * @param buttonId Logical button index.
     * @return Latch transitions modulo 2^32, starting at zero; never cleared by reset. Zero for an invalid index.
     */
    UB_NODISCARD uint32_t latchChangeSequence(uint8_t buttonId) const noexcept
    {
        return (buttonId < N) ? latch_change_sequence_[buttonId] : 0u;
    }

    // ---- Enum-friendly state/latching ---- //

    /**
     * @brief Query the debounced pressed level using an enum identifier.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return True for a debounced pressed level; false for an invalid index.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    UB_NODISCARD bool isPressed(E id) const noexcept { return isPressed(static_cast<uint8_t>(id)); }

    /**
     * @brief Query whether an observed press has reached the long threshold using an enum identifier.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return True for an observed held press past its long threshold; false for an invalid index.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    UB_NODISCARD bool isLongHeld(E id) const noexcept { return isLongHeld(static_cast<uint8_t>(id)); }

    /**
     * @brief Consume the oldest completed interaction using an enum identifier.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Oldest completed Short, Long or Double; None if absent or the index is invalid.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    ButtonPressType getPressType(E id) noexcept { return getPressType(static_cast<uint8_t>(id)); }

    /**
     * @brief Inspect the oldest completed interaction using an enum identifier.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Oldest completed Short, Long or Double; None if absent or the index is invalid.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    UB_NODISCARD ButtonPressType peekPressType(E id) const noexcept { return peekPressType(static_cast<uint8_t>(id)); }

    /**
     * @brief Query the most recent completed press duration using an enum identifier.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Observed duration in milliseconds, or zero before completion or for an invalid index.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    UB_NODISCARD uint32_t getLastPressDuration(E id) const noexcept { return getLastPressDuration(static_cast<uint8_t>(id)); }

    /**
     * @brief Query the current latched state.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return True for an ON latch; false for OFF or an invalid index.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    UB_NODISCARD bool isLatched(E id) const noexcept { return isLatched(static_cast<uint8_t>(id)); }

    /**
     * @brief Set a latch, counting actual changes without generating interaction events.
     *
     * @param id Logical button index.
     * @param on Desired latched state; unchanged values are no-ops and invalid indices are ignored.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    void setLatched(E id, bool on) noexcept { setLatched(static_cast<uint8_t>(id), on); }

    /**
     * @brief Consume the latch-change flag; reset and disable cleanup also clear this flag.
     *
     * @param id Logical button index.
     * @tparam E Enum type representing logical button indices.
     * @tparam E Enum type representing logical button indices.
     * @return Previous flag value, or false for an invalid index; durable sequence is unchanged.
     */
    template <typename E, UB::compat::enable_if_t<UB::compat::is_enum<E>::value, int> = 0>
    bool getAndClearLatchedChanged(E id) noexcept { return getAndClearLatchedChanged(static_cast<uint8_t>(id)); }

private:
    enum class ReaderKind : uint8_t
    {
        NativeElectrical,
        PressedPin,
        PressedCtx,
        PressedResultPin,
        PressedResultCtx,
        ElectricalPin,
        ElectricalCtx,
        ElectricalResultPin,
        ElectricalResultCtx
    };

    struct ResolvedTiming
    {
        uint32_t debounce_ms;
        uint32_t short_press_ms;
        uint32_t long_press_ms;
        uint32_t double_click_ms;
    };

    struct ReaderSnapshot
    {
        ReaderKind kind;
        ReadPinFn read_pin;
        ReadFn read;
        ReadResultPinFn read_result_pin;
        ReadResultFn read_result;
        LevelReadPinFn level_pin;
        LevelReadFn level;
        LevelReadResultPinFn level_result_pin;
        LevelReadResultFn level_result;
        void *ctx;
    };

    // ---- Configuration/state storage ---- //

    uint8_t pins_[N]{};
    ButtonPerConfig per_[N]{};
    ButtonTimingConfig timing_{};
    bool config_valid_{true};
    ButtonConfigError config_error_{ButtonConfigError::None};

    bool last_state_[N]{};
    bool last_state_read_[N]{};
    uint32_t last_state_change_ms_[N]{};
    uint32_t press_start_ms_[N]{};
    bool has_press_start_[N]{};
    bool long_started_[N]{};
    bool suppress_until_release_[N]{}; ///< Baseline/recovery hold must release before new interaction events.
    uint32_t last_duration_ms_[N]{};
    bool pending_short_[N]{};
    uint32_t pending_since_[N]{};
    uint32_t pending_duration_[N]{};

    bool valid_[N]{};
    bool has_sample_[N]{};
    bool acquisition_gap_[N]{};
    ButtonReadError last_error_[N]{};
    uint32_t sample_ms_[N]{};
    uint32_t attempt_ms_[N]{};
    uint32_t error_ms_[N]{};
    uint32_t sample_sequence_[N]{};
    uint32_t change_sequence_[N]{};
    uint32_t generation_[N]{};

    UB::compat::bitset<N> latched_{};
    UB::compat::bitset<N> latched_changed_{};
    uint32_t latch_change_sequence_[N]{};

    ButtonEvent event_queue_[UB_EVENT_QUEUE_CAPACITY]{};
    uint8_t event_count_{0u};
    uint32_t event_sequence_{0u};
    uint32_t dropped_events_{0u};
    bool event_overflow_{false};

    // ---- Reader storage ---- //

    ReaderKind reader_kind_{ReaderKind::NativeElectrical};
    ReadPinFn read_pin_fn_{nullptr};
    ReadFn read_fn_{nullptr};
    ReadResultPinFn read_result_pin_fn_{nullptr};
    ReadResultFn read_result_fn_{nullptr};
    LevelReadPinFn level_read_pin_fn_{nullptr};
    LevelReadFn level_read_fn_{nullptr};
    LevelReadResultPinFn level_result_pin_fn_{nullptr};
    LevelReadResultFn level_result_fn_{nullptr};
    void *read_ctx_{nullptr};
    TimeFn time_fn_{nullptr};

    // ---- Initialization/config validation ---- //

    /**
     * @brief Initialize pins, runtime state and timing validity without reporting latch transitions.
     *
     * @param buttonPins Array of N physical pin or callback key IDs, copied into the handler.
     * @param skipPinInit True to leave pin setup to the application; callback readers never initialize native pins.
     */
    void initialize_(const uint8_t (&buttonPins)[N], bool skipPinInit) noexcept
    {
        const uint32_t now = time_now();
        for (size_t i = 0; i < N; ++i)
        {
            pins_[i] = buttonPins[i];
            if (!skipPinInit && reader_kind_ == ReaderKind::NativeElectrical)
                initPin_(pins_[i]);
            last_error_[i] = ButtonReadError::MissingReader;
            resetRuntime_(i, now, false);
            // Construction establishes the initial value; no runtime transition occurred.
            latched_.set(i, per_[i].latch_initial);
        }

        const ButtonConfigResult validation = validateAllWithGlobal_(timing_);
        config_valid_ = validation.ok;
        config_error_ = validation.error;
    }

    /**
     * @brief Resolve zero-valued per-button overrides against global timings.
     *
     * @param global Global timing thresholds in milliseconds.
     * @param per Per-button overrides; zero timing fields inherit the global value.
     * @return Effective millisecond thresholds after inheritance.
     */
    static ResolvedTiming resolvedTiming_(const ButtonTimingConfig &global, const ButtonPerConfig &per) noexcept
    {
        ResolvedTiming t{};
        t.debounce_ms = per.debounce_ms != 0u ? static_cast<uint32_t>(per.debounce_ms) : global.debounce_ms;
        t.short_press_ms = per.short_press_ms != 0u ? static_cast<uint32_t>(per.short_press_ms) : global.short_press_ms;
        t.long_press_ms = per.long_press_ms != 0u ? static_cast<uint32_t>(per.long_press_ms) : global.long_press_ms;
        t.double_click_ms = per.double_click_ms != 0u ? static_cast<uint32_t>(per.double_click_ms) : global.double_click_ms;
        return t;
    }

    /**
     * @brief Check resolved threshold ordering before applying a configuration.
     *
     * @param t Resolved timing thresholds in milliseconds.
     * @param id Logical button index.
     * @return First timing-order error or success, associated with id.
     */
    static ButtonConfigResult validateTiming_(const ResolvedTiming &t, uint8_t id) noexcept
    {
        if (t.short_press_ms == 0u)
            return ButtonConfigResult{false, ButtonConfigError::InvalidShortPress, id};
        if (t.long_press_ms <= t.short_press_ms)
            return ButtonConfigResult{false, ButtonConfigError::InvalidLongPress, id};
        if (t.debounce_ms > t.short_press_ms)
            return ButtonConfigResult{false, ButtonConfigError::InvalidDebounce, id};
        if (t.double_click_ms < t.debounce_ms)
            return ButtonConfigResult{false, ButtonConfigError::InvalidDoubleClick, id};
        return ButtonConfigResult{true, ButtonConfigError::None, id};
    }

    /**
     * @brief Validate one button after resolving inherited timing fields.
     *
     * @param global Global timing thresholds in milliseconds.
     * @param per Per-button overrides; zero timing fields inherit the global value.
     * @param id Logical button index.
     * @return Resolved timing validation result for id.
     */
    static ButtonConfigResult validateResolved_(const ButtonTimingConfig &global, const ButtonPerConfig &per, uint8_t id) noexcept
    {
        return validateTiming_(resolvedTiming_(global, per), id);
    }

    /**
     * @brief Validate proposed global timings against every per-button override.
     *
     * @param global Global timing thresholds in milliseconds.
     * @return First per-button validation failure, or global success with button_id 0xFF.
     */
    ButtonConfigResult validateAllWithGlobal_(const ButtonTimingConfig &global) const noexcept
    {
        for (size_t i = 0; i < N; ++i)
        {
            const ButtonConfigResult r = validateResolved_(global, per_[i], static_cast<uint8_t>(i));
            if (!r)
                return r;
        }
        return ButtonConfigResult{true, ButtonConfigError::None, 0xFFu};
    }

    // ---- Acquisition ---- //

    /**
     * @brief Determine whether acquisition requires electrical-to-logical normalization.
     *
     * @return True for native or explicit electrical readers; false for logical readers.
     */
    UB_NODISCARD bool readerUsesElectricalPolarity_() const noexcept
    {
        return reader_kind_ == ReaderKind::NativeElectrical ||
               reader_kind_ == ReaderKind::ElectricalPin ||
               reader_kind_ == ReaderKind::ElectricalCtx ||
               reader_kind_ == ReaderKind::ElectricalResultPin ||
               reader_kind_ == ReaderKind::ElectricalResultCtx;
    }

    /**
     * @brief Check that the selected reader exists in this build.
     *
     * @return True for an available native reader or a non-null selected callback.
     */
    UB_NODISCARD bool readerConfigured_() const noexcept
    {
        switch (reader_kind_)
        {
        case ReaderKind::NativeElectrical:
#if UB_HAS_ARDUINO
            return true;
#else
            return false;
#endif
        case ReaderKind::PressedPin:
            return read_pin_fn_ != nullptr;
        case ReaderKind::PressedCtx:
            return read_fn_ != nullptr;
        case ReaderKind::PressedResultPin:
            return read_result_pin_fn_ != nullptr;
        case ReaderKind::PressedResultCtx:
            return read_result_fn_ != nullptr;
        case ReaderKind::ElectricalPin:
            return level_read_pin_fn_ != nullptr;
        case ReaderKind::ElectricalCtx:
            return level_read_fn_ != nullptr;
        case ReaderKind::ElectricalResultPin:
            return level_result_pin_fn_ != nullptr;
        case ReaderKind::ElectricalResultCtx:
            return level_result_fn_ != nullptr;
        default:
            return false;
        }
    }

    /**
     * @brief Acquire one logical sample, applying polarity only to electrical readers.
     *
     * @param i Valid internal button index, less than N.
     * @return Logical sample with validity/error preserved; MissingReader if no source is available.
     */
    UB_NODISCARD ButtonPressedResult acquire_(size_t i) const noexcept
    {
        const uint8_t id = pins_[i];
        switch (reader_kind_)
        {
        case ReaderKind::NativeElectrical:
#if UB_HAS_ARDUINO
        {
            const bool high = digitalRead(id) != LOW;
            return ButtonPressedResult::success(per_[i].active_low ? !high : high);
        }
#else
            return ButtonPressedResult::failure(ButtonReadError::MissingReader);
#endif

        case ReaderKind::PressedPin:
            return read_pin_fn_ ? ButtonPressedResult::success(read_pin_fn_(id))
                                : ButtonPressedResult::failure(ButtonReadError::MissingReader);

        case ReaderKind::PressedCtx:
            return read_fn_ ? ButtonPressedResult::success(read_fn_(read_ctx_, id))
                            : ButtonPressedResult::failure(ButtonReadError::MissingReader);

        case ReaderKind::PressedResultPin:
            return read_result_pin_fn_ ? read_result_pin_fn_(id)
                                       : ButtonPressedResult::failure(ButtonReadError::MissingReader);

        case ReaderKind::PressedResultCtx:
            return read_result_fn_ ? read_result_fn_(read_ctx_, id)
                                   : ButtonPressedResult::failure(ButtonReadError::MissingReader);

        case ReaderKind::ElectricalPin:
            if (!level_read_pin_fn_)
                return ButtonPressedResult::failure(ButtonReadError::MissingReader);
            {
                const bool high = level_read_pin_fn_(id);
                return ButtonPressedResult::success(per_[i].active_low ? !high : high);
            }

        case ReaderKind::ElectricalCtx:
            if (!level_read_fn_)
                return ButtonPressedResult::failure(ButtonReadError::MissingReader);
            {
                const bool high = level_read_fn_(read_ctx_, id);
                return ButtonPressedResult::success(per_[i].active_low ? !high : high);
            }

        case ReaderKind::ElectricalResultPin:
            if (!level_result_pin_fn_)
                return ButtonPressedResult::failure(ButtonReadError::MissingReader);
            {
                const ButtonLevelResult r = level_result_pin_fn_(id);
                if (!r.valid)
                    return ButtonPressedResult::failure(normalizeReadError_(r.error));
                return ButtonPressedResult::success(per_[i].active_low ? !r.high : r.high);
            }

        case ReaderKind::ElectricalResultCtx:
            if (!level_result_fn_)
                return ButtonPressedResult::failure(ButtonReadError::MissingReader);
            {
                const ButtonLevelResult r = level_result_fn_(read_ctx_, id);
                if (!r.valid)
                    return ButtonPressedResult::failure(normalizeReadError_(r.error));
                return ButtonPressedResult::success(per_[i].active_low ? !r.high : r.high);
            }

        default:
            return ButtonPressedResult::failure(ButtonReadError::MissingReader);
        }
    }

    /**
     * @brief Ensure a failed acquisition never reports a success error code.
     *
     * @param error Acquisition failure reason.
     * @return AcquisitionFailed for None; otherwise the supplied error.
     */
    static ButtonReadError normalizeReadError_(ButtonReadError error) noexcept
    {
        return error == ButtonReadError::None ? ButtonReadError::AcquisitionFailed : error;
    }

    /**
     * @brief Record successful acquisition health, timestamp and sample sequence.
     *
     * @param i Valid internal button index, less than N.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     */
    void recordSuccess_(size_t i, uint32_t now) noexcept
    {
        valid_[i] = true;
        has_sample_[i] = true;
        last_error_[i] = ButtonReadError::None;
        sample_ms_[i] = now;
        ++sample_sequence_[i];
    }

    /**
     * @brief Invalidate acquisition and cancel ambiguous interactions while retaining stable level.
     *
     * @param i Valid internal button index, less than N.
     * @param error Acquisition failure reason.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     */
    void recordFailure_(size_t i, ButtonReadError error, uint32_t now) noexcept
    {
        valid_[i] = false;
        last_error_[i] = error;
        error_ms_[i] = now;
        acquisition_gap_[i] = true;

        // Once acquisition is unobserved, interaction history is ambiguous.
        // Stable level is retained, but one-shot classification is cancelled.
        pending_short_[i] = false;
        has_press_start_[i] = false;
        long_started_[i] = false;
    }

    /**
     * @brief Record missing-reader failures for all enabled buttons.
     *
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     */
    void markAllMissingReader_(uint32_t now) noexcept
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (!per_[i].enabled)
                continue;
            attempt_ms_[i] = now;
            recordFailure_(i, ButtonReadError::MissingReader, now);
        }
    }

    // ---- Debounce/interaction helpers ---- //

    /**
     * @brief Classify an observed release or end suppression of an unobserved held press.
     *
     * @param i Valid internal button index, less than N.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     * @param t Resolved timing thresholds in milliseconds.
     */
    void processRelease_(size_t i, uint32_t now, const ResolvedTiming &t) noexcept
    {
        if (suppress_until_release_[i])
        {
            suppress_until_release_[i] = false;
            press_start_ms_[i] = 0u;
            has_press_start_[i] = false;
            long_started_[i] = false;
            return;
        }

        const uint32_t duration = has_press_start_[i] ? (now - press_start_ms_[i]) : 0u;
        last_duration_ms_[i] = duration;

        if (duration >= t.long_press_ms)
        {
            if (!long_started_[i])
            {
                long_started_[i] = true;
                pushEvent_(static_cast<uint8_t>(i), ButtonEventType::LongStarted, now, duration);
            }
            finalizeInteraction_(i, ButtonPressType::Long, ButtonEventType::LongReleased, now, duration);
            pending_short_[i] = false;
        }
        else if (duration >= t.short_press_ms)
        {
            if (pending_short_[i] && (now - pending_since_[i]) <= t.double_click_ms)
            {
                pending_short_[i] = false;
                finalizeInteraction_(i, ButtonPressType::Double, ButtonEventType::Double, now, duration);
            }
            else
            {
                pending_short_[i] = true;
                pending_since_[i] = now;
                pending_duration_[i] = duration;
            }
        }
        else
        {
            // A sub-short press is intentionally not an interaction event.
        }

        press_start_ms_[i] = 0u;
        has_press_start_[i] = false;
        long_started_[i] = false;
    }

    /**
     * @brief Queue a completed interaction and apply its configured latch action.
     *
     * @param i Valid internal button index, less than N.
     * @param legacy Completed interaction type used to select the latch action.
     * @param detailed Detailed event kind to enqueue.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     * @param duration Observed press duration in milliseconds.
     */
    void finalizeInteraction_(size_t i,
                              ButtonPressType legacy,
                              ButtonEventType detailed,
                              uint32_t now,
                              uint32_t duration) noexcept
    {
        pushEvent_(static_cast<uint8_t>(i), detailed, now, duration);
        applyLatch_(i, legacy);
    }

    /**
     * @brief Establish an edge-free level baseline and suppress classification of an already-held input.
     *
     * @param i Valid internal button index, less than N.
     * @param pressed Trustworthy logical pressed state from the acquired sample.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     */
    void synchronizeFromSample_(size_t i, bool pressed, uint32_t now) noexcept
    {
        last_state_[i] = pressed;
        last_state_read_[i] = pressed;
        last_state_change_ms_[i] = now;
        pending_short_[i] = false;
        pending_since_[i] = 0u;
        pending_duration_[i] = 0u;
        long_started_[i] = false;
        acquisition_gap_[i] = false;
        ++generation_[i];

        if (pressed)
        {
            // A held level observed during sync/recovery is current stable state,
            // but it did not begin under observation. Suppress interaction
            // classification until a release establishes a clean boundary.
            press_start_ms_[i] = 0u;
            has_press_start_[i] = false;
            suppress_until_release_[i] = true;
        }
        else
        {
            press_start_ms_[i] = 0u;
            has_press_start_[i] = false;
            suppress_until_release_[i] = false;
        }
    }

    /**
     * @brief Acquire and rebaseline one enabled button without generating events.
     *
     * @param i Valid internal button index, less than N.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     * @return True if disabled or successfully synchronized; false on invalid configuration or acquisition failure.
     */
    bool syncButton_(size_t i, uint32_t now) noexcept
    {
        if (!per_[i].enabled)
            return true;
        if (!config_valid_ || !readerConfigured_())
        {
            attempt_ms_[i] = now;
            recordFailure_(i, ButtonReadError::MissingReader, now);
            return false;
        }
        const ButtonPressedResult sample = acquire_(i);
        attempt_ms_[i] = now;
        if (!sample.valid)
        {
            recordFailure_(i, normalizeReadError_(sample.error), now);
            return false;
        }
        recordSuccess_(i, now);
        synchronizeFromSample_(i, sample.pressed, now);
        return true;
    }

    /**
     * @brief Clear transient state, optionally restoring and accounting for the initial latch value.
     *
     * @param i Valid internal button index, less than N.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     * @param restoreInitialLatch True to restore latch_initial, count a real transition, and clear the legacy flag.
     */
    void resetRuntime_(size_t i, uint32_t now, bool restoreInitialLatch) noexcept
    {
        last_state_[i] = false;
        last_state_read_[i] = false;
        last_state_change_ms_[i] = now;
        press_start_ms_[i] = 0u;
        has_press_start_[i] = false;
        long_started_[i] = false;
        suppress_until_release_[i] = false;
        last_duration_ms_[i] = 0u;
        pending_short_[i] = false;
        pending_since_[i] = 0u;
        pending_duration_[i] = 0u;
        valid_[i] = false;
        has_sample_[i] = false;
        acquisition_gap_[i] = false;
        last_error_[i] = ButtonReadError::MissingReader;
        sample_ms_[i] = 0u;
        attempt_ms_[i] = 0u;
        error_ms_[i] = 0u;
        if (restoreInitialLatch)
        {
            // Account before acquisition, even if sync fails. Qualify the call so
            // lifecycle cleanup cannot invoke an application override.
            ButtonHandler::setLatched(static_cast<uint8_t>(i), per_[i].latch_initial);
            latched_changed_.set(i, false);
        }
    }

    /**
     * @brief Clear disabled runtime state and latch while retaining durable counters and queued events.
     *
     * @param i Valid internal button index, less than N.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     */
    void resetDisabledButton_(size_t i, uint32_t now) noexcept
    {
        resetRuntime_(i, now, false);
        ButtonHandler::setLatched(static_cast<uint8_t>(i), false);
        // Lifecycle controls clear the legacy notification, not the durable sequence.
        latched_changed_.set(i, false);
    }

    // ---- Event queue ---- //

    /**
     * @brief Count an event and append it, recording loss when the bounded queue is full.
     *
     * @param buttonId Logical button index.
     * @param type Detailed event kind.
     * @param now Current millisecond timestamp; elapsed calculations use unsigned subtraction.
     * @param duration Observed press duration in milliseconds.
     */
    void pushEvent_(uint8_t buttonId, ButtonEventType type, uint32_t now, uint32_t duration) noexcept
    {
        ++event_sequence_;
        if (event_count_ >= static_cast<uint8_t>(UB_EVENT_QUEUE_CAPACITY))
        {
            ++dropped_events_;
            event_overflow_ = true;
            return;
        }

        ButtonEvent evt{};
        evt.button_id = buttonId;
        evt.type = type;
        evt.timestamp_ms = now;
        evt.duration_ms = duration;
        evt.sequence = event_sequence_;
        event_queue_[event_count_] = evt;
        ++event_count_;
    }

    /**
     * @brief Remove a queued event while preserving the order of remaining entries.
     *
     * @param index Queue offset to remove; out-of-range offsets are ignored.
     */
    void removeEventAt_(uint8_t index) noexcept
    {
        if (index >= event_count_)
            return;
        for (uint8_t i = index; static_cast<uint16_t>(i) + 1u < event_count_; ++i)
            event_queue_[i] = event_queue_[static_cast<uint8_t>(i + 1u)];
        --event_count_;
    }

    /**
     * @brief Discard queued entries without clearing event sequence or overflow history.
     *
     */
    void clearEventQueue_() noexcept { event_count_ = 0u; }

    /**
     * @brief Map a detailed event to the legacy completed-interaction type.
     *
     * @param type Detailed event kind.
     * @return Short, Double or Long; None for LongStarted or an unknown value.
     */
    static ButtonPressType toPressType_(ButtonEventType type) noexcept
    {
        switch (type)
        {
        case ButtonEventType::Short:
            return ButtonPressType::Short;
        case ButtonEventType::Double:
            return ButtonPressType::Double;
        case ButtonEventType::LongReleased:
            return ButtonPressType::Long;
        case ButtonEventType::LongStarted:
        default:
            return ButtonPressType::None;
        }
    }

    // ---- Latching ---- //

    /**
     * @brief Determine whether a completed interaction matches a latch trigger.
     *
     * @param trigger Configured latch trigger.
     * @param event Completed interaction to match.
     * @return True only for the configured Short, Long or Double interaction.
     */
    static bool latchMatches_(LatchTrigger trigger, ButtonPressType event) noexcept
    {
        switch (trigger)
        {
        case LatchTrigger::Short:
            return event == ButtonPressType::Short;
        case LatchTrigger::Long:
            return event == ButtonPressType::Long;
        case LatchTrigger::Double:
            return event == ButtonPressType::Double;
        default:
            return false;
        }
    }

    /**
     * @brief Apply configured latching to a matching finalized interaction.
     *
     * @param i Valid internal button index, less than N.
     * @param event Completed interaction to match.
     */
    void applyLatch_(size_t i, ButtonPressType event) noexcept
    {
        if (!per_[i].latch_enabled || !latchMatches_(per_[i].latch_on, event))
            return;

        const bool before = latched_.test(i);
        bool after = before;
        switch (per_[i].latch_mode)
        {
        case LatchMode::Toggle:
            after = !before;
            break;
        case LatchMode::Set:
            after = true;
            break;
        case LatchMode::Reset:
            after = false;
            break;
        default:
            break;
        }

        if (after != before)
            setLatched(static_cast<uint8_t>(i), after);
    }

    // ---- Reader reconfiguration helpers ---- //

    /**
     * @brief Capture the active reader selection and borrowed context for rollback.
     *
     * @return Reader callbacks, kind and borrowed context needed to restore the current source.
     */
    ReaderSnapshot readerSnapshot_() const noexcept
    {
        ReaderSnapshot s{};
        s.kind = reader_kind_;
        s.read_pin = read_pin_fn_;
        s.read = read_fn_;
        s.read_result_pin = read_result_pin_fn_;
        s.read_result = read_result_fn_;
        s.level_pin = level_read_pin_fn_;
        s.level = level_read_fn_;
        s.level_result_pin = level_result_pin_fn_;
        s.level_result = level_result_fn_;
        s.ctx = read_ctx_;
        return s;
    }

    /**
     * @brief Restore a prior reader selection without acquiring input.
     *
     * @param s Previously captured reader selection and borrowed context.
     */
    void restoreReader_(const ReaderSnapshot &s) noexcept
    {
        reader_kind_ = s.kind;
        read_pin_fn_ = s.read_pin;
        read_fn_ = s.read;
        read_result_pin_fn_ = s.read_result_pin;
        read_result_fn_ = s.read_result;
        level_read_pin_fn_ = s.level_pin;
        level_read_fn_ = s.level;
        level_result_pin_fn_ = s.level_result_pin;
        level_result_fn_ = s.level_result;
        read_ctx_ = s.ctx;
    }

    /**
     * @brief Clear all reader callbacks and borrowed context before installing a replacement.
     *
     */
    void clearOtherReaders_() noexcept
    {
        read_pin_fn_ = nullptr;
        read_fn_ = nullptr;
        read_result_pin_fn_ = nullptr;
        read_result_fn_ = nullptr;
        level_read_pin_fn_ = nullptr;
        level_read_fn_ = nullptr;
        level_result_pin_fn_ = nullptr;
        level_result_fn_ = nullptr;
        read_ctx_ = nullptr;
    }

    // ---- Platform/time helpers ---- //

    /**
     * @brief Configure a native input with pull-up on Arduino; do nothing on host builds.
     *
     * @param pin Native GPIO number.
     */
    static void initPin_(uint8_t pin) noexcept
    {
#if UB_HAS_ARDUINO
        pinMode(pin, INPUT_PULLUP);
#else
        (void)pin;
#endif
    }

    /**
     * @brief Read the injected clock or Arduino millis; return zero when neither exists.
     *
     * @return Current millisecond timestamp with uint32_t rollover, or zero without a clock.
     */
    UB_NODISCARD uint32_t time_now() const noexcept
    {
        if (time_fn_)
            return time_fn_();
#if UB_HAS_ARDUINO
        return static_cast<uint32_t>(millis());
#else
        return 0u;
#endif
    }
};
