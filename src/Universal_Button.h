/**
 * MIT License
 *
 * @brief Public umbrella header and convenience factories for Universal_Button.
 *
 * @file Universal_Button.h
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

#pragma once

#include "ButtonHandler_Config.h"
#include "ButtonHandler.h"
#include "ButtonTypes.h"

// ---- Version ---- //

#define UNIVERSAL_BUTTON_VERSION "2.0.0"
#define UNIVERSAL_BUTTON_VERSION_MAJOR 2
#define UNIVERSAL_BUTTON_VERSION_MINOR 0
#define UNIVERSAL_BUTTON_VERSION_PATCH 0

// ---- Config-driven alias ---- //

using Button = ButtonHandler<UB::config::NUM_BUTTONS>;

// ---- Native GPIO ---- //

/**
 * @brief Construct a handler from an explicit array of native GPIO pins.
 * @tparam N Number of buttons deduced from @p pins.
 * @param pins Pin array.
 * @param timing Global interaction timing.
 * @param skipPinInit True when pinMode is owned elsewhere.
 * @return Ready-to-use ButtonHandler<N>.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPins(const uint8_t (&pins)[N],
                                            ButtonTimingConfig timing = {},
                                            bool skipPinInit = false)
{
    return ButtonHandler<N>(pins, timing, skipPinInit);
}

/**
 * @brief Config-driven native GPIO factory using UB::config::BUTTON_PINS.
 */
inline Button makeButtons(ButtonTimingConfig timing = {}, bool skipPinInit = false)
{
    return Button(UB::config::BUTTON_PINS, timing, skipPinInit);
}

// ---- Logical pressed-state readers ---- //

/**
 * @brief Config-driven factory for a logical pressed-state callback.
 * @param read Reader returning true when the identified button is physically pressed.
 * @note v2 applies no polarity transform to this callback. Use an electrical reader factory
 *       when the callback returns HIGH/LOW instead of logical pressed state.
 */
inline Button makeButtonsWithReader(bool (*read)(uint8_t),
                                    ButtonTimingConfig timing = {},
                                    bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, read, timing, skipPinInit);
}

/** @brief Explicit-pins form of makeButtonsWithReader(). */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReader(const uint8_t (&pins)[N],
                                                     bool (*read)(uint8_t),
                                                     ButtonTimingConfig timing = {},
                                                     bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit);
}

/** @brief Config-driven context-aware logical pressed-state callback factory. */
inline Button makeButtonsWithReaderCtx(bool (*read)(void *, uint8_t),
                                       void *ctx,
                                       ButtonTimingConfig timing = {},
                                       bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, read, ctx, timing, skipPinInit);
}

/** @brief Explicit-pins context-aware logical pressed-state callback factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReaderCtx(const uint8_t (&pins)[N],
                                                        bool (*read)(void *, uint8_t),
                                                        void *ctx,
                                                        ButtonTimingConfig timing = {},
                                                        bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, ctx, timing, skipPinInit);
}

// ---- Validity-aware logical pressed-state readers ---- //

/** @brief Config-driven validity-aware logical pressed-state reader factory. */
inline Button makeButtonsWithResultReader(ButtonPressedResult (*read)(uint8_t),
                                          ButtonTimingConfig timing = {},
                                          bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, read, timing, skipPinInit);
}

/** @brief Explicit-pins validity-aware logical pressed-state reader factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndResultReader(const uint8_t (&pins)[N],
                                                           ButtonPressedResult (*read)(uint8_t),
                                                           ButtonTimingConfig timing = {},
                                                           bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit);
}

/** @brief Config-driven context-aware validity-aware logical pressed-state reader factory. */
inline Button makeButtonsWithResultReaderCtx(ButtonPressedResult (*read)(void *, uint8_t),
                                             void *ctx,
                                             ButtonTimingConfig timing = {},
                                             bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, read, ctx, timing, skipPinInit);
}

/** @brief Explicit-pins context-aware validity-aware logical pressed-state reader factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndResultReaderCtx(const uint8_t (&pins)[N],
                                                              ButtonPressedResult (*read)(void *, uint8_t),
                                                              void *ctx,
                                                              ButtonTimingConfig timing = {},
                                                              bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, read, ctx, timing, skipPinInit);
}

// ---- Electrical HIGH/LOW readers ---- //

/**
 * @brief Config-driven electrical-level reader factory.
 * @param read Reader returning true for HIGH and false for LOW.
 * @note ButtonPerConfig::active_low converts electrical level into logical pressed state.
 */
inline Button makeButtonsWithElectricalReader(bool (*read)(uint8_t),
                                              ButtonTimingConfig timing = {},
                                              bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit);
}

/** @brief Explicit-pins electrical-level reader factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalReader(const uint8_t (&pins)[N],
                                                               bool (*read)(uint8_t),
                                                               ButtonTimingConfig timing = {},
                                                               bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit);
}

/** @brief Config-driven context-aware electrical-level reader factory. */
inline Button makeButtonsWithElectricalReaderCtx(bool (*read)(void *, uint8_t),
                                                 void *ctx,
                                                 ButtonTimingConfig timing = {},
                                                 bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit);
}

/** @brief Explicit-pins context-aware electrical-level reader factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalReaderCtx(const uint8_t (&pins)[N],
                                                                  bool (*read)(void *, uint8_t),
                                                                  void *ctx,
                                                                  ButtonTimingConfig timing = {},
                                                                  bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit);
}

/** @brief Config-driven validity-aware electrical-level reader factory. */
inline Button makeButtonsWithElectricalResultReader(ButtonLevelResult (*read)(uint8_t),
                                                    ButtonTimingConfig timing = {},
                                                    bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit);
}

/** @brief Explicit-pins validity-aware electrical-level reader factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalResultReader(const uint8_t (&pins)[N],
                                                                     ButtonLevelResult (*read)(uint8_t),
                                                                     ButtonTimingConfig timing = {},
                                                                     bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit);
}

/** @brief Config-driven context-aware validity-aware electrical-level reader factory. */
inline Button makeButtonsWithElectricalResultReaderCtx(ButtonLevelResult (*read)(void *, uint8_t),
                                                       void *ctx,
                                                       ButtonTimingConfig timing = {},
                                                       bool skipPinInit = true)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit);
}

/** @brief Explicit-pins context-aware validity-aware electrical-level reader factory. */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalResultReaderCtx(const uint8_t (&pins)[N],
                                                                        ButtonLevelResult (*read)(void *, uint8_t),
                                                                        void *ctx,
                                                                        ButtonTimingConfig timing = {},
                                                                        bool skipPinInit = true)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit);
}

// ---- Time-source overloads retained from v1.x ---- //

/**
 * @brief Explicit-pins native GPIO factory using a custom millisecond time source.
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
 * @brief Config-driven native GPIO factory using a custom millisecond time source.
 */
inline Button makeButtons(ButtonTimingConfig timing, bool skipPinInit, Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven logical pressed-state reader factory using a custom time source.
 */
inline Button makeButtonsWithReader(bool (*read)(uint8_t),
                                    ButtonTimingConfig timing,
                                    bool skipPinInit,
                                    Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins logical pressed-state reader factory using a custom time source.
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
 * @brief Config-driven context logical pressed-state reader factory using a custom time source.
 */
inline Button makeButtonsWithReaderCtx(bool (*read)(void *, uint8_t),
                                       void *ctx,
                                       ButtonTimingConfig timing,
                                       bool skipPinInit,
                                       Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins context logical pressed-state reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndReaderCtx(const uint8_t (&pins)[N],
                                                        bool (*read)(void *, uint8_t),
                                                        void *ctx,
                                                        ButtonTimingConfig timing,
                                                        bool skipPinInit,
                                                        typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven validity-aware logical reader factory using a custom time source.
 */
inline Button makeButtonsWithResultReader(ButtonPressedResult (*read)(uint8_t),
                                          ButtonTimingConfig timing,
                                          bool skipPinInit,
                                          Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins validity-aware logical reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndResultReader(const uint8_t (&pins)[N],
                                                           ButtonPressedResult (*read)(uint8_t),
                                                           ButtonTimingConfig timing,
                                                           bool skipPinInit,
                                                           typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven context validity-aware logical reader factory using a custom time source.
 */
inline Button makeButtonsWithResultReaderCtx(ButtonPressedResult (*read)(void *, uint8_t),
                                             void *ctx,
                                             ButtonTimingConfig timing,
                                             bool skipPinInit,
                                             Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins context validity-aware logical reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndResultReaderCtx(const uint8_t (&pins)[N],
                                                              ButtonPressedResult (*read)(void *, uint8_t),
                                                              void *ctx,
                                                              ButtonTimingConfig timing,
                                                              bool skipPinInit,
                                                              typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven electrical-level reader factory using a custom time source.
 */
inline Button makeButtonsWithElectricalReader(bool (*read)(uint8_t),
                                              ButtonTimingConfig timing,
                                              bool skipPinInit,
                                              Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins electrical-level reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalReader(const uint8_t (&pins)[N],
                                                               bool (*read)(uint8_t),
                                                               ButtonTimingConfig timing,
                                                               bool skipPinInit,
                                                               typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven context electrical-level reader factory using a custom time source.
 */
inline Button makeButtonsWithElectricalReaderCtx(bool (*read)(void *, uint8_t),
                                                 void *ctx,
                                                 ButtonTimingConfig timing,
                                                 bool skipPinInit,
                                                 Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins context electrical-level reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalReaderCtx(const uint8_t (&pins)[N],
                                                                  bool (*read)(void *, uint8_t),
                                                                  void *ctx,
                                                                  ButtonTimingConfig timing,
                                                                  bool skipPinInit,
                                                                  typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven validity-aware electrical-level reader factory using a custom time source.
 */
inline Button makeButtonsWithElectricalResultReader(ButtonLevelResult (*read)(uint8_t),
                                                    ButtonTimingConfig timing,
                                                    bool skipPinInit,
                                                    Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins validity-aware electrical-level reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalResultReader(const uint8_t (&pins)[N],
                                                                     ButtonLevelResult (*read)(uint8_t),
                                                                     ButtonTimingConfig timing,
                                                                     bool skipPinInit,
                                                                     typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, timing, skipPinInit, timeFn);
}

/**
 * @brief Config-driven context validity-aware electrical-level reader factory using a custom time source.
 */
inline Button makeButtonsWithElectricalResultReaderCtx(ButtonLevelResult (*read)(void *, uint8_t),
                                                       void *ctx,
                                                       ButtonTimingConfig timing,
                                                       bool skipPinInit,
                                                       Button::TimeFn timeFn)
{
    return Button(UB::config::BUTTON_PINS, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit, timeFn);
}

/**
 * @brief Explicit-pins context validity-aware electrical-level reader factory using a custom time source.
 */
template <size_t N>
inline ButtonHandler<N> makeButtonsWithPinsAndElectricalResultReaderCtx(const uint8_t (&pins)[N],
                                                                        ButtonLevelResult (*read)(void *, uint8_t),
                                                                        void *ctx,
                                                                        ButtonTimingConfig timing,
                                                                        bool skipPinInit,
                                                                        typename ButtonHandler<N>::TimeFn timeFn)
{
    return ButtonHandler<N>(pins, BUTTON_ELECTRICAL_READER, read, ctx, timing, skipPinInit, timeFn);
}
