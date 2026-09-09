/**
 * MIT License
 *
 * @brief Native contract tests for Universal_Button v2.
 *
 * @file test_button_handler.cpp
 * @author Little Man Builds (Darren Osborne)
 * @date 2026-08-07
 * @copyright Copyright © 2026 Little Man Builds
 */

#include <ButtonHandler.h>

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{
    unsigned g_assertions = 0u;
    unsigned g_tests = 0u;

#define CHECK(expr)                                                                                 \
    do                                                                                              \
    {                                                                                               \
        ++g_assertions;                                                                             \
        if (!(expr))                                                                                \
        {                                                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                  \
            std::exit(1);                                                                           \
        }                                                                                           \
    } while (false)

    void beginTest(const char *name)
    {
        ++g_tests;
        std::printf("[TEST %u] %s\n", g_tests, name);
    }

    struct Source
    {
        bool state[8];
        bool valid[8];

        Source() : state{false, false, false, false, false, false, false, false},
                   valid{true, true, true, true, true, true, true, true}
        {
        }
    };

    bool logicalCtx(void *ctx, uint8_t id)
    {
        Source *s = static_cast<Source *>(ctx);
        return s->state[id];
    }

    bool electricalCtx(void *ctx, uint8_t id)
    {
        Source *s = static_cast<Source *>(ctx);
        return s->state[id]; // true = HIGH
    }

    ButtonPressedResult checkedLogicalCtx(void *ctx, uint8_t id)
    {
        Source *s = static_cast<Source *>(ctx);
        return s->valid[id] ? ButtonPressedResult::success(s->state[id])
                            : ButtonPressedResult::failure(ButtonReadError::AcquisitionFailed);
    }

    ButtonLevelResult checkedElectricalCtx(void *ctx, uint8_t id)
    {
        Source *s = static_cast<Source *>(ctx);
        return s->valid[id] ? ButtonLevelResult::success(s->state[id])
                            : ButtonLevelResult::failure(ButtonReadError::AcquisitionFailed);
    }

    bool g_counting_level_high = true;
    unsigned g_counting_level_calls = 0u;

    bool countingElectricalPin(uint8_t id)
    {
        (void)id;
        ++g_counting_level_calls;
        return g_counting_level_high;
    }

    const ButtonTimingConfig kTiming{10u, 20u, 50u, 40u};

    template <size_t N>
    void update(ButtonHandler<N> &b, uint32_t t)
    {
        b.update(t);
    }

    void testLogicalCallbackContract()
    {
        beginTest("logical callback is already pressed-state (no polarity transform)");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        CHECK(!b.isPressed(0u));

        s.state[0] = true;
        update(b, 1u);
        update(b, 11u);
        CHECK(b.isPressed(0u));
        const uint32_t generation_before = b.generation(0u);

        // active_low belongs only to electrical readers; changing it must not invert logical callbacks.
        CHECK(b.setActiveLow(0u, false));
        CHECK(b.isPressed(0u));
        CHECK(b.generation(0u) == generation_before);
        CHECK(b.pendingEventCount() == 0u);
    }

    void testElectricalPolarityContract()
    {
        beginTest("electrical callbacks apply active-low/high polarity exactly once");
        Source s;
        const uint8_t pins[1] = {0u};
        s.state[0] = true; // HIGH => released for active-low
        ButtonHandler<1> b(pins, BUTTON_ELECTRICAL_READER, &electricalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        CHECK(!b.isPressed(0u));

        s.state[0] = false; // LOW => pressed
        b.update(1u);
        b.update(11u);
        CHECK(b.isPressed(0u));

        s.state[0] = true;
        CHECK(b.setActiveLow(0u, false)); // HIGH => pressed, synchronized edge-free
        CHECK(b.isPressed(0u));
        CHECK(b.pendingEventCount() == 0u);
    }

    void testElectricalPinReaderSamplesOnce()
    {
        beginTest("electrical pin callback is sampled once per acquisition");
        const uint8_t pins[1] = {0u};
        g_counting_level_high = true;
        g_counting_level_calls = 0u;
        ButtonHandler<1> b(pins, BUTTON_ELECTRICAL_READER, &countingElectricalPin, kTiming, true, nullptr);

        CHECK(b.sync(0u));
        CHECK(g_counting_level_calls == 1u);

        g_counting_level_high = false;
        b.update(1u);
        CHECK(g_counting_level_calls == 2u);
        b.update(11u);
        CHECK(g_counting_level_calls == 3u);
        CHECK(b.isPressed(0u));
    }

    void testCheckedReaderFailureAndRecovery()
    {
        beginTest("reader failure is invalid, retains level, and recovery is edge-free");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &checkedLogicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        s.state[0] = true;
        b.update(1u);
        b.update(11u);
        CHECK(b.isPressed(0u));
        CHECK(b.valid(0u));

        s.valid[0] = false;
        b.update(20u);
        CHECK(!b.valid(0u));
        CHECK(b.isPressed(0u));
        CHECK(b.inputStatus(0u).last_error == ButtonReadError::AcquisitionFailed);

        // Hardware changed during the unobserved gap. Recovery must baseline without release event.
        s.state[0] = false;
        s.valid[0] = true;
        b.update(100u);
        CHECK(b.valid(0u));
        CHECK(!b.isPressed(0u));
        CHECK(b.pendingEventCount() == 0u);
        CHECK(b.generation(0u) >= 2u); // initial sync + recovery sync
    }

    void testCheckedElectricalFailure()
    {
        beginTest("validity-aware electrical reader distinguishes LOW from read failure");
        Source s;
        const uint8_t pins[1] = {0u};
        s.state[0] = true;
        ButtonHandler<1> b(pins, BUTTON_ELECTRICAL_READER, &checkedElectricalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        s.valid[0] = false;
        b.update(5u);
        CHECK(!b.valid());
        CHECK(!b.isPressed(0u));
        CHECK(b.inputStatus(0u).error_ms == 5u);
    }

    void testMissingReaderFailsClosed()
    {
        beginTest("native reader is explicitly unavailable in non-Arduino host build");
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, kTiming, true, nullptr);
        CHECK(!b.configured());
        CHECK(!b.sync(7u));
        CHECK(!b.valid());
        CHECK(!b.isPressed(0u));
        CHECK(b.inputStatus(0u).last_error == ButtonReadError::MissingReader);
    }

    void testDebounceAndBounce()
    {
        beginTest("bounce does not commit until continuously stable");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        s.state[0] = true;
        b.update(1u);
        s.state[0] = false;
        b.update(5u);
        s.state[0] = true;
        b.update(9u);
        b.update(18u);
        CHECK(!b.isPressed(0u));
        b.update(19u);
        CHECK(b.isPressed(0u));
        CHECK(b.changeSequence(0u) == 1u);
    }

    void testShortPress()
    {
        beginTest("short interaction is delayed for double-click disambiguation");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        s.state[0] = true;
        b.update(1u);
        b.update(11u); // press commit
        s.state[0] = false;
        b.update(31u);
        b.update(41u); // release commit, 30 ms
        CHECK(b.peekPressType(0u) == ButtonPressType::None);
        b.update(82u);
        CHECK(b.peekPressType(0u) == ButtonPressType::Short);
        CHECK(b.getPressType(0u) == ButtonPressType::Short);
        CHECK(b.getPressType(0u) == ButtonPressType::None);
        CHECK(b.getLastPressDuration(0u) == 30u);
    }

    void testDoublePress()
    {
        beginTest("double-click preserves two short releases as one Double event");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        s.state[0] = true;  b.update(1u);  b.update(11u);
        s.state[0] = false; b.update(31u); b.update(41u);
        s.state[0] = true;  b.update(45u); b.update(55u);
        s.state[0] = false; b.update(65u); b.update(75u);
        CHECK(b.getPressType(0u) == ButtonPressType::Double);
        CHECK(b.pendingEventCount() == 0u);
    }

    void testLongStartedAndReleased()
    {
        beginTest("long threshold and release are distinct while held level remains queryable");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        s.state[0] = true;
        b.update(1u);
        b.update(11u);
        b.update(61u);
        CHECK(b.isPressed(0u));
        CHECK(b.isLongHeld(0u));
        CHECK(b.peekPressType(0u) == ButtonPressType::None);

        ButtonEvent evt{};
        CHECK(b.peekEvent(evt));
        CHECK(evt.type == ButtonEventType::LongStarted);
        CHECK(evt.duration_ms == 50u);

        s.state[0] = false;
        b.update(70u);
        b.update(80u);
        CHECK(!b.isLongHeld(0u));
        CHECK(b.getPressType(0u) == ButtonPressType::Long);
    }

    void testSynchronizedHeldLevelDoesNotCreateInteraction()
    {
        beginTest("sync/reset of a held button requires release before event classification resumes");
        Source s;
        const uint8_t pins[1] = {0u};
        s.state[0] = true;
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        CHECK(b.isPressed(0u));
        b.update(100u);
        CHECK(!b.isLongHeld(0u));

        s.state[0] = false;
        b.update(101u);
        b.update(111u);
        b.update(200u);
        CHECK(b.pendingEventCount() == 0u);
        CHECK(b.getPressType(0u) == ButtonPressType::None);
    }

    void testReaderReconfigurationIsSynchronized()
    {
        beginTest("reader replacement cannot synthesize a press/release edge");
        Source a;
        Source bsrc;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &a, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        bsrc.state[0] = true;
        CHECK(b.setReadFn(&logicalCtx, &bsrc));
        CHECK(b.isPressed(0u));
        CHECK(b.pendingEventCount() == 0u);
        CHECK(b.changeSequence(0u) == 0u);
    }

    void testFailedReaderReconfigurationRollsBack()
    {
        beginTest("failed reader replacement rolls back to prior source");
        Source a;
        Source bad;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &checkedLogicalCtx, &a, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        bad.valid[0] = false;
        CHECK(!b.setReadResultFn(&checkedLogicalCtx, &bad));
        CHECK(b.configured());
        CHECK(b.valid());
    }

    void testTimingValidation()
    {
        beginTest("timing configuration is validated before activation");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        ButtonConfigResult r = b.setGlobalTiming(ButtonTimingConfig{30u, 20u, 50u, 40u});
        CHECK(!r);
        CHECK(r.error == ButtonConfigError::InvalidDebounce);

        r = b.setGlobalTiming(ButtonTimingConfig{10u, 20u, 20u, 40u});
        CHECK(!r);
        CHECK(r.error == ButtonConfigError::InvalidLongPress);

        r = b.setGlobalTiming(ButtonTimingConfig{10u, 20u, 50u, 5u});
        CHECK(!r);
        CHECK(r.error == ButtonConfigError::InvalidDoubleClick);

        r = b.setGlobalTiming(ButtonTimingConfig{10u, 25u, 60u, 50u});
        CHECK(r);
        CHECK(b.configured());
    }

    void testFailedGlobalTimingSyncKeepsPriorConfig()
    {
        beginTest("global timing change rolls back when synchronization fails");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &checkedLogicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        s.valid[0] = false;
        const ButtonConfigResult r = b.setGlobalTiming(ButtonTimingConfig{5u, 10u, 30u, 10u});
        CHECK(!r);
        CHECK(r.error == ButtonConfigError::SynchronizationFailed);
        CHECK(b.configured());

        s.valid[0] = true;
        CHECK(b.sync(100u));
        s.state[0] = true;
        b.update(101u);
        b.update(111u);
        s.state[0] = false;
        b.update(116u);
        b.update(126u);
        b.update(170u);
        CHECK(b.getPressType(0u) == ButtonPressType::None);
        CHECK(b.getLastPressDuration(0u) == 15u);
    }

    void testPerButtonTimingValidation()
    {
        beginTest("per-button overrides validate resolved timing without corrupting prior config");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        ButtonPerConfig pc{};
        pc.short_press_ms = 60u;
        CHECK(!b.setPerConfig(0u, pc));
        CHECK(b.valid());
        CHECK(!b.setPerConfig(4u, pc));
    }

    void testEventQueuePreservesMultipleInteractions()
    {
        beginTest("multiple completed interactions survive delayed consumption in order");
        Source s;
        const uint8_t pins[1] = {0u};
        const ButtonTimingConfig t{0u, 1u, 2u, 0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, t, true, nullptr);
        CHECK(b.sync(0u));

        // Two long presses => LongStarted + LongReleased twice, with no consumer reads in between.
        for (uint32_t base = 1u; base <= 10u; base += 5u)
        {
            s.state[0] = true;  b.update(base);
            b.update(base + 2u);
            s.state[0] = false; b.update(base + 3u);
        }
        CHECK(b.pendingEventCount() == 4u);
        ButtonEvent e{};
        CHECK(b.popEvent(e)); CHECK(e.type == ButtonEventType::LongStarted);
        CHECK(b.popEvent(e)); CHECK(e.type == ButtonEventType::LongReleased);
        CHECK(b.popEvent(e)); CHECK(e.type == ButtonEventType::LongStarted);
        CHECK(b.popEvent(e)); CHECK(e.type == ButtonEventType::LongReleased);
        CHECK(!b.popEvent(e));
    }

    void testEventOverflowIsExplicit()
    {
        beginTest("bounded event queue reports overflow and dropped count");
        Source s;
        const uint8_t pins[1] = {0u};
        const ButtonTimingConfig t{0u, 1u, 2u, 0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, t, true, nullptr);
        CHECK(b.sync(0u));

        uint32_t now = 1u;
        for (unsigned press = 0u; press < 9u; ++press)
        {
            s.state[0] = true;  b.update(now++);
            b.update(now + 1u); now += 2u;
            s.state[0] = false; b.update(now++);
        }
        CHECK(b.pendingEventCount() == static_cast<uint8_t>(UB_EVENT_QUEUE_CAPACITY));
        CHECK(b.eventOverflowed());
        CHECK(b.droppedEventCount() == 2u);
        CHECK(b.eventSequence() == 18u);
        b.clearEventOverflow();
        CHECK(!b.eventOverflowed());
        CHECK(b.droppedEventCount() == 2u);
    }

    void testLegacyApiDrainsLongStarted()
    {
        beginTest("legacy getPressType ignores LongStarted but returns LongReleased");
        Source s;
        const uint8_t pins[1] = {0u};
        const ButtonTimingConfig t{0u, 1u, 2u, 0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, t, true, nullptr);
        CHECK(b.sync(0u));
        s.state[0] = true; b.update(1u); b.update(3u);
        CHECK(b.getPressType(0u) == ButtonPressType::None); // consumes LongStarted only
        s.state[0] = false; b.update(4u);
        CHECK(b.getPressType(0u) == ButtonPressType::Long);
        CHECK(b.pendingEventCount() == 0u);
    }

    void testLatchingAndDurableLatchSequence()
    {
        beginTest("latching follows finalized interactions and exposes durable change sequence");
        Source s;
        const uint8_t pins[1] = {0u};
        const ButtonTimingConfig t{0u, 1u, 5u, 1u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, t, true, nullptr);
        ButtonPerConfig pc{};
        pc.latch_enabled = true;
        pc.latch_mode = LatchMode::Toggle;
        pc.latch_on = LatchTrigger::Short;
        CHECK(b.setPerConfig(0u, pc));
        CHECK(b.sync(0u));

        s.state[0] = true; b.update(1u);
        s.state[0] = false; b.update(2u);
        b.update(4u);
        CHECK(b.isLatched(0u));
        CHECK(b.latchChangeSequence(0u) == 1u);
        CHECK(b.getAndClearLatchedChanged(0u));
        CHECK(!b.getAndClearLatchedChanged(0u));
        CHECK(b.inputStatus(0u).latch_change_sequence == 1u);
        CHECK(b.pendingEventCount() == 1u);
        const uint32_t events = b.eventSequence();
        CHECK(b.resetAndSync());
        CHECK(!b.isLatched(0u));
        CHECK(b.latchChangeSequence(0u) == 2u);
        CHECK(b.inputStatus(0u).latch_change_sequence == 2u);
        CHECK(!b.getAndClearLatchedChanged(0u));
        CHECK(b.pendingEventCount() == 0u);
        CHECK(b.eventSequence() == events);
    }

    template <size_t N>
    void checkLatch(const ButtonHandler<N> &b, uint8_t id, bool value, uint32_t sequence)
    {
        CHECK(b.isLatched(id) == value);
        CHECK(b.latchChangeSequence(id) == sequence);
        CHECK(b.inputStatus(id).latch_change_sequence == sequence);
    }

    void testLatchDisableLifecycle()
    {
        beginTest("disable paths count actual latch transitions and preserve queued interactions");
        for (int configDisable = 0; configDisable < 2; ++configDisable)
        {
            Source s;
            const uint8_t pins[] = {0u, 1u};
            const ButtonTimingConfig timing{0u, 1u, 5u, 1u};
            ButtonHandler<2> b(pins, &checkedLogicalCtx, &s, timing);
            CHECK(b.sync(0u));
            checkLatch(b, 0u, false, 0u);
            checkLatch(b, 1u, false, 0u);
            s.state[0] = true; b.update(1u);
            s.state[0] = false; b.update(2u);
            b.update(4u);
            CHECK(b.pendingEventCount() == 1u);
            const uint32_t events = b.eventSequence();
            const uint32_t generation = b.generation(0u);
            b.setLatched(0u, true);
            b.setLatched(1u, true);
            CHECK(b.getAndClearLatchedChanged(1u));
            ButtonPerConfig disabled{};
            disabled.enabled = false;
            for (int repeat = 0; repeat < 2; ++repeat)
            {
                CHECK(configDisable ? b.setPerConfig(0u, disabled) : b.enable(0u, false));
                checkLatch(b, 0u, false, 2u);
                checkLatch(b, 1u, true, 1u);
                CHECK(!b.getAndClearLatchedChanged(0u));
                CHECK(!b.getAndClearLatchedChanged(1u));
                CHECK(b.generation(0u) == generation);
                CHECK(b.eventSequence() == events);
                CHECK(b.pendingEventCount() == 1u);
            }
            ButtonEvent event{};
            CHECK(b.popEvent(event));
            CHECK(event.button_id == 0u && event.type == ButtonEventType::Short);
            CHECK(event.sequence == events);
            CHECK(b.enable(0u, true));
            CHECK(b.enable(0u, false)); // An already false latch does not change.
            checkLatch(b, 0u, false, 2u);

            // Manual latch control is allowed while disabled. Failed re-enable
            // clears that latch even though no new input baseline was acquired.
            b.setLatched(0u, true);
            s.valid[0] = false;
            CHECK(!b.enable(0u, true));
            checkLatch(b, 0u, false, 4u);
            CHECK(!b.inputStatus(0u).enabled);
            CHECK(!b.getAndClearLatchedChanged(0u));
            CHECK(!b.enable(0u, true));
            checkLatch(b, 0u, false, 4u);
            CHECK(b.pendingEventCount() == 0u);
            CHECK(b.eventSequence() == events);
        }
    }

    void testLatchResetLifecycle()
    {
        beginTest("reset restores either initial latch once, even when synchronization fails");
        for (int initial = 0; initial < 2; ++initial)
        for (int succeeds = 0; succeeds < 2; ++succeeds)
        for (int voidReset = 0; voidReset < 2; ++voidReset)
        {
            Source s;
            const uint8_t pins[] = {0u, 1u};
            ButtonHandler<2> b(pins, &checkedLogicalCtx, &s, kTiming);
            checkLatch(b, 0u, false, 0u); // Construction has no prior transition.
            CHECK(!b.getAndClearLatchedChanged(0u));
            ButtonPerConfig config{};
            config.latch_initial = initial != 0;
            CHECK(b.setPerConfig(0u, config));
            checkLatch(b, 0u, false, 0u); // Configuration alone does not restore it.
            b.setLatched(0u, initial == 0);
            const uint32_t before = b.latchChangeSequence(0u);
            s.valid[0] = succeeds != 0;
            for (int repeat = 0; repeat < 2; ++repeat)
            {
                if (voidReset)
                    b.reset();
                else
                    CHECK(b.resetAndSync() == (succeeds != 0));
                checkLatch(b, 0u, initial != 0, before + 1u);
                checkLatch(b, 1u, false, 0u);
                CHECK(!b.getAndClearLatchedChanged(0u));
                CHECK(b.valid(0u) == (succeeds != 0));
                CHECK(b.valid(1u) == (succeeds != 0));
                CHECK(b.pendingEventCount() == 0u);
                CHECK(b.eventSequence() == 0u);
            }
        }
    }

    void testManualLatchAccounting()
    {
        beginTest("manual latch operations keep durable counters independent of consumable flags");
        Source s;
        const uint8_t pins[] = {0u, 1u};
        ButtonHandler<2> b(pins, &checkedLogicalCtx, &s, kTiming);
        b.setLatched(0u, true);
        b.setLatched(0u, true);
        b.setLatched(1u, true);
        checkLatch(b, 0u, true, 1u);
        CHECK(b.getAndClearLatchedChanged(0u));
        CHECK(!b.getAndClearLatchedChanged(0u));
        checkLatch(b, 0u, true, 1u);
        b.clearLatchedMask(1u);
        b.clearLatchedMask(1u);
        checkLatch(b, 0u, false, 2u);
        checkLatch(b, 1u, true, 1u);
        b.clearAllLatched();
        b.clearAllLatched();
        checkLatch(b, 0u, false, 2u);
        checkLatch(b, 1u, false, 2u);
        CHECK(b.getAndClearLatchedChanged(0u));
        CHECK(b.getAndClearLatchedChanged(1u));
        b.setLatched(255u, true);
        CHECK(!b.enable(255u, false));
        CHECK(!b.setPerConfig(255u, ButtonPerConfig{}));
        checkLatch(b, 255u, false, 0u);
        checkLatch(b, 0u, false, 2u);
        checkLatch(b, 1u, false, 2u);
        CHECK(!b.getAndClearLatchedChanged(255u));
        CHECK(b.pendingEventCount() == 0u);
        CHECK(b.eventSequence() == 0u);
    }

    void testResetAndSyncPreservesHeldLevelWithoutEvent()
    {
        beginTest("resetAndSync reads the current level instead of assuming released");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        s.state[0] = true;
        b.update(1u); b.update(11u);
        CHECK(b.isPressed(0u));
        CHECK(b.resetAndSync());
        CHECK(b.isPressed(0u));
        CHECK(b.pendingEventCount() == 0u);
        b.update(1000u);
        CHECK(!b.isLongHeld(0u));
    }

    void testMillisWraparound()
    {
        beginTest("debounce and duration arithmetic remain correct across uint32_t wrap");
        Source s;
        const uint8_t pins[1] = {0u};
        const ButtonTimingConfig t{5u, 10u, 30u, 20u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, t, true, nullptr);
        const uint32_t near = std::numeric_limits<uint32_t>::max() - 8u;
        CHECK(b.sync(near));
        s.state[0] = true;
        b.update(near + 1u);
        b.update(near + 6u); // press commit before wrap
        s.state[0] = false;
        b.update(4u);        // candidate after wrap
        b.update(9u);        // release commit, duration 12 ms
        b.update(30u);       // flush short
        CHECK(b.getPressType(0u) == ButtonPressType::Short);
        CHECK(b.getLastPressDuration(0u) == 12u);
    }

    void testMultipleButtonsIndependentTiming()
    {
        beginTest("multiple buttons retain independent debounce/interaction timing");
        Source s;
        const uint8_t pins[2] = {0u, 1u};
        ButtonHandler<2> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));

        ButtonPerConfig pc{};
        pc.debounce_ms = 20u;
        pc.short_press_ms = 30u;
        pc.long_press_ms = 80u;
        pc.double_click_ms = 50u;
        CHECK(b.setPerConfig(1u, pc));

        s.state[0] = true;
        s.state[1] = true;
        b.update(1u);
        b.update(11u);
        CHECK(b.isPressed(0u));
        CHECK(!b.isPressed(1u));
        b.update(21u);
        CHECK(b.isPressed(1u));
    }

    void testStatusSequencesAndFreshness()
    {
        beginTest("sample freshness and state-change sequencing are distinct");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(10u));
        const uint32_t g = b.generation(0u);
        const uint32_t seq = b.sequence(0u);
        b.update(20u);
        CHECK(b.sequence(0u) == seq + 1u);
        CHECK(b.changeSequence(0u) == 0u);
        CHECK(b.sampleMs(0u) == 20u);
        CHECK(b.generation(0u) == g);
        CHECK(b.status().valid);
        CHECK(b.status().has_sample);
    }

    void testBoundsFailClosed()
    {
        beginTest("malformed indices fail closed without altering state");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        CHECK(!b.isPressed(9u));
        CHECK(!b.isLongHeld(9u));
        CHECK(b.getPressType(9u) == ButtonPressType::None);
        CHECK(b.peekPressType(9u) == ButtonPressType::None);
        CHECK(b.getLastPressDuration(9u) == 0u);
        CHECK(!b.setActiveLow(9u, false));
        CHECK(!b.enable(9u, false));
    }

    void testContextCallbackShape()
    {
        beginTest("Context callback returning asserted state works with default per config");
        Source mcp;
        const uint8_t pins[4] = {0u, 1u, 2u, 3u};
        ButtonHandler<4> buttons(pins, &logicalCtx, &mcp,
                                 ButtonTimingConfig{30u, 200u, 1000u, 400u}, true, nullptr);
        CHECK(buttons.sync(0u));
        mcp.state[1] = true; // Horn asserted by cached MCP adapter.
        buttons.update(1u);
        buttons.update(31u);
        CHECK(buttons.isPressed(1u));
        CHECK(!buttons.isPressed(0u));
        CHECK(!buttons.isPressed(2u));
        CHECK(!buttons.isPressed(3u));
    }



    void testExternalInvalidationContract()
    {
        beginTest("external source-health invalidation retains level and rebaselines recovery");
        Source s;
        const uint8_t pins[1] = {0u};
        ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
        CHECK(b.sync(0u));
        s.state[0] = true;
        b.update(1u); b.update(11u);
        CHECK(b.isPressed(0u));
        b.invalidate(ButtonReadError::AcquisitionFailed);
        CHECK(!b.valid(0u));
        CHECK(b.isPressed(0u));
        s.state[0] = false;
        b.update(100u);
        CHECK(b.valid(0u));
        CHECK(!b.isPressed(0u));
        CHECK(b.pendingEventCount() == 0u);
    }

    void testInteractionBoundarySweep()
    {
        beginTest("short/long classification is exact across a duration sweep");
        const ButtonTimingConfig t{0u, 20u, 50u, 0u};
        for (uint32_t duration = 0u; duration <= 100u; ++duration)
        {
            Source s;
            const uint8_t pins[1] = {0u};
            ButtonHandler<1> b(pins, &logicalCtx, &s, t, true, nullptr);
            CHECK(b.sync(0u));
            s.state[0] = true;
            b.update(1u);
            if (duration >= 50u)
                b.update(1u + 50u);
            s.state[0] = false;
            b.update(1u + duration);
            b.update(2u + duration);

            const ButtonPressType expected = duration >= 50u ? ButtonPressType::Long
                                          : duration >= 20u ? ButtonPressType::Short
                                                            : ButtonPressType::None;
            CHECK(b.getPressType(0u) == expected);
            CHECK(b.getLastPressDuration(0u) == duration);
        }
    }

    void testFourButtonStateSweep()
    {
        beginTest("four-button debounced state snapshots cover every logical combination");
        const uint8_t pins[4] = {0u, 1u, 2u, 3u};
        for (unsigned mask = 0u; mask < 16u; ++mask)
        {
            Source s;
            for (unsigned i = 0u; i < 4u; ++i)
                s.state[i] = (mask & (1u << i)) != 0u;
            ButtonHandler<4> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
            CHECK(b.sync(0u));
            for (unsigned i = 0u; i < 4u; ++i)
            {
                CHECK(b.isPressed(static_cast<uint8_t>(i)) == ((mask & (1u << i)) != 0u));
                CHECK(b.valid(static_cast<uint8_t>(i)));
            }
        }
    }

    void testExhaustiveLogicalAndElectricalStates()
    {
        beginTest("logical/electrical reader truth tables are exhaustive");
        for (unsigned logical = 0u; logical < 2u; ++logical)
        {
            Source s;
            s.state[0] = logical != 0u;
            const uint8_t pins[1] = {0u};
            ButtonHandler<1> b(pins, &logicalCtx, &s, kTiming, true, nullptr);
            CHECK(b.sync(0u));
            CHECK(b.isPressed(0u) == (logical != 0u));
            CHECK(b.setActiveLow(0u, logical != 0u));
            CHECK(b.isPressed(0u) == (logical != 0u));
        }

        for (unsigned activeLow = 0u; activeLow < 2u; ++activeLow)
        {
            for (unsigned high = 0u; high < 2u; ++high)
            {
                Source s;
                s.state[0] = high != 0u;
                const uint8_t pins[1] = {0u};
                ButtonHandler<1> b(pins, BUTTON_ELECTRICAL_READER, &electricalCtx, &s, kTiming, true, nullptr);
                CHECK(b.setActiveLow(0u, activeLow != 0u));
                CHECK(b.isPressed(0u) == ((activeLow != 0u) ? !(high != 0u) : (high != 0u)));
            }
        }
    }
}

int main()
{
    testLogicalCallbackContract();
    testElectricalPolarityContract();
    testElectricalPinReaderSamplesOnce();
    testCheckedReaderFailureAndRecovery();
    testCheckedElectricalFailure();
    testMissingReaderFailsClosed();
    testDebounceAndBounce();
    testShortPress();
    testDoublePress();
    testLongStartedAndReleased();
    testSynchronizedHeldLevelDoesNotCreateInteraction();
    testReaderReconfigurationIsSynchronized();
    testFailedReaderReconfigurationRollsBack();
    testTimingValidation();
    testFailedGlobalTimingSyncKeepsPriorConfig();
    testPerButtonTimingValidation();
    testEventQueuePreservesMultipleInteractions();
    testEventOverflowIsExplicit();
    testLegacyApiDrainsLongStarted();
    testLatchingAndDurableLatchSequence();
    testLatchDisableLifecycle();
    testLatchResetLifecycle();
    testManualLatchAccounting();
    testResetAndSyncPreservesHeldLevelWithoutEvent();
    testMillisWraparound();
    testMultipleButtonsIndependentTiming();
    testStatusSequencesAndFreshness();
    testBoundsFailClosed();
    testContextCallbackShape();
    testExternalInvalidationContract();
    testInteractionBoundarySweep();
    testFourButtonStateSweep();
    testExhaustiveLogicalAndElectricalStates();

    std::printf("PASS: %u tests / %u assertions\n", g_tests, g_assertions);
    return 0;
}
