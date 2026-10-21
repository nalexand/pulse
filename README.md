# Pulse Studio DAW

<img width="2559" height="1525" alt="pulse_studio_03" src="https://github.com/user-attachments/assets/d9cc2d59-3e3b-45cf-8e1d-9778c2b56572" />


A native, fully offline Windows music workstation with a familiar FL Studio–style
multiwindow interface. Original code, original synthesized sounds, built on
[JUCE](https://juce.com/) — no account, no cloud, no browser, no internet
connection required.

**This repository is the release drop and carries the complete corresponding
source** for the shipped binary, as required by AGPL-3.0-only. It contains the
prebuilt Windows executable, the full application source (`src/`), the native
test suite (`tests/`), the build system (`CMakeLists.txt`, `build.cmd`), the
current feature-status matrix and this README.

---

## Download / run

Grab [`pulse_studio.exe`](pulse_studio.exe) from this repository and run it.

```
pulse_studio.exe
```

- Everything is stored next to the executable in a local `data\` folder
  (recovery snapshots, history, recordings, edits, templates, layout).
- The last recovery snapshot is restored automatically on startup.
- Launching again brings the existing window forward.
- Press **Space** (or the triangular **Play** button) to start playback.
- **F10** opens audio device settings.

Hardware MIDI is intentionally not implemented.

| Item | Value |
| --- | --- |
| Release binary | `pulse_studio.exe` (v0.75.0) |
| Platform | Windows x64 (native, no installer) |
| Size | ~9.7 MB |
| Framework | JUCE 8.0.16-security |
| Source | `src/` · `tests/` · `CMakeLists.txt` · `build.cmd` |

---

## Build from source

The complete corresponding source for `pulse_studio.exe` is in this repository:
application code in `src/`, the native test suite in `tests/`, and the build
system in `CMakeLists.txt` and `build.cmd`.

Requires Visual Studio C++ Build Tools, the Windows SDK, CMake, Ninja and Git.
Run `build.cmd`; it initializes MSVC, fetches the pinned JUCE 8.0.16-security
sources into `vendor/JUCE` (commit `ea77444750de45e036e0279bb0291144bf8e3688`),
configures and builds the app and tests, then runs the engine/persistence suite.
The resulting executable is written to
`build-native\PulseStudio_artefacts\Release\`.

---

## What works today

- **Audio devices** — Windows Audio input/output, sample rate, buffer and
  channel configuration; playback / pause / stop, ruler seek, looping, static
  tempo, swing, accented metronome, tap tempo.
- **Instruments** — synthesized kick, snare, hats, clap, bass, pad and lead plus
  stereo samplers. Add, clone, rename, mute, solo and delete channels.
- **Channel rack & step sequencer** — grouped gray/pink 32-step grid with drag
  painting/erasing, melodic previews, pan/volume, pattern create/duplicate/
  select/rename, probability and ratchets.
- **Piano roll** — draw/move/resize/paint/erase, velocity and probability,
  octave transpose, ghost notes, scale shading, quantize, lasso select,
  clipboard, duplicate, scroll and zoom.
- **Playlist** — multitrack pattern, audio and linked automation clips with
  movement, painting, cloning, edge trims, splitting, source offsets, gain and
  linear fades.
- **Song markers & sections** — up to 128 named, colour-coded markers and
  section ranges, a Playlist marker rail, a floating navigator, jump /
  previous / next, snapped dragging, section-end resizing, linked section
  looping and one-click section export.
- **Mixer** — faders, pan, mute/solo, tone filter, saturation, envelopes,
  shared ping-pong delay, peak meters; fixed inserts with stereo EQ,
  stereo-linked peak compressor (with external sidechain detector) and
  algorithmic reverb, routed to master, bus or another insert.
- **Recording** — one/two-bar count-in, up to two bars of punch pre-roll,
  punch ranges, raw input meters with dBFS readout and clip hold, mono/stereo
  input selection, independent monitor gain, and 24-bit WAV takes written in
  the background to the selected Playlist track. Typing-keyboard note overdub
  recording is supported.
- **Automation** — channel volume, pan, tone, saturation, delay send and
  master volume, with linear/held segments, point editing, clipboard, clip
  cloning/resizing, live readouts and automated offline export.
- **Samples** — WAV, AIFF, FLAC and Ogg import/drag-drop, interpolation,
  sampler pitch, envelopes, preserved stereo, selected source ranges and
  held-note loops.
- **Browser** — background local folder navigation, name search, scrolling and
  up to 32 folder bookmarks; single-click preview, double-click opens the
  waveform editor.
- **Waveform editor** — stereo range selection, wheel zoom, Shift+wheel scroll,
  reverse, normalize to 98% peak and linear fades written as separate local
  24-bit copies; send a range to a sampler or the Playlist cursor.
- **Project audio files** — deduplicated source audit, availability/decoder/
  duration checks, manual locate or recursive folder search, SHA-256 verified
  relinking of shared references in one undoable edit.
- **Templates** — searchable browser, six original starters, personal packages
  that collect samples and support full-project or channel/mixer-only content.
- **Projects** — atomic save/load and validation, 15-second recovery snapshots,
  100-entry undo/redo, named versions, project notes, SHA-256 sample
  collection with relative paths for portable projects.
- **Export** — Song or Pattern WAV/FLAC at 44.1/48/96 kHz, mono/stereo,
  16/24-bit PCM or 32-bit float WAV, full/custom/punch ranges, 0–10 s tails,
  two-pass peak normalization, optional deterministic TPDF dither, and
  isolated source/bus batches.

See [`FEATURE_STATUS.md`](FEATURE_STATUS.md) for the honest, per-feature status
of the 115-item roadmap — **Partial** means a working subset exists, not a
finished feature. Hardware MIDI, collaboration/cloud, plugin hosting and several
other items are explicitly **Skipped** or **Planned**.

---

## Known limits

- 16 channels, 32 patterns, 512 notes per channel/pattern, 512 Playlist clips,
  32768 scheduled events; Playlist lanes map to channels.
- Fixed 32-step rack, 4/4 meter, static tempo. No tempo maps or ASIO yet.
- Preloaded samples up to 120 s each with a 512 MB cache; no streaming yet.
- Recording: 120 s, two input channels into one track; no latency compensation,
  take comping or retrospective capture.
- Automation: 64 clips × 256 points, Song mode only; no curved segments.
- MIDI interchange is PPQ paired notes up to 64 beats, static tempo, no
  controllers or hardware MIDI.
- Export is WAV/FLAC only, at most one hour including tail; no metadata,
  loudness normalization or true-peak limiting.
- Use
  Pulse projects, MIDI interchange and audio imports.
- No installer, no macOS/Linux build, no themes, no shortcut remapping, no
  screen-reader-complete custom controls.

---

## License

**AGPL-3.0-only** — `SPDX-License-Identifier: AGPL-3.0-only`.

The full license text is in [`LICENSE`](LICENSE). Pulse Studio is built on
[JUCE](https://juce.com/), used under its **AGPLv3** option; third-party notices
and the pinned framework version are listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

**You may** download, install, run and modify Pulse Studio free of charge, for
private, educational or commercial use. Music you produce with it is yours — the
license does not extend to your audio output.

**If you redistribute** the program or a modified version, you must do so under
the same AGPL-3.0-only terms and include the complete corresponding source.

**Source availability.** The complete corresponding source for this binary is
included in this repository (`src/`, `tests/`, `CMakeLists.txt`, `build.cmd`)
at <https://github.com/nalexand/pulse>, together with the build instructions
above that fetch the pinned JUCE sources.

**No warranty.** To the extent permitted by applicable law the program is
provided "as is" without warranty of any kind, including merchantability and
fitness for a particular purpose. The entire risk as to quality and performance
is with you. Should the program prove defective, you assume the cost of all
necessary servicing, repair or correction. See sections 15–17 of the license.

