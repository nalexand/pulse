# Third-party notices

Pulse Studio is licensed **AGPL-3.0-only** (see [`LICENSE`](LICENSE)).
Third-party components retain their own licenses, reproduced or referenced below.

---

## JUCE

| Item | Value |
| --- | --- |
| Component | JUCE framework modules |
| Version | 8.0.16-security |
| Pinned commit | `ea77444750de45e036e0279bb0291144bf8e3688` |
| Location | `vendor/JUCE` (development workspace) |
| License | Dual-licensed: **AGPLv3** or the commercial JUCE 8 licence |
| Upstream | <https://github.com/juce-framework/JUCE> · <https://juce.com/legal/juce-8-licence/> |
| Notice file | `vendor/JUCE/LICENSE.md` |

Pulse Studio uses JUCE under the **AGPLv3** option. No commercial JUCE
subscription has been purchased. Because of this, the combined work — including
the distributed `pulse_studio.exe` — must be conveyed under AGPL terms, and the
complete corresponding source must accompany the binary.

JUCE's own copyright and module notices are preserved unmodified.

---

## Original content

- **Application source** (`src/`) — original work, © 2026 nalexand, AGPL-3.0-only.
- **Factory sounds** — synthesized at runtime by the original application code.
  No third-party samples, presets or sound libraries are bundled.
- **Project files** (`*.pulse`) — original format, no third-party data.

---

## Not used / not included

- **No Image-Line material.** No FL Studio source code, assets, artwork, icons,
  logos, instrument names or product names are included. "Pulse Studio" is an
  independent application; only generic workflow conventions and publicly
  documented keyboard shortcuts are followed for familiarity.
- **No plugin hosting**, so no third-party VST/AU binaries are distributed.
- **No bundled audio samples** from any library.

---

## Your obligations when redistributing

1. Keep this file and all upstream copyright notices intact.
2. Ship the full AGPL-3.0 text (`LICENSE`).
3. Provide the complete corresponding source, including the pinned JUCE sources
   or a build instruction that fetches the exact commit above.
4. Mark modified versions as modified, with a date (AGPL §5a).
