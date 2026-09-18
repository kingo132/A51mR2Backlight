# A51mR2Backlight

A small Lilu plugin that provides native internal-display brightness control for the **Alienware Area-51m R2** when the internal eDP panel is driven by the **AMD Radeon RX 5700M (Navi10)**.

This project extracts only the backlight path that was originally implemented in WhateverGreen PR #90, instead of carrying a fork of the full WhateverGreen AMD stack.

## Why this exists

The original Area-51m R2 backlight implementation was added to WhateverGreen in 2021. Newer macOS releases changed the private `AMDRadeonX6000Framebuffer` implementation, and maintaining the brightness fix inside a full WhateverGreen fork became increasingly fragile.

A51mR2Backlight isolates the machine-specific functionality into a dedicated plugin:

1. Adds the AppleBacklight panel profiles used by the original WEG patch.
2. Prevents AppleMCCSControl from claiming the internal panel.
3. Hooks the RX 5000-series framebuffer `bklt` attribute.
4. Captures AMD's `panel_cntl` and forwards brightness changes to `dce_driver_set_backlight`.
5. On newer macOS versions, falls back to **validated unique signatures** when Apple no longer exposes the two private C symbols.

It does **not** contain WhateverGreen's general AMD, Intel, NVIDIA, AGDP, connector, DRM, or framebuffer patches.


## Architecture

The source is intentionally split by responsibility:

```text
kern_start.cpp
    -> BacklightController
       -> AppleBacklightPatcher
          - AppleBacklight panel profiles
          - AppleMCCSControl suppression
       -> RadeonBacklightPatcher
          - framebuffer bklt hooks
          - panel controller capture
          - private AMD PWM function resolution
          - brightness-to-PWM conversion
```

`kern_start.cpp` contains only Lilu plugin registration. `kern_backlight.*` coordinates the two independent patchers. Apple panel data lives in its own header so the control flow remains readable, while all Navi10-specific logic is confined to `kern_radeonbacklight.*`.

Release builds keep a small set of stage-level `SYSLOG` messages so a failed hook can still be located. High-frequency values such as individual `bklt` and PWM writes are emitted only by Debug builds with `-a51bkldbg`.

The Radeon `bklt` get hook also acts as a capability advertisement, matching the original WhateverGreen Navi10 implementation. `AppleIntelPanelA::setDisplay` probes `getAttributeForConnection('bklt')` before reaching the routed base `AppleIntelPanel::setDisplay`, so the plugin must return success as soon as the Radeon hook is ready even if no cached brightness value is valid yet. Cached-state validity remains tracked separately through `CurrentBrightnessValid`.

The AppleBacklight path deliberately stays on Apple's normal initialisation flow. The plugin only patches the panel-id format and ensures the required panel profiles when `AppleIntelPanel::setDisplay` runs; it does not replay or reconstruct AppleBacklight state after the fact. The Radeon `bklt` capability response is what allows `AppleIntelPanelA` to continue into Apple's own `buildDisplayParams()` path and publish `linear-brightness`.

## Supported target

- Alienware Area-51m R2
- AMD Radeon RX 5700M / Navi10 driving the internal eDP panel
- macOS Monterey through Tahoe
- x86_64
- Lilu 1.7.2 or newer

This is intentionally a machine-specific kext. Other Navi10 laptops may have similar hardware, but they are not the validation target.

## Installation with OpenCore

1. Build or download `A51mR2Backlight.kext`.
2. Keep `Lilu.kext` enabled and loaded before this plugin.
3. Copy `A51mR2Backlight.kext` to `EFI/OC/Kexts`.
4. Add it to `Kernel -> Add` after Lilu.
5. If WhateverGreen is still required for other graphics fixes, **do not use `applbkl=3`**. Use `applbkl=0` so WEG does not install a second copy of the AppleBacklight/Navi10 brightness hooks.
6. Do not load the old forked WEG brightness implementation and this kext at the same time.

Example OpenCore entry:

```xml
<dict>
    <key>Arch</key>
    <string>x86_64</string>
    <key>BundlePath</key>
    <string>A51mR2Backlight.kext</string>
    <key>Comment</key>
    <string>Area-51m R2 RX 5700M internal brightness</string>
    <key>Enabled</key>
    <true/>
    <key>ExecutablePath</key>
    <string>Contents/MacOS/A51mR2Backlight</string>
    <key>MaxKernel</key>
    <string>25.99.99</string>
    <key>MinKernel</key>
    <string>21.0.0</string>
    <key>PlistPath</key>
    <string>Contents/Info.plist</string>
</dict>
```

`25.x` is macOS Tahoe's Darwin kernel family. Remove `MaxKernel` if you are deliberately testing a later macOS release after validating its framebuffer signatures.

### Boot arguments

- `-a51bkloff` — disable the plugin.
- `-a51bkldbg` — enable plugin debug logging.
- `-a51bklbeta` — allow loading on a newer kernel than the plugin's declared maximum when supported by the installed Lilu version. Use only for bring-up/testing.
- `-a51bkllegacycurve` — use the original WEG 100-step PWM mapping for A/B testing instead of the default linear 8-bit mapping.
- `-a51bklnorestore` — disable reapplying the last known brightness after a later panel-controller initialisation. Useful only for sleep/wake A/B testing.

Logs are prefixed with `A51mR2Backlight` and use `main`, `apple`, and `radeon` module tags.

## Building

The layout follows normal Acidanthera/Lilu plugin projects.

### Dependencies

- Xcode / Command Line Tools
- [Lilu](https://github.com/acidanthera/Lilu)
- [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)

The CI workflow bootstraps dependencies automatically. For a local build from a clean checkout, use the repository bootstrap script. It downloads the official Debug Lilu SDK instead of rebuilding Lilu itself, which avoids old deployment-target failures with current Xcode versions:

```bash
./Scripts/bootstrap.sh
xcodebuild -jobs 1 -configuration Debug
# or
xcodebuild -jobs 1 -configuration Release
```

The archive phase creates a zip under `build/Release/`.

## Runtime diagnostics

Version 1.2 publishes a compact live state set on the plugin IOService so most validation no longer depends on early kernel logs:

```bash
ioreg -lw0 -r -c A51mR2Backlight
```

Useful properties include `RadeonHooksReady`, `PanelControllerCaptured`,
`PanelInitCount`, `BrightnessWriteCount`, `BrightnessRestoreCount`,
`CurrentBrightnessValid`, `CurrentBrightness`, `MaxBrightness`, `LastPWM`,
`BrightnessReadCount`, `BrightnessCapabilityFallbackCount`, `PwmMapping`,
`AppleBacklightHooksReady`, and `PanelProfilesReady`.

The default PWM conversion now preserves AppleBacklight's `linear-brightness`
domain and quantises directly to AMD's 8-bit backlight value. The full-scale
value still uses the original `0x1FF00` encoding. Use
`-a51bkllegacycurve` to compare against the original percentage-based mapping.

On later `dce_panel_cntl_hw_init` calls (for example after a framebuffer
reinitialisation), the plugin reapplies the last known brightness after the
original AMD initialiser returns. Both wake behaviours have been observed on
Sequoia 15.7.7: some wakes restore through ordinary macOS `bklt` writes without
a new panel init, while others re-run panel init and exercise this fallback
before macOS sends its follow-up writes. Use `-a51bklnorestore` only for A/B
testing of the fallback.

## Tahoe strategy

The original WEG code resolved:

- `_dce_panel_cntl_hw_init`
- `_dce_driver_set_backlight`

Newer Apple framebuffer builds do not reliably expose those private symbols. The user's WEG fork worked around this with per-release hard-coded offsets plus a byte check.

This standalone kext is deliberately stricter:

1. Try normal symbol resolution first.
2. If the symbol is unavailable, scan the loaded framebuffer image for a validated function signature.
3. Accept the fallback only when there is **exactly one** match.
4. If there are zero or multiple matches, log the failure and leave the PWM path disabled rather than jumping to a guessed address.

The supplied driver corpus exposed an important detail: the old 20-byte modern
`dce_driver_set_backlight` prologue appears **three times** in Sonoma, Sequoia
15.3 and Tahoe. A51mR2Backlight therefore uses a 76-byte modern signature that
is identical at the real function entry and unique in all three supplied modern
drivers. The panel-init signature remains 20 bytes because it is already unique.

This removes the fixed Tahoe offsets (`0x12DCB3`, `0x12E0EC`) from the runtime
dependency while still validating the exact function bodies against real Apple
driver builds. See [`Reference/Framebuffers/README.md`](Reference/Framebuffers/README.md).

## Source lineage

The brightness logic comes from the author's original WhateverGreen contribution:

- WhateverGreen commit `25b0dcbe67a2c9c0d54fabd9a0edf10c07208321`
- `Add AMD Radeon RX 5000 series PWM backlight control support (#90)`
- Original author: `kingo <46492291+kingo132@users.noreply.github.com>`

The AppleBacklight profile injection and AppleMCCSControl suppression are also extracted from WhateverGreen because they are part of the complete `applbkl=3` behavior, not optional cosmetic helpers.

See [`Docs/EXTRACTION.md`](Docs/EXTRACTION.md) for the exact split.

For bring-up on a new macOS build, follow [`Docs/DEBUGGING.md`](Docs/DEBUGGING.md) stage by stage.

## Safety / failure behavior

The plugin intentionally fails closed. If a required route or unique signature cannot be established, the AMD PWM hook is not marked ready and brightness writes are not redirected to an unverified address.

Before testing a new macOS point release, keep a bootable OpenCore fallback entry and know how to disable the plugin with `-a51bkloff`.

## License

BSD 3-Clause, matching the WhateverGreen source this code was extracted from. See `LICENSE.txt`.

## Checking a new AMDRadeonX6000Framebuffer before booting

If you extract Apple's framebuffer executable from a new macOS update, verify the fallback signatures first:

```bash
python3 Tools/verify_signatures.py /path/to/AMDRadeonX6000Framebuffer --family modern
```

For Tahoe, both modern signatures should report exactly one match. A zero- or
multi-match result is intentionally treated as unsafe by the kext as well.

The repository also contains the supplied Big Sur, Monterey, Sonoma, Sequoia
15.3 and Tahoe reference binaries under `Reference/Framebuffers/`, together
with hashes, Mach-O UUIDs and verified offsets in `manifest.json`.
