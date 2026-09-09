/**
 * MIT License
 *
 * @brief Public data types for Universal_Button.
 *
 * @file ButtonTypes.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include <stdint.h>

/**
 * @brief Completed interaction types exposed by the legacy press API.
 *
 * @note Long is emitted on release. Use ButtonEventType::LongStarted or
 *       isLongHeld() when an action must begin as soon as the threshold is met.
 */
enum class ButtonPressType : uint8_t
{
    None,  ///< No completed interaction is waiting.
    Short, ///< One short press after the double-click window expires.
    Long,  ///< Long press completed and released.
    Double ///< Two short presses completed inside the double-click window.
};

/**
 * @brief Detailed interaction events stored in the bounded event queue.
 */
enum class ButtonEventType : uint8_t
{
    Short,       ///< Finalized single short press.
    Double,      ///< Finalized double-click interaction.
    LongStarted, ///< Long threshold reached while the button remains held.
    LongReleased ///< Long interaction completed on release.
};

/**
 * @brief Latching behavior applied when a configured trigger event occurs.
 */
enum class LatchMode : uint8_t
{
    Toggle, ///< Flip latched state (OFF ↔ ON).
    Set,    ///< Force latched state ON.
    Reset   ///< Force latched state OFF.
};

/**
 * @brief Which completed interaction should drive latching.
 */
enum class LatchTrigger : uint8_t
{
    Short, ///< Trigger on ButtonPressType::Short.
    Long,  ///< Trigger on ButtonPressType::Long (release of a long press).
    Double ///< Trigger on ButtonPressType::Double.
};

/**
 * @brief Acquisition error reported by a validity-aware reader.
 */
enum class ButtonReadError : uint8_t
{
    None,             ///< Last acquisition completed successfully.
    MissingReader,    ///< No usable reader exists in the current build/configuration.
    AcquisitionFailed ///< The configured reader could not obtain a trustworthy state.
};

/**
 * @brief Result returned by a logical pressed-state reader.
 */
struct ButtonPressedResult
{
    /**
     * @brief Store a logical sample and its acquisition result; defaults to invalid.
     *
     * @param p Logical state: true means pressed.
     * @param v True when the sample value is trustworthy.
     * @param e Failure reason when v is false; stored as supplied.
     */
    constexpr ButtonPressedResult(bool p = false, bool v = false, ButtonReadError e = ButtonReadError::AcquisitionFailed) noexcept
        : pressed(p), valid(v), error(e) {}

    bool pressed{false};                                       ///< Logical state: true means physically pressed.
    bool valid{false};                                         ///< True when @ref pressed is trustworthy.
    ButtonReadError error{ButtonReadError::AcquisitionFailed}; ///< Failure reason when invalid.

    /**
     * @brief Construct a successful logical pressed-state result.
     *
     * @param value Logical pressed state.
     * @return Successful result.
     */
    static constexpr ButtonPressedResult success(bool value) noexcept
    {
        return ButtonPressedResult{value, true, ButtonReadError::None};
    }

    /**
     * @brief Construct a failed logical pressed-state result.
     *
     * @param why Failure reason.
     * @return Failed result.
     */
    static constexpr ButtonPressedResult failure(ButtonReadError why = ButtonReadError::AcquisitionFailed) noexcept
    {
        return ButtonPressedResult{false, false, why};
    }
};

/**
 * @brief Result returned by an electrical-level reader.
 */
struct ButtonLevelResult
{
    /**
     * @brief Store an electrical sample and its acquisition result; defaults to invalid.
     *
     * @param h Electrical state: true means HIGH.
     * @param v True when the sample value is trustworthy.
     * @param e Failure reason when v is false; stored as supplied.
     */
    constexpr ButtonLevelResult(bool h = false, bool v = false, ButtonReadError e = ButtonReadError::AcquisitionFailed) noexcept
        : high(h), valid(v), error(e) {}

    bool high{false};                                          ///< Electrical input level: true = HIGH, false = LOW.
    bool valid{false};                                         ///< True when @ref high is trustworthy.
    ButtonReadError error{ButtonReadError::AcquisitionFailed}; ///< Failure reason when invalid.

    /**
     * @brief Construct a successful electrical-level result.
     *
     * @param value Electrical level where true means HIGH.
     * @return Successful result.
     */
    static constexpr ButtonLevelResult success(bool value) noexcept
    {
        return ButtonLevelResult{value, true, ButtonReadError::None};
    }

    /**
     * @brief Construct a failed electrical-level result.
     *
     * @param why Failure reason.
     * @return Failed result.
     */
    static constexpr ButtonLevelResult failure(ButtonReadError why = ButtonReadError::AcquisitionFailed) noexcept
    {
        return ButtonLevelResult{false, false, why};
    }
};

/**
 * @brief Validation error for timing or per-button configuration.
 */
enum class ButtonConfigError : uint8_t
{
    None,                 ///< Configuration is valid.
    InvalidButton,        ///< Requested button index is outside the handler.
    InvalidDebounce,      ///< Debounce timing is not compatible with the resolved thresholds.
    InvalidShortPress,    ///< Short-press threshold is invalid.
    InvalidLongPress,     ///< Long-press threshold does not exceed the short threshold.
    InvalidDoubleClick,   ///< Double-click window is too short for the configured debounce.
    SynchronizationFailed ///< Valid configuration could not be synchronized to the input source.
};

/**
 * @brief Result returned by configuration-changing APIs.
 */
struct ButtonConfigResult
{
    /**
     * @brief Store the outcome of a configuration request.
     *
     * @param success True when the configuration request succeeded.
     * @param e Validation or synchronization failure reason.
     * @param id Affected button index, or 0xFF for a global result.
     */
    constexpr ButtonConfigResult(bool success = true, ButtonConfigError e = ButtonConfigError::None, uint8_t id = 0xFFu) noexcept
        : ok(success), error(e), button_id(id) {}

    bool ok{true};                                    ///< True when the requested configuration was applied.
    ButtonConfigError error{ButtonConfigError::None}; ///< Validation failure when @ref ok is false.
    uint8_t button_id{0xFFu};                         ///< Affected button, or 0xFF for global configuration.

    /**
     * @brief Allow concise `if (result)` checks.
     *
     * @return True when the configuration request succeeded.
     */
    constexpr explicit operator bool() const noexcept { return ok; }
};

/**
 * @brief Detailed event record stored in the bounded event queue.
 */
struct ButtonEvent
{
    uint8_t button_id{0};                         ///< Logical button index.
    ButtonEventType type{ButtonEventType::Short}; ///< Event kind.
    uint32_t timestamp_ms{0};                     ///< Timestamp when the event was finalized/emitted.
    uint32_t duration_ms{0};                      ///< Observed press duration where meaningful.
    uint32_t sequence{0};                         ///< Monotonic event sequence for loss detection.
};

/**
 * @brief Configuration for debounce and interaction timings.
 */
struct ButtonTimingConfig
{
    uint32_t debounce_ms;     ///< Minimum stable time before a raw level becomes debounced state.
    uint32_t short_press_ms;  ///< Minimum debounced hold time for a short interaction.
    uint32_t long_press_ms;   ///< Hold time at which LongStarted becomes true/emitted.
    uint32_t double_click_ms; ///< Maximum first-release → second-release interval for a double click.

    /**
     * @brief Set global timing thresholds in milliseconds; validation occurs in the handler.
     *
     * @param debounce Minimum stable level interval in milliseconds.
     * @param short_press Minimum short-press duration in milliseconds.
     * @param long_press Long-press threshold in milliseconds.
     * @param double_click Maximum interval between first and second short releases in milliseconds.
     */
    constexpr ButtonTimingConfig(uint32_t debounce = 30,
                                 uint32_t short_press = 200,
                                 uint32_t long_press = 1000,
                                 uint32_t double_click = 400) noexcept
        : debounce_ms(debounce),
          short_press_ms(short_press),
          long_press_ms(long_press),
          double_click_ms(double_click)
    {
    }
};

/**
 * @brief Optional per-button overrides and latching configuration.
 *
 * @details A zero timing field inherits the corresponding global timing.
 *          `active_low` is used only for native/electrical-level readers.
 *          Logical pressed-state readers already return the final pressed state.
 */
struct ButtonPerConfig
{
    uint16_t debounce_ms{0};     ///< 0 => inherit global debounce_ms.
    uint16_t short_press_ms{0};  ///< 0 => inherit global short_press_ms.
    uint16_t long_press_ms{0};   ///< 0 => inherit global long_press_ms.
    uint16_t double_click_ms{0}; ///< 0 => inherit global double_click_ms.
    bool active_low{true};       ///< Electrical readers: true = LOW means pressed.
    bool enabled{true};          ///< False excludes this button from acquisition/interaction processing.

    bool latch_enabled{false};                  ///< Maintain a latched state for this button.
    LatchMode latch_mode{LatchMode::Toggle};    ///< Toggle / Set / Reset when the trigger fires.
    LatchTrigger latch_on{LatchTrigger::Short}; ///< Completed interaction that drives latching.
    bool latch_initial{false};                  ///< Latched state restored by resetAndSync().
};

/**
 * @brief Per-button acquisition and state metadata.
 */
struct ButtonInputStatus
{
    bool enabled{false};                                        ///< Current runtime enable state.
    bool valid{false};                                          ///< Most recent acquisition is trustworthy.
    bool has_sample{false};                                     ///< At least one successful acquisition has occurred.
    bool pressed{false};                                        ///< Current debounced logical level.
    bool long_held{false};                                      ///< Long threshold has been reached while still held.
    ButtonReadError last_error{ButtonReadError::MissingReader}; ///< Most recent acquisition error.
    uint32_t sample_ms{0};                                      ///< Timestamp of the most recent successful acquisition.
    uint32_t attempt_ms{0};                                     ///< Timestamp of the most recent acquisition attempt.
    uint32_t error_ms{0};                                       ///< Timestamp of the most recent failed acquisition.
    uint32_t sample_sequence{0};                                ///< Successful acquisition counter.
    uint32_t change_sequence{0};                                ///< Debounced level-transition counter.
    uint32_t generation{0};                                     ///< Edge-free synchronization generation.
    uint32_t latch_change_sequence{0};                          ///< Runtime latch transitions modulo 2^32, including disable/reset; never cleared by reset.
};

/**
 * @brief Handler-wide health/event metadata.
 */
struct ButtonHandlerStatus
{
    bool configured{false};                                  ///< A usable reader and valid timing configuration are present.
    bool valid{false};                                       ///< Every enabled button has a valid current acquisition.
    bool has_sample{false};                                  ///< Every enabled button has at least one successful acquisition.
    bool event_overflow{false};                              ///< One or more detailed events could not be queued.
    ButtonConfigError config_error{ButtonConfigError::None}; ///< Constructor/global timing validation state.
    uint32_t event_sequence{0};                              ///< Total detailed events generated, including dropped events.
    uint32_t dropped_events{0};                              ///< Number of events rejected because the queue was full.
};
