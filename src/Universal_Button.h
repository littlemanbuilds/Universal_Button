#pragma once

#include <Arduino.h>
#include <ButtonHandler_Config.h>
#include <ButtonHandler.h>

// ---- Version macro ---- //

#define UNIVERSAL_BUTTON_VERSION "1.6.1"

// ---- Aliases ---- //

using Button = ButtonHandler<NUM_BUTTONS>;

// ---- Native GPIO readers (active-low with INPUT_PULLUP) ---- //

/**
 * @brief Zero-boilerplate factory with an explicit pins array.
 *
 * Initializes pins to INPUT_PULLUP by default and treats digitalRead(pin)==LOW
 * as "pressed" (active-low). Set skipPinInit=true if pins are configured elsewhere.
 *
 * @tparam N Number of buttons (deduced from pins).
 * @param pins Reference to an array of length N with pin IDs.
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, GPIO mode is NOT configured in this factory.
 * @return A ready-to-use ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N],
                                            ButtonTimingConfig timing = {},
                                            bool skipPinInit = false)
{
    return ButtonHandler<N>(pins, timing, skipPinInit);
}

/**
 * @brief Config-driven factory (uses BUTTON_PINS / NUM_BUTTONS).
 *
 * Initializes pins to INPUT_PULLUP by default and treats digitalRead(pin)==LOW
 * as "pressed" (active-low). Set skipPinInit=true if pins are configured elsewhere.
 *
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, GPIO mode is NOT configured in this factory.
 * @return A ready-to-use Button.
 */
inline Button makeButtons(ButtonTimingConfig timing = {}, bool skipPinInit = false)
{
    return Button(BUTTON_PINS, timing, skipPinInit);
}

// ---- External readers (function-pointer fast path) ---- //

/**
 * @brief Config-driven factory with an external function-pointer reader.
 *
 * Use when button states come from a source other than native GPIO (e.g., a
 * port expander). The read function must return true when the identified
 * key is currently pressed (already polarity-corrected for your hardware).
 *
 * @param read Reader: bool(uint8_t id) → pressed?
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for BUTTON_PINS.
 * @return A Button sized by NUM_BUTTONS.
 */
inline Button makeButtonsWithReader(bool (*read)(uint8_t),
                                    ButtonTimingConfig timing = {},
                                    bool skipPinInit = true)
{
    return Button(BUTTON_PINS, read, timing, skipPinInit);
}

/**
 * @brief Explicit-pins factory with an external function-pointer reader.
 *
 * Same as makeButtonsWithReader, but you provide the pins array explicitly
 * instead of using the config mapping.
 *
 * @tparam N Number of buttons (deduced from pins).
 * @param pins Reference to an array of length N with key IDs.
 * @param read Reader: bool(uint8_t id) → pressed?
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for pins.
 * @return A ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                     bool (*read)(uint8_t),
                                                     ButtonTimingConfig timing = {},
                                                     bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit);
}

// ---- External readers (context-aware callback) ---- //

/**
 * @brief Config-driven factory with a context-aware reader callback.
 *
 * Useful for devices that require a handle/context (e.g., I²C expanders,
 * SPI shift registers, or driver classes). The read function must return
 * true when the identified key is currently pressed (already polarity-
 * corrected for your hardware).
 *
 * @param read Reader: bool(void* ctx, uint8_t id) → pressed?
 * @param ctx Opaque pointer passed back to read on each call.
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for BUTTON_PINS.
 * @return A Button sized by NUM_BUTTONS.
 */
inline Button makeButtonsWithReaderCtx(bool (*read)(void *, uint8_t), void *ctx,
                                       ButtonTimingConfig timing = {},
                                       bool skipPinInit = true)
{
    return Button(BUTTON_PINS, read, ctx, timing, skipPinInit);
}

/**
 * @brief Explicit-pins factory with a context-aware reader callback.
 *
 * Same as makeButtonsWithReaderCtx, but you provide the pins array explicitly
 * instead of using the config mapping.
 *
 * @tparam N Number of buttons (deduced from pins).
 * @param pins Reference to an array of length N with key IDs.
 * @param read Reader: bool(void* ctx, uint8_t id) → pressed?
 * @param ctx Opaque pointer passed back to read on each call.
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for pins.
 * @return A ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReaderCtx(const uint8_t (&pins)[N],
                                                        bool (*read)(void *, uint8_t), void *ctx,
                                                        ButtonTimingConfig timing = {},
                                                        bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, ctx, timing, skipPinInit);
}

// ---- Arduino-free overloads ---- //

/**
 * @brief Zero-boilerplate factory with explicit pins and optional time source.
 *
 * @tparam N Number of buttons (deduced from pins).
 * @param pins Reference to an array of length N with pin IDs.
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, GPIO mode is NOT configured in this factory.
 * @param timeFn Optional time source (ms). nullptr → use millis().
 * @return A ready-to-use ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N],
                                            ButtonTimingConfig timing,
                                            bool skipPinInit,
                                            typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven factory with optional time source (uses BUTTON_PINS).
 *
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, GPIO mode is NOT configured in this factory.
 * @param timeFn Optional time source (ms). nullptr → use millis().
 * @return A ready-to-use Button.
 */
inline Button makeButtons(ButtonTimingConfig timing, bool skipPinInit, Button::TimeFn timeFn)
{
    return Button(BUTTON_PINS, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven factory with external fast reader and optional time source.
 *
 * @param read Reader: bool(uint8_t id) → pressed?
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for BUTTON_PINS.
 * @param timeFn Optional time source (ms). nullptr → use millis().
 * @return A Button sized by NUM_BUTTONS.
 */
inline Button makeButtonsWithReader(bool (*read)(uint8_t),
                                    ButtonTimingConfig timing,
                                    bool skipPinInit,
                                    Button::TimeFn timeFn)
{
    return Button(BUTTON_PINS, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins factory with external fast reader and optional time source.
 *
 * @tparam N Number of buttons (deduced from pins).
 * @param pins Reference to an array of length N with key IDs.
 * @param read Reader: bool(uint8_t id) → pressed?
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for pins.
 * @param timeFn Optional time source (ms). nullptr → use millis().
 * @return A ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                     bool (*read)(uint8_t),
                                                     ButtonTimingConfig timing,
                                                     bool skipPinInit,
                                                     typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven factory with context-aware reader and optional time source.
 *
 * @param read Reader: bool(void* ctx, uint8_t id) → pressed?
 * @param ctx Opaque pointer passed back to read on each call.
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for BUTTON_PINS.
 * @param timeFn Optional time source (ms). nullptr → use millis().
 * @return A Button sized by NUM_BUTTONS.
 */
inline Button makeButtonsWithReaderCtx(bool (*read)(void *, uint8_t), void *ctx,
                                       ButtonTimingConfig timing,
                                       bool skipPinInit,
                                       Button::TimeFn timeFn)
{
    return Button(BUTTON_PINS, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins factory with context-aware reader and optional time source.
 *
 * @tparam N Number of buttons (deduced from pins).
 * @param pins Reference to an array of length N with key IDs.
 * @param read Reader: bool(void* ctx, uint8_t id) → pressed?
 * @param ctx Opaque pointer passed back to read on each call.
 * @param timing Global debounce/press-duration configuration.
 * @param skipPinInit If true, pinMode is not called for pins.
 * @param timeFn Optional time source (ms). nullptr → use millis().
 * @return A ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReaderCtx(const uint8_t (&pins)[N],
                                                        bool (*read)(void *, uint8_t), void *ctx,
                                                        ButtonTimingConfig timing,
                                                        bool skipPinInit,
                                                        typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, read, ctx, timing, skipPinInit, timeFn);
}
