# Changelog

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
