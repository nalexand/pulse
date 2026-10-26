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

The features below reflect the functionality documented through **0.75.0** in
this release folder's [changelog](CHANGELOG.md) and
[feature matrix](FEATURE_STATUS.md). Many roadmap items have working subsets;
this is not a claim that the full DAW roadmap is finished.

- **Workspace** — movable, resizable and maximizable editor panels, saved
  layouts, three factory workspaces and up to 32 personal workspaces; automatic
  fitting on smaller screens, saved 75–200% requested scaling and offscreen
  window recovery with **Ctrl+Shift+H**.
- **Commands & shortcuts** — **Ctrl+P** searches actions, channels, patterns,
  clips, markers, sounds and templates. A searchable editor supports 73
  shortcut bindings, conflict checks and local shortcut-file exchange.
- **Audio devices & transport** — Windows Audio input/output, sample rate,
  buffer and input-channel configuration; play, pause, stop, ruler seek,
  Pattern/Song playback, static tempo, swing, looping and local DSP/device
  reports. Seek, resume and loop starts restore held melodic notes.
- **Metronome & timing** — accented click with four sounds and adjustable
  level, tap tempo, one/two-bar count-in, punch ranges and up to two bars of
  punch pre-roll.
- **Instruments & sound design** — synthesized kick, snare, hats, clap, bass,
  pad and lead, stereo samplers, and editable **Pulse Analog, FM, Wavetable,
  Additive and Resonator** instruments. Sound controls include envelopes,
  filters/LFOs, wavetable morphing, sixteen additive harmonics and plucked-string
  or struck-bar physical models, depending on the instrument.
- **Sound library** — 72 original factory sounds, up to 128 personal sounds,
  categories, favorites and local `.pulsesound` exchange. Factory/personal
  sounds and project samples are available from the Browser for direct channel
  assignment; patches remain embedded in projects and templates.
- **Typing-keyboard performance** — full **Z–/** and **Q–]** piano rows,
  starting at C3/C4 by default, with octave, transpose, velocity and sustain
  controls; pressed/sustained keys highlight in the Piano roll. Note overdub
  recording and bounded 15/30/60-second retrospective phrase capture are
  available without MIDI hardware.
- **Channel rack & step sequencer** — add, clone, rename, mute, solo and delete
  channels; volume/pan, drag painting/erasing and melodic previews. The rack
  displays 4–256 steps, recalls template lengths and offers a 16–64-step
  whole-beat length dialog. Notes support probability, ratchets and conditional
  repeat cycles.
- **Pattern management** — create, duplicate, rename, reorder and delete
  patterns, inspect usage, make a clip unique, split channels, extract selected
  notes and merge patterns in layers or sequence. Automatic or chosen pattern
  colours carry across the Browser, rack, Playlist and Piano roll.
- **Piano roll** — draw, paint, move, resize, erase, transpose, lasso, duplicate,
  scroll and zoom; velocity, probability, ghost notes and scale guides.
  Cross-pattern note copying supports original-position, cursor and start
  alignment, plus explicit copy/move to another pattern/channel.
- **Composition tools** — quantize and seeded humanize, scale fitting, chord
  builders, editable chord progressions with voice leading, Euclidean rhythms,
  six-order arpeggiation and groove capture/application. Tools offer audible
  previews, explicit Apply/Cancel, Undo and local preset libraries.
- **Microtonal tuning** — per-pitched-channel 128-key cent-offset tables,
  reference A4, equal-division generators, Scala `.scl` imports and local tuning
  presets; bundled pitched instruments and calibrated samplers use the tuning.
- **Playlist** — multitrack pattern, audio and linked automation clips with
  painting, group selection/lasso, group move/copy, clipboard, duplicate and
  typed destination-bar placement. Loop a ruler range or selected clip span
  with the full arrangement playing while editing Piano-roll notes.
- **Audio clips & transitions** — edge trims, splitting, source offsets, gain
  and independent Linear, Equal power or S-curve fades. Reviewed crossfade
  fitting works on selected overlapping audio clips without modifying sources.
- **Song markers & sections** — up to 128 named, colour-coded markers and
  ranges, a Playlist marker rail, floating navigator, snapped dragging,
  section-end resizing, linked section loops, whole-clip section copy/move
  and section export.
- **Mixer & routing** — source/bus routing with cycle checks, faders, pan,
  mute/solo, tone filter, saturation, envelopes and a shared ping-pong delay.
  Source, bus and **Master** inserts offer reorderable EQ, dynamics, reverb,
  chorus, flanger and phaser, one instance of each, with eleven factory chains,
  chain clipboard and local `.pulseeffects` files. Master saturation and delay
  return are editable; **Dry master** bypasses Master processing and the return.
- **Dynamics & stereo tools** — compressor, sample-peak limiter, gate and
  expander modes; external sidechain detection for compressor/gate/expander.
  Source/bus/Master stereo controls include width, mono, L/R swap, polarity
  and output trim.
- **Meters & analysis** — live stereo peak/RMS, held peaks, full-scale sample
  counts, correlation and a 2048-point spectrum. Offline mix reports measure
  sample peaks, RMS, DC, crest and spectral balance, with an eligible reviewed
  Master-gain reduction. Local reference-track A/B comparison supports bounded
  passages and optional RMS level matching; these are not LUFS/true-peak tools.
- **Input recording** — dry mono Input 1/Input 2 or stereo capture to one
  Playlist track, raw input dBFS meters/clip hold, independent monitoring gain,
  count-in and punch/pre-roll. Background 24-bit WAV takes are retained locally
  and inserted with Undo, within the release's 120-second recording limit.
- **Retrospective audio** — opt-in 15/30/60-second dry-input history during
  uninterrupted Song playback, with a selectable memory budget. Freeze a
  captured passage, review/audition it, open it in the Waveform editor or place
  it at its captured Song position as one Undo. Capture starts OFF each session.
- **Automation** — channel volume, pan, tone, saturation, delay send and Master
  volume; point editing, clipboard, clip cloning/resizing and Linear, Hold,
  Smooth or Ease segments with tension. Seeded curve generators create editable
  points; playback and offline export use the same automation evaluator.
- **Samples & tuning** — WAV, AIFF, FLAC and Ogg import/drag-drop, preserved
  stereo, interpolation, envelopes, selected source ranges and held-note loops.
  Sampler root-note/fine-cent controls and reviewed monophonic pitch detection
  help align sample playback with Piano-roll pitches.
- **Local sample Browser** — background folder navigation/search, 32 folder
  bookmarks and up to 256 favorite files, previews and direct sampler
  assignment. Per-source BPM settings support next-beat, project-tempo looping
  previews; this uses resampling, so changing playback speed also changes pitch.
- **Waveform editor** — stereo selection, zoom/scroll and audition; cut, copy,
  insert/replace paste, duplicate, trim, delete, silence, reverse, normalize and
  fades in separate retained audio copies, with independent Undo/Redo.
  Background sample-rate conversion preserves duration/pitch; selected ranges
  can be sent to a sampler or the Playlist.
- **Slicing, repair & transcription** — transient detection with editable
  slice markers and Playlist slicing; bounded DC removal, mains-hum reduction,
  isolated-click repair and spectral-band attenuation with before/after review.
  Bounded monophonic audio-to-notes conversion creates an editable pattern;
  polyphonic transcription and full-song accuracy are not promised.
- **Notation** — read-only single-channel current-pattern staff view with
  treble/bass clefs and local MusicXML/SVG export, using an explicitly rounded
  4/4 sixteenth-note representation. This is not a full score editor.
- **Project audio files** — background source audits, decoder/duration checks,
  manual locate and recursive folder search. SHA-256 fingerprints can identify
  renamed known files; reviewed relinking updates shared references in one
  Undo while retaining sampler and clip settings.
- **Templates & portable projects** — six original starters, searchable personal
  full-project or channel/mixer templates, sample collection with relative
  paths, and portable ZIP import/export with manifest/checksum validation.
- **Saving, recovery & history** — validated atomic saves with flushed/read-back
  verification and prior-file backups, 15-second background recovery snapshots,
  configurable archived-recovery retention, named project versions with
  comparison/restore, project notes and a searchable 100-edit session Undo/Redo
  history. Version files and recovery snapshots remain separate from session Undo.
- **Export & interchange** — Song/Pattern WAV or FLAC at 44.1/48/96 kHz,
  mono/stereo, 16/24-bit PCM or 32-bit float WAV; full/custom/punch/section
  ranges, timeline warm-up, 0–10-second tails, two-pass sample-peak normalization,
  optional deterministic TPDF dither and isolated source/bus batches.
  MIDI interchange supports paired PPQ notes and static tempo.
- **Local MCP & project commands** — an opt-in, twelve-tool MCP interface
  controls the open app's sounds, tracks, notes, patterns, Playlist, sections,
  automation, transport, saving/export and mix analysis. Revision checks,
  backups and atomic Undo protect edits. A constrained offline text-command
  interface also supports common project edits. See [MCP.md](MCP.md) and the
  [model-independent operating skill](skills/pulse-studio-mcp/SKILL.md).
  The optional Python MCP bridge and an AI host are separate from the
  standalone workstation; an external host may use its own network services.

See [FEATURE_STATUS.md](FEATURE_STATUS.md) for the boundaries and remaining
work in all 115 roadmap items. **Partial** means a working subset, not a finished
feature. Third-party plugin hosting, ASIO, tempo/meter maps, pitch-preserving
stretching, take comping and other advanced workflows remain unfinished;
hardware MIDI and cloud collaboration are not implemented in this release.

---

## Known limits

- 16 channels, 32 patterns, 512 notes per channel/pattern, 512 Playlist clips,
  32768 scheduled events; Playlist lanes map to channels.
- Variable rack display (4–256 steps), with a 16–64-step length dialog;
  4/4 meter and static tempo. No tempo maps or ASIO yet.
- Preloaded samples up to 120 s each with a 512 MB cache; no streaming yet.
- Recording: 120 s, two input channels into one track; no latency compensation
  or take comping. Retrospective dry-input capture is limited to bounded
  uninterrupted Song passages, not a continuous recording backup.
- Automation: 64 clips × 256 points, Song mode only; six target types and
  drawn/generated curves, without automation recording modes.
- MIDI interchange is PPQ paired notes up to 64 beats, static tempo, no
  controllers or hardware MIDI.
- Export is WAV/FLAC only, at most one hour including tail; no metadata,
  loudness normalization or true-peak limiting.
- Use
  Pulse projects, MIDI interchange and audio imports.
- No installer, no macOS/Linux build, no custom themes and no
  screen-reader-complete custom controls. Shortcut remapping is available.

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

