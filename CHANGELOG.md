# Changelog

All notable changes to Universal_Button are documented here.

## [2.0.0] - 2026-08-07

### Changed

- Removed the generic `LIBRARY_VERSION*` macros from the default public surface because those global names collide across libraries. `UNIVERSAL_BUTTON_VERSION*` now carries the version directly.
- Corrected the v1.x callback polarity defect. Existing `makeButtonsWithReader(...)` and context-reader callbacks now follow their documented contract: `true` means logically pressed and is not polarity-transformed again.
- Added explicitly named electrical HIGH/LOW reader APIs for callbacks that need library-side active-low/high normalization.
- Removed the silent `TestButton`/GPIO25 fallback. `Universal_Button.h` now requires an explicit `BUTTON_LIST(X)` mapping.
- Moved the canonical generated mapping into `UB::config`; v1.x global mapping aliases remain available by default for migration and can be disabled with `UB_NO_LEGACY_CONFIG_GLOBALS`.
- `reset()` now synchronizes current hardware state instead of assuming every input is released.
- Runtime reader, polarity, timing and per-button configuration changes are synchronized so held inputs do not create synthetic events.

### Added

- Validity-aware logical readers through `ButtonPressedResult`.
- Validity-aware electrical readers through `ButtonLevelResult`.
- Explicit acquisition health/freshness metadata (`valid`, `hasSample`, sample/error timestamps, acquisition/change sequences and synchronization generation).
- `invalidate()` for cached/external input systems whose source health is known outside the button callback.
- Bounded detailed event queue with `ButtonEvent`, monotonic event sequence, sticky overflow indication and dropped-event count.
- Explicit `LongStarted` and `LongReleased` detailed event semantics while keeping legacy `ButtonPressType::Long` release-based.
- `isLongHeld()` for continuous hold/deadman-style logic.
- Timing validation through `ButtonConfigResult` / `ButtonConfigError`.
- Durable latch-change sequence metadata.
- Dedicated native tests, sanitizers, host example syntax checks, package/release contract checks and GitHub Actions compile matrix.
- Beginner-first README path while retaining the full technical reference.

### Fixed

- Logical active-high/active-low callback inversion caused by applying polarity to a callback that already returned pressed state.
- One-slot event overwrite when multiple interactions completed before a consumer read them.
- Reader failures being indistinguishable from a legitimate released state.
- Synthetic press/release interactions after runtime reader/polarity changes.
- Interaction timing continuing across unobserved acquisition gaps.
- Held buttons being treated as newly pressed after synchronization/reset.
- Ambiguous first-frame/first-sample state for external providers.
- Missing timing-order validation.
- C++11/Clang portability of `[[nodiscard]]` use through a compatibility macro.

### Compatibility notes

- PW_PVT's existing context callback shape remains source-compatible and now receives the logical-pressed behavior its callback documentation already expected.
- PW_PVT's MCP failure path should change from `buttons.reset()` to `buttons.invalidate(...)` when PW_PVT itself is updated, because v2 reset semantics intentionally synchronize hardware rather than force a released state.

## [1.7.0]

- Previous release supplied in the project archive.
