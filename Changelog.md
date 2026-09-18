# Changelog

## 1.2.1

- Remove the misleading `LinearBrightnessSeen` live diagnostic and the corresponding base-`setDisplay` check. On Sequoia, `linear-brightness` is created later by the derived `AppleIntelPanelA::setDisplay` path, so checking inside the routed base method can report `No` on a fully working system.
- Keep `AppleBacklightDisplay -> IODisplayParameters -> linear-brightness` as the authoritative end-to-end validation step in the debugging guide.
- Document both validated sleep/wake behaviours: ordinary `bklt` re-writes without panel reinitialisation, and panel reinitialisation with one successful brightness-restore fallback followed by normal `bklt` writes.
- No functional changes to `bklt` capability handling, PWM conversion, panel-controller capture, or wake brightness restore.

## 1.2.0

- Remove the 1.1.1–1.1.5 AppleBacklight late-recovery/notifier/replay state machine; 1.1.6 validation proved the normal Apple path succeeds once `bklt` capability semantics are correct.
- Keep AppleBacklight handling minimal and idempotent: route `AppleIntelPanel::setDisplay`, patch the panel-id format, ensure the seven profiles, and verify `linear-brightness`.
- Keep the validated `bklt` capability fix: capability is advertised whenever Radeon hooks are ready, independently of whether a cached current-brightness value is valid.
- Keep direct linear 8-bit PWM quantisation with the original `0x1FF00` full-brightness encoding; retain `-a51bkllegacycurve` only as an A/B fallback.
- Keep panel-init brightness restore only as a fallback for true framebuffer reinitialisation. Normal Sequoia 15.7.7 sleep/wake was observed to restore brightness through ordinary `bklt` writes without re-running panel init.
- Reduce runtime diagnostics to the state that proved useful during validation; remove all `DisplayRecovery*`, `PanelRecovery*`, and derived-replay properties.
- Reduce Release AppleBacklight logging after the first successful `setDisplay`; later display-set events remain Debug-only.

## 1.1.6

- Fixed the actual 1.1.x regression: `getAttributeForConnection('bklt')` was incorrectly gated on `CurrentBrightnessValid`.
- `AppleIntelPanelA::setDisplay` uses the `bklt` get-attribute call as a capability probe before it reaches the routed base `AppleIntelPanel::setDisplay`; returning the original unsupported status made the Apple backlight path exit before profiles and `linear-brightness` could be built.
- Separate capability from cached-state validity: once the Radeon hook is ready, `bklt` reads advertise success. A valid cached brightness is returned when available; otherwise a failed original query falls back to zero, matching the original WhateverGreen Navi10 behavior.
- Added `BrightnessReadCount` and `BrightnessCapabilityFallbackCount` diagnostics to prove the capability probe path at boot.
- Keep the late-recovery machinery for this test build, but it should remain dormant if the normal Apple path now succeeds; it can be removed after validation.

## 1.1.5

- Reverse-engineered the Sequoia 15.7.7 AppleBacklight / AppleBacklightExpert binaries to identify the missing late-recovery step.
- Late recovery now replays the complete `AppleIntelPanelA::setDisplay(IODisplay *)` path instead of only the base `AppleIntelPanel::setDisplay`.
- The derived method naturally calls the routed base method (where profiles are repaired) and then continues through `buildDisplayParams()`, which installs `IODisplayParameters` including `linear-brightness`.
- Added diagnostics for derived symbol resolution, runtime panel class, derived replay attempts, and failures.

## 1.1.4

- Fixed Sequoia 15.7.7 recovery being blocked by `OSDynamicCast(IODisplay, AppleBacklightDisplay)`.
- Recovery validates the exact runtime class name `AppleBacklightDisplay` before using the matched object with the Apple `setDisplay` ABI.
- Added display-class and raw-replay diagnostics.

## 1.1.3

- Retry AppleBacklight recovery when AppleIntelPanel/AppleIntelPanelA is published, closing the display-before-panel boot-order race.
- Use the panel service delivered by the IOKit notification directly during recovery.
- Add diagnostics for panel recovery notifications and recovery early-exit reasons.

## 1.1.2

- Probe for an already-published AppleBacklightDisplay immediately after the AppleIntelPanel hook is installed.
- Use first-publish notifications so future AppleBacklightDisplay instances are observed as soon as they register.
- Add notification/probe counters to distinguish normal hook execution, immediate recovery, and future-display recovery.

## 1.1.1

- Add an AppleBacklightDisplay recovery path for boots where AppleIntelPanel::setDisplay runs before the route is active.
- Replay only when `linear-brightness` is missing and no normal wrapped setDisplay call has been observed.
- Add IORegistry counters for recovery arm/attempt/success state.

## 1.1.0 - test

- Preserve and expose the real current brightness state instead of reporting zero before the first write.
- Reapply the last known brightness after later panel-controller initialisations; add `-a51bklnorestore` for A/B testing.
- Replace the percentage-first PWM conversion with direct linear 8-bit quantisation; add `-a51bkllegacycurve` for A/B testing.
- Revalidate AppleBacklight panel profiles on every `setDisplay` call so display reinitialisation is idempotent.
- Publish AppleBacklight/Radeon state and counters on the `A51mR2Backlight` IORegistry service.
- Keep Night Shift/color handling intentionally out of the kernel plugin while adding counters that help correlate display reinitialisation with wake.

## Unreleased

- Split orchestration, AppleBacklight/MCCS handling, and Radeon PWM handling into focused modules.
- Move Apple panel profile data out of control-flow code.
- Add stage-level Release logging and detailed Debug-only brightness/PWM logging.
- Add a local dependency bootstrap script compatible with current Xcode versions.
- Raise the project deployment target to macOS 12 for current Xcode compatibility.
- Add a Big Sur → Tahoe AMDRadeonX6000Framebuffer reference corpus with hashes, UUIDs and verified offsets.
- Replace the ambiguous 20-byte modern `dce_driver_set_backlight` fallback with a 76-byte signature verified unique in Sonoma, Sequoia 15.3 and Tahoe.
- Add AppleBacklight bring-up diagnostics, including `linear-brightness` verification after `AppleIntelPanel::setDisplay`.
- Document stale Lilu dump detection and `liludump=N` collection.

## 1.0.0

- Extract RX 5000 / Navi10 internal PWM backlight support from WhateverGreen.
- Include the AppleBacklight profile injection required by the original `applbkl=3` path.
- Include AppleMCCSControl probe suppression.
- Replace version-specific Tahoe function offsets with symbol-first, unique-signature fallback resolution.
- Fail closed if a private AMD function cannot be identified unambiguously.
- Add GitHub Actions build workflow and OpenCore installation documentation.
