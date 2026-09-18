# AMDRadeonX6000Framebuffer reference corpus

These binaries were supplied by the project owner and are kept here as a
versioned reference corpus for validating the private AMD backlight hooks.
They are **not** loaded or bundled into `A51mR2Backlight.kext`.

The runtime kext still resolves public C++ framebuffer methods by symbol first.
For the two private DC routines, it tries symbols and then a validated byte
signature. The reference corpus exists so changes to those signatures can be
reviewed against real Apple driver builds instead of guessed from one OS.

## Verified offsets

| Label | Source version | Mach-O UUID | panel init | set backlight | family |
|---|---|---|---:|---:|---|
| Big Sur | 4.6.21.0.0 | A163820E-69B2-3799-8ED8-61BB92CDBB9B | `0x124B21` | `0x124F56` | legacy |
| Monterey | 4.8.54.0.0 | 2FE08341-C2AC-391F-91AD-104EB632986C | `0x12CB94` | `0x12CFC9` | legacy |
| Sonoma | 5.5.17.0.0 | 63B9D080-B8F5-3C13-8D62-7E308A0E8870 | `0x12D99F` | `0x12DDD8` | modern |
| Sequoia 15.3 | 6.1.13.0.0 | 425C841B-BAFE-3B4F-8182-CA824CFA8CF9 | `0x12DA0F` | `0x12DE48` | modern |
| Tahoe | 7.1.6.0.0 | E7531D6B-3201-31A0-97C8-8826F8F69320 | `0x12DCB3` | `0x12E0EC` | modern |

Full hashes and machine-readable results are in `manifest.json`.

## Important finding

The old 20-byte modern `dce_driver_set_backlight` prologue is **not unique**.
It occurs three times in every supplied Sonoma, Sequoia 15.3 and Tahoe binary.
That means the first standalone implementation correctly failed closed, but it
also meant the PWM path could never become ready on those releases.

The project now uses a 76-byte signature for the modern function. The first 76
bytes of the real function are identical across the supplied Sonoma, Sequoia
15.3 and Tahoe binaries and occur exactly once in each.

The 20-byte panel-init signatures are unique in the corresponding legacy and
modern binaries. The legacy 20-byte set-backlight signature is unique in the
supplied Big Sur and Monterey binaries.

Ventura is currently supported through the legacy signature family, matching
the historical patch, but there is no Ventura binary in this corpus yet. Add
one before treating Ventura as independently re-validated.

## Re-run validation

From the repository root:

```bash
python3 Tools/verify_signatures.py \
  Reference/Framebuffers/AMDRadeonX6000Framebuffer.bigsur \
  Reference/Framebuffers/AMDRadeonX6000Framebuffer.monterey \
  --family legacy

python3 Tools/verify_signatures.py \
  Reference/Framebuffers/AMDRadeonX6000Framebuffer.sonoma \
  Reference/Framebuffers/AMDRadeonX6000Framebuffer.sequoia.15.3 \
  Reference/Framebuffers/AMDRadeonX6000Framebuffer.tahoe \
  --family modern
```

To inspect hashes, Mach-O UUIDs, source versions and symbol availability:

```bash
python3 Tools/analyze_framebuffers.py Reference/Framebuffers/AMDRadeonX6000Framebuffer.*
```
