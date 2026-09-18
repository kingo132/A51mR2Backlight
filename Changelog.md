# Changelog

## 1.0.0

- Extract RX 5000 / Navi10 internal PWM backlight support from WhateverGreen.
- Include the AppleBacklight profile injection required by the original `applbkl=3` path.
- Include AppleMCCSControl probe suppression.
- Replace version-specific Tahoe function offsets with symbol-first, unique-signature fallback resolution.
- Fail closed if a private AMD function cannot be identified unambiguously.
- Add GitHub Actions build workflow and OpenCore installation documentation.
