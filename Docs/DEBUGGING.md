# Debugging A51mR2Backlight

The plugin is split into independent stages so a failure can be located before changing code.

Use a **Debug** build while bringing up a new macOS version and boot with:

```text
-a51bkldbg -liludbgall keepsyms=1 debug=0x100
```

If WhateverGreen is also loaded, keep `applbkl=0` so only A51mR2Backlight owns this path.


## Capturing early Lilu/plugin logs reliably

`log show` and `dmesg` can miss very early kernel-extension messages. With a
**Debug** Lilu build, add a temporary dump argument such as:

```text
liludump=60
```

After booting, wait at least 60 seconds and verify that the dump filename
matches the versions you are actually running:

```bash
kmutil showloaded | grep -Ei 'A51mR2Backlight|Lilu|WhateverGreen'
uname -r
sudo ls -lt /var/log/Lilu_*.txt
```

For example, a current Lilu 1.7.2 boot on Darwin 24.6 should not be debugged
from an old file named `Lilu_1.7.0_24.3.txt`. Treat a mismatched version/kernel
filename as a stale dump.

Then filter the newest matching file:

```bash
sudo grep -Ei   'A51mR2Backlight|AppleBacklight|MCCS|X6000|PWM|panel'   /var/log/Lilu_1.7.2_24.6.txt
```

## Live runtime state (1.2+)

After the system reaches the desktop, inspect the plugin directly:

```bash
ioreg -lw0 -r -c A51mR2Backlight
```

For a healthy end-to-end brightness path, look for:

```text
AppleBacklightHooksReady = Yes
PanelProfilesReady = Yes
RadeonHooksReady = Yes
PanelControllerCaptured = Yes
CurrentBrightnessValid = Yes
BrightnessWriteCount > 0
LastPWM > 0   # except at minimum brightness
```

`PanelInitCount` is useful around sleep/wake, but it is not expected to increase
on every wake. Both behaviours were observed on the validated Sequoia 15.7.7
system: one wake left `PanelInitCount` unchanged and restored through ordinary
`bklt` writes, while another incremented `PanelInitCount` and exercised the
brightness-restore fallback before follow-up `bklt` writes. If panel init does
increase, `BrightnessRestoreCount` should also increase once a valid brightness
value has been observed, unless `-a51bklnorestore` is present.

The default PWM mapping is `linear-8-bit`. Add `-a51bkllegacycurve` only when
comparing against the original WEG percentage mapping.

## Stage 0 — plugin loaded

```bash
kmutil showloaded | grep -Ei 'A51mR2Backlight|Lilu|WhateverGreen'
```

Expected: Lilu and `com.kingo132.A51mR2Backlight` are present.

## Stage 1 — coordinator started

```bash
log show --last boot --style compact --predicate 'process == "kernel"' \
  | grep 'A51mR2Backlight'
```

Expected Release/Debug checkpoints:

```text
main: @ initialising on Darwin ...
main: @ kext watchers registered
```

If these are missing while the kext appears in `kmutil`, verify that the installed kext is the newly built one.

## Stage 2 — Apple backlight path

Expected checkpoints:

```text
apple: @ processing AppleBacklight
apple: @ AppleBacklight hooks ready
apple: @ AppleMCCSControl probes disabled
apple: @ ensured AppleBacklight panel profiles (... repaired)
apple: @ panel display set returned 1
```

Then verify the service exists **and exports a brightness parameter**:

```bash
ioreg -lw0 -r -c AppleBacklightDisplay
```

`AppleBacklightDisplay` existing by itself is not enough. Its
`IODisplayParameters` dictionary must contain `linear-brightness`; otherwise
macOS will not expose the brightness slider even if the display service is
present. Check this on the final `AppleBacklightDisplay` object instead of in
the routed base `AppleIntelPanel::setDisplay`: Sequoia builds the display
parameters later in `AppleIntelPanelA::setDisplay`, after the base call returns.

If this stage fails, stay in `kern_applebacklight.*`; do not change Radeon code yet.


If `AppleBacklightHooksReady=Yes` but `PanelSetDisplayCount=0` and `linear-brightness` is absent,
check the Radeon Stage 3 path next. `AppleIntelPanelA::setDisplay` probes
`getAttributeForConnection('bklt')` as a capability check before it reaches the routed base
`AppleIntelPanel::setDisplay`. A healthy 1.2 boot normally records at least one
`BrightnessReadCount`; on Navi10 a failed original query may also increment
`BrightnessCapabilityFallbackCount` while still returning capability success to Apple.
Do not add a late AppleBacklight replay: the validated fix is to preserve the original WEG
`bklt` capability semantics so Apple's normal initialisation can complete.

## Stage 3 — Radeon framebuffer hooks

Expected checkpoints:

```text
radeon: @ processing AMDRadeonX6000Framebuffer
radeon: @ framebuffer brightness methods routed
radeon: @ private PWM functions resolved and panel-init routed
radeon: @ RX 5000 PWM brightness hooks are ready
```

A signature failure is reported explicitly with its match count. Zero or multiple matches intentionally stop the PWM path.

## Stage 4 — panel controller captured

Expected after the AMD display path initialises:

```text
radeon: @ panel controller captured; max brightness 0x...
```

This proves the `dce_panel_cntl_hw_init` wrapper actually executed, not merely that it was installed.

## Stage 5 — brightness events reach the framebuffer

Move the macOS brightness slider while watching the Debug log:

```bash
log stream --style compact --predicate 'process == "kernel"' \
  | grep 'A51mR2Backlight'
```

Expected Debug-only messages:

```text
radeon: @ (DBG) bklt write ...
radeon: @ (DBG) PWM write brightness=... max=... pwm=...
```

Interpretation:

- No `bklt write`: the problem is above the AMD PWM call, usually AppleBacklight or the framebuffer method route.
- `bklt write` but no `PWM write`: one of the PWM prerequisites is not ready.
- Both messages appear but panel brightness does not change: focus only on `panelController`, `dce_driver_set_backlight`, its ABI, or PWM encoding for that macOS build.

## Minimal log collection

```bash
{
  echo '===== SYSTEM ====='
  sw_vers
  uname -r

  echo '===== BOOT ARGS ====='
  nvram -p | grep boot-args || true

  echo '===== LOADED ====='
  kmutil showloaded | grep -Ei 'A51mR2Backlight|Lilu|WhateverGreen' || true

  echo '===== A51mR2Backlight ====='
  log show --last boot --style compact --predicate 'process == "kernel"' \
    | grep 'A51mR2Backlight' || true

  echo '===== APPLE BACKLIGHT ====='
  ioreg -lw0 -r -c AppleBacklightDisplay || true
} > ~/Desktop/A51mR2Backlight-debug.txt
```

Work through the stages in order. Do not compensate for a failed early stage by adding later-stage patches.
