# Extraction notes

## Baseline examined

The supplied `WhateverGreen.zip` contains a Git checkout rather than a source-only archive.

Its checked-out baseline is commit `bc1b7c3`, with local modifications in:

- `WhateverGreen.xcodeproj/project.pbxproj`
- `WhateverGreen/kern_rad.cpp`
- `WhateverGreen/kern_rad.hpp`
- `WhateverGreen/kern_start.cpp`

The original backlight feature itself is already present in upstream history as commit:

`25b0dcbe67a2c9c0d54fabd9a0edf10c07208321` — **Add AMD Radeon RX 5000 series PWM backlight control support (#90)**.

## What was retained

### From the original PR #90 / upstream WEG

- `AppleBacklightDisplay -> IODisplayParameters -> linear-brightness -> max` lookup.
- Interception of `AMDRadeonX6000_AmdRadeonFramebuffer::setAttributeForConnection` for `bklt`.
- Interception of `getAttributeForConnection` for `bklt`.
- Capture of `panel_cntl` through `dce_panel_cntl_hw_init`.
- Call into `dce_driver_set_backlight` using AMD's U16.16-like PWM value.
- Full-brightness value `0x1FF00`.
- AppleBacklight `F14Txxxx` through `F24Txxxx` panel profiles.
- AppleBacklight panel-id wildcard patch (`F%uT%04x` -> `F%uTxxxx`).
- AppleMCCSControl Gibraltar/Cello probe suppression.

### From the supplied local Tahoe work

The fork had version-specific offsets and 20-byte validation patterns for the two private C functions. The standalone implementation keeps the signatures, but intentionally drops the hard-coded offsets.

Modern (Sonoma/Sequoia/Tahoe) `dce_panel_cntl_hw_init` prologue:

```
55 48 89 E5 41 57 41 56 41 54 53 48 83 EC 10 48 89 FB 4C 8D
```

Modern `dce_driver_set_backlight` prologue:

```
55 48 89 E5 41 57 41 56 41 55 41 54 53 50 41 89 F6 48 89 FB
```

The supplied fork's Tahoe offsets were:

- `dce_panel_cntl_hw_init`: `0x12DCB3`
- `dce_driver_set_backlight`: `0x12E0EC`

Those offsets are documented only for provenance; A51mR2Backlight does not depend on them.

## What was deliberately not retained

The local `kern_rad.cpp/.hpp` modifications also contain extensive reverse-engineering and experiments around:

- AGDC mode validation and mode-set logging
- detailed timing structures
- framebuffer EDID logging
- logger internals
- sleep/wake experiments
- panel power-state inspection
- extra AMD internal structures
- fixed offsets unrelated to basic brightness

None of those are required for the original RX 5700M PWM brightness feature, so they are excluded from this project.

## Runtime relationship with WhateverGreen

A51mR2Backlight depends on **Lilu**, not on WhateverGreen.

WhateverGreen can still be loaded for unrelated fixes, but its Apple backlight feature must be disabled (`applbkl=0`) to avoid two plugins routing the same functions.
