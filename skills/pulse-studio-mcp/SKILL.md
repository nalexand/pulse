---
name: pulse-studio-mcp
description: Compose, edit, design sounds, arrange, play, save and export music in the open local Pulse Studio app through its MCP tools. Use for controlling Pulse Studio from an AI client, including creating editable songs; not for FL Studio automation or application source development.
---

# Pulse Studio through MCP

Control the actual open Pulse Studio workstation, not a project JSON file behind it. This skill is model/client independent: use the tools exposed by your MCP host. A host may prefix names with its server namespace; the underlying tool names are `pulse_status`, `pulse_edit`, etc. No source-code reading is needed.

Read [the connection and API reference](references/api.md) for setup, exact operation fields/ranges, units, routing, error recovery and a complete composition example. Read it before your first composition or when constructing an unfamiliar operation. It describes Pulse Studio 0.95.0 with twelve tools (the eleven-tool base from 0.31 plus offline mix analysis). Audio fade authoring requires app/server 0.54+ and an existing native audio clip; use `set_audio_fades` with a fresh clip index and revision, preserving every unspecified field. Current project writes use schema 35; schemas 1–34 with supported instruments remain readable; schemas 1-16 gain neutral harmonic defaults, and schemas 1-15 migrate missing fade shapes to Linear. The optional pattern colour field requires app/server 0.34+; omit it for older versions. section_clips requires app/server 0.36+ and whole clips inside the section; extract_notes and merge_patterns require app/server 0.35+; use fresh lane indices and inspect appended results before retrying a timeout.

## Essential workflow

1. Call `pulse_status`, then `pulse_get_project` when editing an existing song. Establish the current title, tracks, patterns, clips, recording/export/modal state and `revision`. Respect existing work and the user's requested scope.
2. For an explicitly requested new composition, call `pulse_new_song` with the latest revision. This creates a History backup and an undoable new session. For a revision to an existing song, preserve unrelated notes, sounds and clips.
3. Read status immediately before **every mutation**, including transport, view, save and export. Pass `expected_revision` from that read. Send dependent writes sequentially. Reconcile a revision conflict against the user's new state instead of blindly retrying.
4. Use `pulse_edit` batches to configure sounds, patterns and note lanes, then the Playlist. One successful batch is one undo step. Inspect errors (`isError`), not just transport success. Rejected batches do not commit partial edits.
5. Read the project back. Check note pitches/timing, pattern lengths, clip tracks and lengths, routes, sections and Song/PAT mode against the intended music. `pulse_view` and `pulse_snapshot` show the actual app to the user.
6. Save with `pulse_save_project`. Export with `pulse_export_audio`, then poll status until `exporting` is false **and** the status says export completed. A returned destination path alone does not prove a finished export.
7. If playback is requested, use `pulse_transport`. Verify advancing `positionBeats`, processed frames and active audio outputs; use a snapshot or a fresh audio measurement to substantiate sound, not a stale historical peak. Report actual files and practical limits.

## Decisions that prevent lost work or misleading results

- `set_notes` replaces **one entire pattern/channel lane**. To append/edit individual notes, read that lane and send the full retained+changed list. An empty array clears it.
- `set_arrangement` replaces **all clips**, including existing automation and audio clips. Its input only represents pattern clips. Do not use it to rebuild a mixed audio/automation arrangement: the current API cannot round-trip those clip types through that operation. For a new song, send pattern clips first, then append automation with `add_automation`. For an existing mixed arrangement, stay within supported lane/sound edits or explain the API gap.
- A pattern contains a note lane for each channel. A Playlist pattern clip plays only its specified `track` lane, repeating that lane at the pattern's length until the clip ends. Placing one clip does not play every channel in the pattern. Song mode needs clips; PAT mode plays the active pattern directly.
- All indices are zero-based; visible “Track 8” is `track: 7`. Times are quarter-note beats, except track attack/release and the explicitly named synth/effect units. A four-beat bar at 120 BPM lasts two seconds. Tempo is currently static; there are no tempo-map/MIDI-device requirements.
- Sounds are editable built-in synths and synthetic drums. Call `pulse_list_sounds` for exact factory preset names and settings. Do not invent plugin identifiers or describe a simple synthetic approximation as a realistic recorded instrument. There is no MCP sample import, plugin host or vocal generator in this API.
- Make the scope of a cover/transcription clear. MCP enters the notes you send; it does not discover a recording's melody. Use a reliable transcription/reference for exact pitches and rhythms. Do not attribute guessed or simplified notes to an MCP bug or claim exactness without checking them.
- A timeout may occur **after** a mutation committed. Inspect status/project and compare the intended result before retrying. This matters especially for append operations and Undo. Do not edit mailbox files, session tokens, recovery or saved project files as a substitute for tools.
- Stop a mutation if recording, a modal dialog, an unapplied note-tool preview or an export is blocking it; complete independent read-only work and let the user finish their active action. Do not dismiss their modal or cancel their take just to gain control.
- Save/export names are basenames with no path or extension. Prefer a new name for an alternate version. Use `overwrite: true` only when replacing that named output is within the user's request.
- Backups under `data/History` and Undo preserve earlier project state. They do not copy externally referenced sample audio; keep source assets available. Personal sound libraries and keyboard capture memory are separate from saved projects.

## Offline mix review

For an authorized mix review, use `pulse_mix_analysis` (app/server 0.68+) with `start` and a fresh revision, then poll read-only `status` until `mixAnalysis.busy` is false. Require a successful current report; explain sample-peak/RMS/FFT observations without claiming LUFS, true peak or having listened. Apply only within the user's mix-editing scope: an eligible `apply_level` re-renders, verifies source bytes and changes just Master gain as one Undo. Poll completion and read back the project. A returned busy response does not mean it applied. Saved/stale reports cannot authorize corrections. See the reference's **Offline mix analysis** section for exact fields and failure handling.

## Discovery and delivery

For another AI client, provide this whole skill directory and configure that client's MCP stdio server using the reference. A project-local skill is not automatically installed in every AI application. The portable Pulse Studio package includes this skill and the Python MCP server. Keep its `references` directory beside `SKILL.md` when copying it.

Deliver a concise description of what actually changed, links to saved project/audio files, and any meaningful unverified point. Technical waveform/level checks verify rendering and signal health; they do not prove musical accuracy or that the result was heard by ear.

Version 0.41 adds native portable project ZIP export/import, collection/checksums, cancellation and guarded backup/Undo when opening imported songs. No new MCP operation or persisted song/sound schema. Native project opening now advances revision, so always read status again after a user opens a different song. ZIP packaging is available in the native File menu, not through pulse_export_audio.

Version 0.42 adds native scale/key guides, pitch fitting and chord-building previews. No new MCP tools or operation fields. Guide preferences stay outside projects. For the equivalent note changes, read the current lane and use set_notes with a fresh expected_revision; preserve the full destination lane when appending chords because set_notes replaces it. Native unapplied harmony previews use the existing busy guard for MCP writes.

Version 0.43 adds native quick/named version snapshots, local version browsing, structural comparison and guarded restore with backup/Undo. No new MCP tools, operations or song/sound schema. Snapshots are standard .pulse files with collected relative assets in data/Versions. A native restore changes the project revision, so read pulse_status or pulse_get_project again before a write. Comparison dialogs use the existing modal busy guard for MCP writes.

Keyboard shortcuts (0.44) are native local preferences: OPTIONS > Keyboard shortcuts supports remapping, conflict checks, disable/defaults and .pulseshortcuts import/export. They do not change the eleven MCP tools or project revisions. Key examples use factory defaults; command/menu hints show live bindings. MCP clients should use tool operations rather than keyboard simulation.

Sampler tuning (0.45+): set_track.params.sampleRoot is an integer MIDI note 0–127 describing the recorded pitch (C4=60, A 3=57); sampleTune is a playback correction from -100 to +100 cents, positive raising the result. A sharp A 3 sample by 23.5 cents needs {"sampleRoot":57,"sampleTune":-23.5}; then write intended Piano roll pitches normally. These fields affect only sampler playback, not Playlist audio. Use them on an already loaded sampler, inspect readback and use a fresh revision for each write; arbitrary sample loading remains unavailable. Schema 15 persists calibration; schemas 1–14 migrate to C4/0. Native Channel settings > Tune > Detect pitch proposes a monophonic estimate for review; the detector has no MCP operation. Eleven tools remain.

App 0.47 displays Automatic patterns with distinct swatches by pattern order; explicit palette values retain their existing meaning. Native ruler/selected-clip loop selection creates or updates the ordinary section `pulse-playlist-loop` and uses `loopSection`. It plays all Song tracks in that time span. Existing MCP marker/section operations remain available; no new tool or operation was added. Read fresh state after native edits.

App 0.48 adds native selected-audio crossfade fitting and fade clearing using existing schema 15 fade fields. These UI actions are not new MCP operations. Read fresh state after native edits; set_arrangement still replaces the entire clip list and does not expose arbitrary audio file loading or fade authoring.

App 0.49 adds native harmony preset management and .pulseharmony files. Recall previews settings and a temporary key; Apply commits generated/fitted notes normally. Presets do not introduce MCP operations or alter project schema 15. Resolve pending native previews before MCP writes, and read fresh state after Apply.


App 0.55 adds native personal progression presets and six complete factory starters in Chord progression > Presets. Presets retain key/scale, degrees, octave, step, gate, velocity, triads/sevenths, voice leading and duplicate policy; recall keeps the target beat and Preview/Original mode, previews a temporary guide, and Apply remains one Undo after PAT/Song validation. Import/export uses .pulseharmony; the shared 128-entry catalog uses harmony schema 2 when progressions are present and reads schema 1. Scale/chord-only exports remain schema 1. This is separate from project schema 17, the instrument sound catalog and sound format. There are no new MCP tools or operations for preset management. Apply/Cancel pending native previews before MCP writes, then read fresh state; do not interpret progression presets as instrument sounds or write catalog files to bypass the enabled MCP boundary.


### Pulse Additive (app/server 0.56+)

Restart both native app and stdio server after updating. The Additive release expanded pulse_list_sounds to 64 factory sounds; current 0.66 lists 72, including eight Resonator sounds. Use instrument string additive in add_track/set_track; project readback uses integer12. In synth overrides, harmonics is exactly 16 finite JSON numbers from 0 to 1, ordered fundamental through harmonic 16. Providing it replaces the complete array; omission preserves every level. Unknown fields, booleans, strings, NaN/infinity, wrong lengths and out-of-range values reject atomically. The full patch stays embedded; there are no external wavetable/spectrum assets. Other synth fields keep their documented ranges. Mix/coarse/detune control two oscillators using the same spectrum; sub/noise are independent, while FM/table/wave-selection fields are stored but do not add those engines. Harmonic levels smooth, normalize by at least their sum, and upper oscillator partials fade below Nyquist. All-zero harmonics silence the harmonic oscillators; disable sub/noise too when silence is required.

Read fresh pulse_get_project first and confirm app version 0.56+ and an existing source-channel index. A sample operation for an existing source track 7 inside a revision-checked pulse_edit.operations batch is:

```json
{"op":"set_track","track":7,"instrument":"additive","preset":"Glass partial keys","synth":{"harmonics":[1,0,0.5,0,0.25,0,0.12,0,0,0,0,0,0,0,0,0.08]},"params":{"volume":0.4}}
```

Choose the actual track from readback; do not reuse an example index or revision blindly. Instrument and preset must match; preset recall runs before overrides. Read back all 16 levels with float tolerance and verify notes/routes remain as intended. Save/export/Undo use existing tools; resolve native note previews first. Current project writes use schema 25 and require app 0.65+, reading schemas 1-22 with neutral defaults for newly introduced fields; pre-17 synths gain neutral harmonic defaults. Additive sounds/custom harmonic data and libraries containing them or additive favorites require sound/library schema 3, reading 1-2; older-engine files without new harmonic data still write schema 2. Keep pre-update project/History backups for old apps. Harmony presets retain their separate schema 1/2 format. The tool count remains eleven.


The 0.56 bridge retries temporary Windows sharing locks while reading session metadata or the same response, within the original timeout. It submits each request once. A response-cleanup failure does not turn a known result into a failed mutation; a locked leftover response can remain in its session folder. Metadata timeout explicitly means no request was submitted. Response timeout can still mean an edit applied: inspect fresh status/project state before deciding what to do, and never blindly repeat a mutation.


App 0.57 adds native timing/velocity groove templates and extraction. TOOLS > Groove templates or Note tools > Groove opens selected/all-lane preview; Presets offers six factories, a separate 128-entry local library and .pulsegroove import/export. A 2-32-cell profile repeats from pattern beat 0 with per-cell offsets (-49..49% of grid), velocity targets and active masks; grid 0.0625-4 beats, cycle<=64 beats, timing/velocity strength 0-100%, optional straight-grid window 0-50% of a cell. Capture reads one explicit full cycle from original selected/source notes, averages offsets/velocities and leaves empty cells inactive. Apply validates PAT/Song event capacity and bakes changed note beats/velocities as one Undo; pitch, gate, probability/ratchets and source order remain. Global Song swing still applies on top. Native capture/catalog/cell editing are not new MCP operations: resolve any pending preview, then read fresh committed notes using pulse_get_project. Do not assume a profile is linked to notes or repeat an Apply blindly. Current project schema 18, sound/library schema 3, harmony schema 1/2, 64 instrument sounds and eleven tools stay unchanged. Groove files use their own schema 1 and contain no source notes/audio/capture range. Persistent per-pattern groove assignments, broader phase alignment, multichannel capture and third-party formats remain pending.

## Automation shapes and generators (0.58)

Create a clip by right-clicking a mixer volume, pan, cutoff, drive, send or master control and choosing **Create automation clip**. Double-click a Playlist automation clip to open the editor. **Shape...** sets the selected point's outgoing segment, or every segment, to **Linear**, **Smooth**, **Ease in**, **Ease out** or **Hold**. Tension **-1 to +1** warps the time within a segment without overshooting its endpoints; positive values delay movement, negative values advance it. Hold ignores tension and jumps at the next point. The existing STEP switch toggles Hold while retaining the other shape settings.

**Generate...** opens a compact draft graph for **Sine**, **Triangle**, **Square**, **Step sequence**, **Seeded random hold** or **Seeded random smooth**. Set period in beats (0.0625-600), cycle phase (0-1; 1 aliases 0), normalized minimum/maximum, even resolution 4-64, unsigned seed 0-4294967295 and 2-32 comma-separated step values. Minimum must not exceed maximum. Triangle and Square use two cells per cycle; Step sequence uses its entered list; sine and random modes use Resolution. Sine is sampled into linear segments. Random smooth joins its seeded knots with Smooth segments; truncated first/last segments are fitted between the generated endpoint values. Phase offsets the knot grid. Random modes produce new seeded knots across periods; Period controls their density rather than repeating a fixed random cycle. Resolution/steps controls irrelevant to a wave are retained for switching waves and still require valid values.

The graph previews the draft without changing playback, the song revision or Undo. **Apply** replaces only that clip's points as one Undo; **Cancel** keeps the original curve. A successful Apply remembers the controls for this app session. Generated points, including endpoints and shape metadata, are the saved musical result: they remain editable and reproduce the same seeded curve in playback, export, recovery and templates. Generator settings are not a persistent live modulation processor. Change the draft and Apply again to regenerate. Increase period or reduce resolution if the result exceeds 256 points; the whole project remains limited to 64 automation clips / 4096 points. Invalid values, recording, pending note previews and stale clip/revision callbacks retain existing data.

**TOOLS** and **Ctrl+P** also expose Automation segment shape and Generate automation curve. Song mode plays 126 channel/synth/effect/Master/auxiliary targets. Dedicated Touch/Latch passes are available; full multi-control Write/Trim recording, plugin/tempo targets, modulation routing and personal generator preset catalogs remain pending.

Automation segment metadata was introduced with **schema 18**; current writes use schema 23, reading schemas 1-22. Legacy segments gain neutral Linear/tension-zero defaults and retain their previous audio. Modern points save required `curve` (0 Linear, 1 Smooth, 2 Ease in, 3 Ease out), `tension` (-1..1) and `hold` (boolean); Hold takes precedence. Curved content cannot be mislabeled as an older schema. Keep old project/History backups for apps before 0.58.

The eleven MCP tools remain. App/server **0.58+** extends `add_automation` point fields and adds `set_automation_points` and `generate_automation` inside `pulse_edit`. Read a fresh project/revision and an existing automation clip index before editing. Replacement needs the complete point list, including both clip endpoints. Generator edits preserve the clip's position, length and linked parameter. Both operations use the native validation/Undo transaction and cannot open arbitrary files.

Read back points after generation and inspect the linked target and clip timing. Save/export/Undo use the existing tools. Generator periods and point beats use quarter-note beats, not bars or seconds. Reconcile a stale revision; never replay an edit blindly after a timeout.

Example operations, using a freshly read existing automation clip index (1 here):

```json
[{"op":"generate_automation","clip":1,"wave":"random_smooth","period":4,"phase":0,"low":0.2,"high":0.8,"resolution":8,"seed":12345}]
```

`wave` IDs are `sine`, `triangle`, `square`, `steps`, `random_hold`, `random_smooth`. Optional fields default to phase 0, low 0, high 1, resolution 16, seed 12345 and steps [0,1,0.25,0.75]. `wave` and `period` are required. For `set_automation_points`, supply `clip` and `points`; each point requires `beat` and `value`, with optional hold=false, curve=0 and tension=0. Missing endpoints, non-automation targets, invalid ranges/types and excess point counts reject the complete batch. With `add_automation`, the same optional shape fields apply.

## Inspect and navigate session history (0.59+)

Use `pulse_status(include_history=true)` when asked to find or restore an earlier edit. The dated `history.states` describe affected channels, patterns, clips and settings; `history.currentState` identifies the present checkpoint. To restore a chosen state, read a fresh revision/history, calculate the signed state difference, then call `pulse_undo` with that revision, `direction` undo/redo and positive integer `steps` (1-100). A zero difference requires no write. The whole move is one revision; normal Undo/Redo remains available afterward. Omitted direction/steps preserve the older one-Undo behavior. New history fields require the open app version 0.59+: the bridge rejects older/unknown versions before submitting, since they could ignore direction/steps.

Read back the resulting song before further changes. Do not reuse indices after a new edit or repeat navigation blindly after a timeout. A new edit clears the Redo branch; the hundred-edit journal is session memory and does not survive restart. Persistent snapshots use the separate project version library. Finish recording/previews/dialogs/exports before a write. See the reference's Session history section for returned fields, baseline time and examples. The native equivalent is EDIT > Project edit history / Ctrl+Alt+H; selecting a row only inspects it until Restore.

## Missing and renamed audio (0.60)

The native Project audio files window learns referenced-file fingerprints and searches a chosen local folder for exact-byte renamed copies. It updates shared sampler/Playlist paths as one Undo while retaining calibration/ranges/loops/offsets/fades. Collected SHA-256 names work without a local cache; ordinary paths need an earlier valid observation or use name-only proposals. Duplicates require explicit choice, incomplete scans cannot auto-apply unique matches, and changed candidates/recording/stale results reject. Identity cache observations stay outside the song and Undo; project schema 18 and eleven tools remain.

There is no new MCP file-search/load/relink operation. Do not invent a samplePath field for pulse_edit or write project JSON behind the app. If a native repair is needed, explain FILE > Project audio files / Search folder / review / Apply or Choose match. Once completed, call fresh pulse_status and pulse_get_project, inspect every shared source/range/tuning/clip and then continue supported edits/save/export. Cache or source files that are missing/unreadable do not prove an MCP transport bug. Re-encoded/retagged audio does not match the original raw-byte hash. Read the reference's Missing audio section for limits and cache behavior.

## Master processing and clear sample playback (0.61)

Click the **Master** name at the top of the Mixer strip, or search **Master processing** with Ctrl+P. This opens the actual shared delay return/feedback, master volume and soft saturation controls. **Dry master** bypasses saturation and sets the delay return to zero with one Undo. Channel/bus inserts, delay sends, notes, source files and feedback settings are retained. It does not remove reverb already recorded into a source file.

New projects bypass master saturation. A new sampler channel has zero delay send and bypassed EQ/compressor/reverb/chorus/flanger/phaser. Replacing the sound of an existing channel retains that channel's processing and routing; check its Mixer delay send and routed bus if it still sounds wet. Feedback zero gives a single echo when return/send are nonzero; set the return or that channel's send to zero for no echoes. The sampler still uses its amplitude envelope, channel filter, gain and pan. Master bypass allows unclipped floating-point levels, so lower the faders when the output peaks exceed full scale.

Browser/waveform preview remains available while a song plays when explicitly requested, but an earlier preview stops on Play, a played note, recording, or sample assignment. This avoids the original sample playing over its channel copy. Preview cancellation also works when its command queue is full.

Version 0.61 introduced **schema 19**, reading schemas 1-18. Current writes use schema 23 and require app 0.65+. Older projects retain the original master tanh saturation and 50% shared delay return on migration, so opening them does not silently remix them. Choose Dry master explicitly to hear an old project without that master processing, then save. These output controls were introduced in 0.61; 0.62 adds the independent Master insert chain described below.

MCP still has eleven tools. On app 0.61+, `pulse_edit` / `set_song` accepts `masterSoftClip` (boolean; new-song default false), `delayReturn` (0-1, default 0.5), and the existing `delayFeedback` (0-0.8, default 0.3). For a dry master use `{"op":"set_song","masterSoftClip":false,"delayReturn":0}` with a freshly read revision. `pulse_get_project` returns both new fields. They participate in the existing atomic edit, history, save, recovery and render behavior. Older apps reject these new edit fields; do not omit them and claim a dry master on an old app. Version 0.62 adds the six-effect Master chain described below; arbitrary plugin insert chains remain pending.

## Master insert effects (0.62)

In **F9 Mixer**, click **Master** and choose **Insert effects...**, or click one of the six slots in the Master sidebar. The existing EQ, compressor, reverb, chorus, flanger and phaser editors now work on an independent Master chain. **Master stereo tools** adds width/mono/swap/input-polarity/output-trim processing after those six effects. Master compression uses an internal stereo-peak detector and has its own gain-reduction meter. Channel selection and the Master target stay separate; typed dialogs, slot menus and chain-load callbacks reject a changed target or stale revision.

Right-click a Master slot to reorder it, use **Chain...** for the eight factory chains, copy/paste between Master/channels/buses, or save/load the existing `.pulseeffects` files. Chain recall retains the destination's stereo utility, routing and sidechain settings; stereo controls remain separate. Each accepted change is undoable. **Dry master** bypasses all Master inserts, its stereo utility and soft saturation and mutes the shared delay return in one edit, retaining effect values/order and every channel/bus setting. Undo restores the complete Master state.

Signal order is channel/bus processing and routing → shared delay return plus metronome → Master inserts in their saved order → Master stereo → Master volume → optional soft saturation. Live playback, input monitoring and offline exports use the same DSP path. Raw recorded WAV takes remain dry. Explicit Browser/waveform preview stays at original-speed dry audition gain outside the Master chain. Effects are fixed, one instance of each; third-party plugins, duplicate instances, Master sidechain routing, latency compensation and effect-parameter automation are still pending.

New projects bypass all Master inserts and stereo tools. **Schema 20** embeds the full Master chain and stereo state and reads schemas 1-19 with neutral, bypassed Master insert defaults, preserving their previous sound. Existing 0.61 master saturation/delay settings retain their meanings; older pre-0.61 songs still migrate with their original saturation/return. Master insert fields were introduced in app 0.62; current schema 23 files require app 0.65+. Keep pre-update backups for older executables. Complete and channels/mixer templates, history, recovery, collected saves and portable ZIPs retain Master state.

MCP retains eleven tools. Restart the native app **and the MCP server** after updating; a running older server can reject newly supported fields even when a newer app is open. In `pulse_edit`, `set_song.masterEffects` is a **partial edit object** with `eq`, `compressor`, `reverb`, `chorus`, `flanger`, `phaser`, `stereo` and `order`. The same groups now work in `set_track.effects`. Omitted groups/fields retain their values. A supplied group enables its processor unless `enabled:false` is explicit; `order` only changes order and enables nothing. It is exactly six unique integers 0-5, identifying EQ/compressor/reverb/chorus/flanger/phaser. Master has no output or external sidechain route.

Compressor fields: threshold -60..0 dB, ratio 1..20, attack 0.1..100 ms, release 5..1000 ms, makeup -12..24 dB. Flanger/phaser fields: enabled, mix 0..1, rate 0.02..5 Hz, depth 0..1, centre (0.2..8 ms flanger / 100..4000 Hz phaser), feedback -0.85..0.85, width 0..1. Existing EQ/reverb/chorus/stereo ranges stay unchanged. Reject unknown fields and keep fresh revisions. Readback is the **serialized model**, not the edit shape: `project.masterEffects` contains `chain.settings` (ordered six effects), `stereo`, and fixed -1 output/sidechain fields. Do not send that complete readback wrapper as an edit object.

Example operations in a fresh-revision `pulse_edit` batch:

```json
[{"op":"set_song","masterEffects":{"eq":{"low":-1,"mid":0,"high":1},"compressor":{"threshold":-18,"ratio":2,"attack":25,"release":180,"makeup":0},"order":[0,1,2,3,4,5,6,7]}}]
```

For a fully dry Master through MCP, set masterSoftClip false and delayReturn 0 and explicitly bypass eq/compressor/reverb/chorus/flanger/phaser/stereo in masterEffects. This preserves their values and source/bus processing. Read back, save and verify asynchronous export completion as usual. Master-level automation still uses existing target 5; it does not automate the inserts.

## Conditional notes (0.63)

Select notes in Piano roll and use Channel settings > **Repeat: Always...**, or search **Note repeat condition** in the command palette. **Alt-right-click** an enabled Channel rack step or a Piano roll note opens quick choices (Always, first/last of two/four/eight); Custom accepts a cycle of 1-16 repeats and a pass within that cycle. A group selection changes together in one Undo. A rack step targets all notes displayed in that cell, including off-grid/chord notes. Conditional notes show a pass/cycle badge or amber indicator. Ordinary right-click deletion and Shift-click ratchets retain their behavior.

On app/server 0.63+, each `set_notes.notes` object optionally accepts `conditionEvery` (integer 1-16, default 1) and `conditionStep` (integer 1-conditionEvery, default 1). Example for an existing track/pattern, using a freshly read revision:

```json
{"op":"set_notes","pattern":0,"track":7,"notes":[{"beat":0,"length":0.25,"pitch":60,"velocity":0.8,"probability":1,"ratchets":2,"conditionEvery":4,"conditionStep":4}]}
```

`set_notes` replaces that whole pattern/channel lane; include any notes you intend to retain. An omitted condition means Always; it does not inherit the replaced note's condition. Invalid pass/cycle combinations reject the complete edit batch. `pulse_get_project` returns both fields. Standard save/recovery/templates, note copying, transformation tools, Undo/Redo and offline render retain them. Finish native takes/previews and read the revision again before writing. Restart the updated MCP server to refresh cached tool schemas.

- **PAT:** pass = completed pattern loops + 1. Play from Stop and seek reset counting to pass 1; pause/resume retains the current count. A nonlooped PAT export evaluates its first pass, so a fourth-pass-only note is silent there. To export a full variation cycle, arrange that pattern across the desired Song length.
- **Song:** the zero-based pass is `floor(patternRepeatStartBeat / sourcePatternLength)`, counting from song beat 0, not from each clip. For a four-beat pattern, pass 4 occurs at beats 12-16, then 28-32. Separate clips share the same grid; a clip starting off that grid uses the pass containing its repeat start. All ratchets retain that parent repeat's condition, including sub-hits crossing a pattern grid boundary.
- **Song seeks/section loops:** the decision stays attached to Song time. Looping the fourth pass keeps playing it; looping an earlier pass does not advance that condition. This makes playback, held-note chase and export consistent. Changing source pattern length or moving clips can change their chosen pass.
- **Chance** is evaluated in addition to the condition. Existing deterministic probability behavior is retained, including its per-ratchet decisions. Conditional evaluation itself requires no callback allocation. Live typing/audition remains available independently of scheduled conditions.
- **MIDI export** bakes accepted timeline conditions into ordinary MIDI notes; the MIDI file has no conditional metadata. Existing probability/ratchet MIDI-export limitations remain. Conditional group exclusion, a manual fill switch and first-pass-only conditions are still pending.

Conditional notes were introduced in **schema 21**, requiring app 0.63+. Current writes use schema 23 and require app 0.65+. Schemas 1-20 load missing conditions as Always without changing prior audio. Schema 21 requires both fields on every stored note; malformed fields and conditional notes mislabeled as an older schema reject atomically. Keep pre-update `.pulse.bak`/History backups for older executables. Master processing introduced in schemas 19/20 retains its existing behavior.

## Dynamics modes (0.64)

F9 > a channel, bus or Master > the existing Compressor insert > **Mode** chooses Compressor, Peak limiter, Noise gate or Downward expander. The tab is now labelled Dynamics; it remains the same stable insert identity 1 in the six-effect chain. Its enabled state, position, presets, clipboard, local chain files, Undo and rendering work across all four modes. Right-click the visible knobs to type actual dB, ratios or milliseconds. Mode changes retain stored values and do not enable a bypassed insert automatically. One mode runs per insert; to combine them, route a gated source through a compressor bus and then a limited Master.

- **Compressor:** existing threshold -60..0 dBFS, ratio 1..20, attack 0.1..100 ms, release 5..1000 ms, makeup -12..24 dB. Knee adds 0..24 dB of smooth transition around the threshold. Default knee 0 retains the original transfer/audio path.
- **Peak limiter:** threshold is its sample-peak ceiling (-60..0 dBFS); makeup becomes input trim (-12..24 dB). Internal linked stereo detection, immediate reduction and release 5..1000 ms preserve the L/R ratio and bound the output of this insert. Attack/ratio/gate controls are inactive but retained. It has zero lookahead and zero added latency. This is not a true-peak/oversampled limiter. Later effects, stereo tools, channel/Master gains or a separate raw preview can exceed this insert's ceiling. For a final Master ceiling, put Dynamics last in the chain, keep downstream stereo/gain neutral, and export without normalization that raises the result.
- **Noise gate:** opens at threshold, closes below threshold minus hysteresis (0..24 dB), waits Hold (0..1000 ms), then releases toward Range (0..96 dB attenuation). Attack opens the gain; release closes it. Range is a finite attenuation floor, not a forced digital mute. Ratio/knee are retained but inactive.
- **Downward expander:** below threshold, attenuation is `(threshold - detectorDb) * (ratio - 1)`, bounded by Range. Ratio 1 is neutral after gain settles; stronger ratios suppress quiet material more. Attack/release smooth gain changes. Hysteresis/Hold/Knee are retained but inactive.

Source and bus Compressor/Gate/Expander can use the existing external detector routing (one-sample delayed peak). Master always uses its internal stereo detector. Limiter always uses the actual internal stereo signal after input trim, even when a stored source detector route exists. All modes expose their own insert's gain reduction through the existing meter. Factory chains now include **Peak ceiling**, **Quiet gate**, and **Gentle expander** (eleven chains total).

MCP keeps eleven tools. In a revision-checked `pulse_edit`, `set_track.effects.compressor` and `set_song.masterEffects.compressor` add `mode` (string `compressor`, `limiter`, `gate`, `expander`), `range` 0..96 dB, `hysteresis` 0..24 dB, `hold` 0..1000 ms and `knee` 0..24 dB. Defaults: compressor, 60, 6, 50, 0. Existing threshold/ratio/attack/release/makeup/enabled fields retain their units and meanings; supplying a group enables it unless `enabled:false`. Omitted properties retain their values. Example for an existing Master:

```json
{"op":"set_song","masterEffects":{"compressor":{"mode":"limiter","threshold":-1,"makeup":0,"release":100},"order":[0,2,3,4,5,1,6,7],"stereo":{"enabled":false}},"master":1,"masterSoftClip":false}
```

Read a fresh revision, inspect existing state, and preserve intentional downstream settings rather than copying example values blindly. Read back `project.masterEffects.chain.settings.dynamics` or `project.tracks[index].mixer.dynamics`: serialized modes are integers 0 compressor, 1 limiter, 2 gate, 3 expander, with numeric range/hysteresis/hold/knee. The persisted shape differs from the partial MCP edit shape. Invalid fields reject the whole batch; native dialogs also reject stale revisions, changed targets, recording and pending note previews.

Current writes use **project schema 23**, reading schemas 1-22 with legacy compressor/knee-zero defaults. New effect-chain exchange files use **schema 2**, reading schema 1 with the same defaults. Old projects/presets retain their audio; new modes cannot be mislabeled as old schemas. Templates, collected saves, autosave/recovery, version snapshots and full exports retain the state. Keep pre-update backups for older executables and restart the app/MCP server after updating. Native status, MCP metadata and runtime reports now derive the release version from CMake rather than retaining stale 0.60 labels.

Sidechain filtering, optional lookahead, oversampling/true-peak measurement, and smoother bypass transitions remain pending; the overall dynamics roadmap is still partial.

MCP export level (0.64+): `pulse_export_audio` optionally accepts `normalize` (boolean, default true). Set **`normalize:false`** to preserve mix/limiter levels in the existing 48 kHz/24-bit dithered WAV with a three-second tail; this overrides no project or native-export preference and still requires a fresh revision. Omitting the flag retains the original -1 dBFS peak-normalized export. The updated bridge refuses this explicit option on older apps rather than silently letting them ignore it. Poll until exporting=false and inspect completion before using the file.

## Channel microtonal tuning (0.65)

Right-click a pitched channel in the Channel rack and choose **Microtonal tuning...**, use Piano roll's **Tuning** button (wide windows) / pane menu, or search **Channel microtonal tuning** with Ctrl+P. Each source has an independent complete 128-key table; buses and synthesized drum voices have no pitch-table controls. Factory sound changes preserve channel tuning. **Edit all keys / reference A4** opens a draft: choose any key, edit its offset, move to another key to retain the draft value, set the table name/reference and enable/bypass, then Apply once. Cancel discards the whole draft. Invalid values and stale target/revision, recording or unresolved note-preview contexts cannot commit. Unchanged settings add no Undo. Enable toggles only bypass; Reset explicitly replaces the table with standard 12-TET/A4=440.

Offsets are **-24000..+24000 cents relative to ordinary MIDI 12-TET**, reference A4 **220..880 Hz**. Target frequency is `440 * 2^((key-69)/12 + cents[key]/1200) * referenceHz/440`; the reference factor and offsets only apply when enabled. Negative cents lower pitch. Piano roll keeps the ordinary note name as the key's MIDI position, widening keys to show offset (including reference shift) and target Hz; keyboard layout, stored note numbers, scale guides and octave/transpose tools remain semitone/MIDI-position tools. In non-12 scales twelve keys need not span a sounding octave. Oscillators retain their existing high-frequency/Nyquist limits; extreme mappings cannot reproduce arbitrary ultrasonic pitches. Existing held voices retain tuning captured at onset; release/retrigger or Stop/Play after editing. Sampler root and fine tune describe source calibration and combine with the requested target pitch; microtuning also adjusts held-sample seek/wrap positions. Playlist audio, monitoring and original-speed Browser preview are unaffected.

**Equal divisions of octave** creates a reviewable draft with **5..72 divisions**, root **MIDI 0..127** (default C4/60), and reference frequency. Each next MIDI key advances one scale degree, so 24 EDO uses 24 keys per octave. The root retains its ordinary reference-adjusted 12-TET pitch. **Import Scala / tuning preset** reads local **.scl** scales (1..128 entries, 64 KiB maximum) containing ratios, integer ratios, decimal cents, comments and trailing interval labels, with negative/below-root distances and non-octave repeating periods. Intervals are bounded to +/-4800 cents, with a positive final period; the baked table must fit the per-key bounds. Unordered intervals are retained. Empty description becomes Imported Scala scale. Scala imports use the same adjacent-degree/root/reference mapping; `.kbm` keyboard maps, unmapped keys, MTS/MPE and tuning plugins remain pending. Format reference: [Scala scale format](https://www.huygens-fokker.org/scala/scl_format.html).

**Save tuning preset** writes a verified atomic local `.pulsetuning` (separate schema 1), including name, reference, all 128 offsets and bypass state. Import opens a draft for review; preset mappings are already baked and ignore the Scala-root fields. Tables are embedded in **project schema 23**, requiring app 0.65+, without dependence on the source Scala/preset file. Schemas 1-22 migrate to neutral bypassed tables and retain prior audio; nondefault tuning cannot be labeled as an older schema. Full/channel templates, collected/portable saves, recovery, versions, Undo/Redo, callback playback, WAV/FLAC export and separate tuning presets retain them. Instrument `.pulsesound` files keep their existing tone-only format; use `.pulsetuning` to share the independent mapping. Ordinary MIDI export preserves note numbers and **does not encode microtonal tuning or pitch bends**; render audio to preserve the sound.

MCP retains eleven tools. Restart **both app and server** after updating. `set_track` / `add_track` accept optional **tuning**, a complete replacement object, not a partial merge: `enabled` boolean, `name` nonblank printable string up to 100 characters without surrounding spaces, `referenceHz` 220..880, and `cents` exactly 128 finite numbers -24000..24000. Omit the object to preserve an existing track's tuning. Buses reject tuning edits; drum voices ignore pitch tables. A single batch is atomic and takes a fresh revision from readback. Read back `project.tracks[index].tuning` to verify every value. Scala/file import is native UI, not a new MCP path-reading operation. Other models can compute the complete offsets and send them through the existing edit tool:

```python
tuning = {"enabled": True, "name": "Quartertones from C4", "referenceHz": 440,
          "cents": [(key - 60) * (1200 / 24 - 100) for key in range(128)]}
# Read the current revision and choose the actual pitched channel index first.
operations = [{"op": "set_track", "track": 7, "tuning": tuning}]
# pulse_edit(expected_revision=<fresh revision>, operations=operations)
# pulse_get_project() -> verify project.tracks[7].tuning, then audition/export.
```

## Editor timing and tool cursors (0.65.1)

**Ctrl+C / Ctrl+V** in Piano roll now preserves the exact source beat positions by default, both within a pattern and between patterns/channels. This includes initial silence and unsorted/chord note timing; shortening a pattern from 64 to 32 steps does not scale the retained notes. Paste can extend a short destination to fit, up to the existing 64-beat/512-note limits, and remains one Undo. **Ctrl+Shift+V** retains its original-position behavior. To intentionally shift a phrase, click the Piano-roll ruler or grid and choose **EDIT > Paste notes at cursor**, or search that command with Ctrl+P. **Paste notes at start** aligns the first copied onset to zero. Hovering alone leaves the Piano-roll edit cursor unchanged. Copied Playlist clips still paste at the Playlist cursor, and waveform/automation clipboard behavior is unchanged.

Changing a pattern's step count resets the Piano-roll view and edit cursor to its start. In PAT mode, changing length or selecting another pattern also restarts its playback from zero rather than inheriting the previous pattern's phase. In Song mode these editor operations keep the Song transport running where it is. Piano-roll and rack playback indicators now follow the selected pattern's actual Playlist instances on the displayed source channel, using `(Song beat - clip start) modulo pattern length`; they are hidden before/after those instances or while another pattern/channel is playing. Off-grid starts, repeated/partial instances and distinct overlapping phases are supported. PAT mode retains its own pattern clock.

Editor pointers distinguish **Draw (pencil)**, **Paint (brush)**, **Delete (red cross)** and **Select (dashed selection rectangle)** on both Piano roll and Playlist. Select remains visibly marked over notes and block ends. Draw/Paint show horizontal resize arrows over note/clip ends and a move hand over bodies. Ctrl's temporary selection cursor and keyboard tool switches update without requiring another pointer movement. Other controls retain their knob/window cursors. No project schema or MCP operation changes are introduced in this patch.


## Physical sound design (app/server 0.66+)

The eleven tools now accept instrument `physical` (readback ID 13, native name Pulse Resonator). `pulse_list_sounds` returns 72 factory sounds, including eight Resonator presets. Existing first-64 names/IDs remain stable. `set_track` and `add_track` accept a factory `preset`, then optional `synth.physical` overrides. The `physical` object replaces the complete seven-field group: never send only one nested field. Omission preserves it. Read the current values before changing just one control.

```json
{"op":"set_track","track":7,"instrument":"physical","preset":"Cedar string","synth":{"physical":{"mode":0,"decay":5,"damping":0.3,"brightness":0.8,"position":0.25,"body":0.4,"stiffness":0.1}},"params":{"attack":0.001,"release":1.5,"send":0}}
```

Wrap this operation in `pulse_edit` with a freshly read `expected_revision`. Mode is integer 0 plucked string or 1 struck free bar; decay is 0.1–20 seconds T60; damping/body/stiffness are 0–1; brightness 0.05–1; position 0.05–0.95. All seven fields must be finite numbers of the declared types. Stiffness affects strings only. Higher damping shortens upper-mode decay; brightness changes excitation, position suppresses modal nodes, body blends three fixed resonances. Each note excites once; sustained notes naturally fade. Longer ADSR release permits the natural tail after note-off. Shared `coarse`, filter, ADSR/LFO and channel microtuning work; oscillator mix/detune/waves/sub/noise are unused by this engine. Do not describe it as a bowed/wind model or realistic recorded instrument.

Native Browser/search/generator, favorites/personal library, sound exchange, templates, recovery, save and export retain the settings. Projects write schema 24, reading 1–23; physical sounds/libraries use schema 4, reading 1–3. Older instrument data retain sound/library schema 2 or 3 when possible. Restart both app and server after updating. Read back `instrument:13`, all model values and note lanes; save and verify export completion using the ordinary guarded workflow. The broader DAW roadmap remains unfinished.


## Local command batches and read-only edit previews (0.67+)

The tool count stays eleven. `pulse_edit` now accepts optional boolean `preview` (default false). With `preview:true`, the native app validates and compiles the entire candidate, returning `preview:true`, `candidateProject` and `resolvedCommands`; it does **not** publish the candidate, change revision/dirty state, create Undo, write a backup or save/export anything. The ordinary current revision and busy guards still apply. The bridge refuses to submit preview requests to apps older than 0.67 because they would ignore the field and could commit the edit. Update/restart both app and stdio server.

A new operation `{"op":"project_commands","text":"..."}` accepts the native bounded command grammar. It can be combined with ordinary structured operations; targets resolve against the preceding candidate state. At most sixteen text commands are permitted across the entire atomic edit, even when split across operations. Each text operation is at most 4096 characters. Blank lines are ignored. Keywords and names match case-insensitively; name matching is exact and duplicates reject. Names use double quotes, with `\"` and `\\` for literal quotes/backslashes. Numbers **inside text start at 1**; structured operation indices remain **zero-based**. Destinations and pattern lengths use quarter-note beats; destinations start at beat 0.

Recommended guarded sequence:

1. Read `pulse_status` and, for an existing song, `pulse_get_project`.
2. Call `pulse_edit` with `preview:true`, the latest `expected_revision`, and the intended operations.
3. Inspect every `resolvedCommands` line and `candidateProject`. A dry run is not a completed edit. Errors do not apply anything.
4. Read status again. If its revision differs from the preview revision, create and inspect a new preview first. Otherwise apply the same operations with that revision and `preview:false` (or omit preview). A conflict requires a fresh preview of the changed project; do not blindly retry.
5. Read back, then save/export through the normal separate tools. One successful batch is one Undo; text is not needed to reopen the saved project.

```json
{
  "expected_revision": 123,
  "preview": true,
  "operations": [
    {
      "op": "project_commands",
      "text": "set tempo to 108 bpm\nrename track 8 to \"Strings\"\nuse sound \"Cedar string\" on track \"Strings\"\nlower track \"Bass\" by 2 db\nset track 8 volume to 50 percent\ntranspose track 8 in pattern 1 up 3 semitones"
    }
  ]
}
```

Use the grammar and limits below. Preview also supports ordinary structured operations; `resolvedCommands` describes text-command actions only and can be empty for a structured-only batch. Always inspect the candidate for those operations. `set_track`/`add_track` additionally accept boolean `mute` and `solo`; existing source/bus rules still apply. Preview is read-only even for a no-op, and returns its candidate before the ordinary no-op commit shortcut.

### Native command grammar

Open **TOOLS > Local project commands...**, or find it with Ctrl+P. Enter one command per line. **Preview** resolves names and shows the actual track numbers, changes and destinations; **Apply** commits the entire batch as one Undo. Cancel leaves the song unchanged. Back returns to your text. A changed project, recording, note preview or export prevents applying an old review.

Examples:

```text
set tempo to 108 bpm
rename track 8 to "Strings"
use sound "Cedar string" on track "Strings"
lower track "Bass" by 2 db
set track 8 volume to 50 percent
transpose track 8 in pattern 1 up 3 semitones
```

This is a local constrained command parser, with case-insensitive keywords and exact case-insensitive names. Track and pattern numbers inside text start at **1**. Use double quotes around names, including names containing spaces; escape a literal quote as `\"` and a backslash as `\\`. Duplicate names reject rather than selecting the first match. Commands resolve in order, so a later command can use a name assigned earlier in the same batch. One invalid line rejects everything and reports its line number.

The supported forms are:

```text
set tempo to 120 bpm
set loop on
set metronome off
set track 8 volume to 60 percent
set track 8 pan to -20 percent
set track 8 send to 10 percent
set track 8 cutoff to 4000 hz
lower track "Bass" by 2 db
raise track 8 by 1 db
mute track 2
unmute track 2
solo track 8
unsolo track 8
rename song to "My song"
rename track 8 to "Strings"
rename pattern 1 to "Verse"
set pattern 1 length to 8 beats
use sound "Cedar string" on track 8
transpose track 8 in pattern 1 up 2 semitones
transpose track 8 in pattern 1 down 2 semitones
copy section "Chorus" to beat 32
move section "Chorus" to beat 32
```

Limits: 16 nonblank commands per atomic edit, 4096 characters per text operation. Tempo 30–300 BPM (`bpm` optional); volume 0–150 percent, pan -100–100, send 0–80, cutoff 60–20000 Hz. Relative level changes use positive 0–96 dB and reject results above the channel's maximum. Pattern lengths are 1–64 quarter-note beats and cannot exclude existing note onsets. Transpose accepts integer 0–48 semitones and rejects any result outside MIDI 0–127, preserving every other note field. These are MIDI key-number steps; channel microtuning still determines the sounding intervals. Section destinations are zero-origin quarter-note beats; the existing whole-clip boundary, capacity and overlap guards apply. Sections require unique names; rename a duplicate first. Sounds come from the original factory Browser.

The parser operates offline through normal validated project edits. It does not interpret arbitrary prose, run code, access arbitrary files, export audio, start transport, invoke an LLM, or change the project schema. Use the normal export and transport controls. The local MCP API supports the same commands and an explicit read-only edit preview; see the guarded sequence above. Wider language coverage and optional local language models remain future work.



Pulse Granular was removed in 0.79. There are 72 factory sounds. Do not request instrument `granular` or `synth.granular`; both are rejected. Read the removal/compatibility section in the reference for legacy projects and hidden catalog preservation.


## 0.80 Delay and Distortion (current)

Restart both app and server. Twelve tools remain. Current projects write schema **28**; supported older schemas through **27** remain readable. Ordered insert IDs are now `0 EQ, 1 Dynamics, 2 Reverb, 3 Chorus, 4 Flanger, 5 Phaser, 6 Delay, 7 Distortion`. An authored `order` must contain all eight unique IDs exactly once. Saved six-slot legacy orders append bypassed IDs 6 and 7.

`set_track.effects.delay` and `set_song.masterEffects.delay` accept partial objects: `enabled` boolean; `repeats` integer 1–16; `interval` milliseconds 20–1000; `fade` fraction 0–1; `mix` fraction 0–1. Supplying an effect object defaults `enabled` to true unless explicitly false, preserving unspecified numeric values. Each echo is spaced by `interval`; the first has unit wet gain and repeat k has gain `(1-fade)^(k-1)`. The dry/wet crossfade is controlled by `mix`. No extra feedback repeats occur. Count, interval or bypass changes clear old echo history. At 120 BPM 250 ms is an eighth note. Set the export tail to at least count × interval if the last source sound is at the export boundary; maximum echo span is 16 seconds.

`distortion` accepts `enabled` boolean and `mode` integer **0 Soft/tube, 1 Hard clip, 2 Fuzz** plus `drive` dB 0–48, `lowCut` Hz 20–1000, `tone` Hz 800–12000, `presence` dB -12–12, `asymmetry` 0–1, `cabinet` 0–1, `mix` 0–1 and `output` dB -24–6. Cabinet is a colour filter, not an IR loader. For a guitar-like tone, choose `physical` preset `Cedar string` or a native-loaded guitar sample, then apply these settings. This changes processing, not the source instrument or notes. Reorder Distortion before Delay for clean repeats of the distorted source. The legacy `params.send`, `delayReturn` and `delayFeedback` still control the old shared ping-pong effect independently; set sends to zero when only the new inserts should be heard.

Example operation (use a fresh actual revision with `pulse_edit`):

```json
{"op":"set_track","track":7,"effects":{"order":[0,1,2,3,4,5,7,6],"distortion":{"enabled":true,"mode":0,"drive":22,"lowCut":100,"tone":5500,"presence":2,"asymmetry":0.15,"cabinet":0.8,"mix":1,"output":-8},"delay":{"enabled":true,"repeats":4,"interval":250,"fade":0.4,"mix":0.25}},"params":{"send":0}}
```

Read the project back to verify the resulting `tracks[7].mixer.delay` / `.distortion` and order; Master settings appear inside `project.masterEffects.chain.settings`. Partial edits, preview, strict validation and one Undo per committed batch follow the existing contract. There is no new sample-loading tool. Granular remains removed.


## 0.81 parameter automation (current)

The native app and server must both be 0.81.0+. Current projects write schema **29**, reading older supported songs through schema 28. The existing twelve tool names remain. IDs 0–5 keep their original meaning. No tutorials are being written yet; the Windows release gates and final tutorial step are in `V1_RELEASE_PLAN.md`.

Call `pulse_get_project` and read **automationParameters** before constructing an unfamiliar curve. It is a 101-entry list with `id`, stable `key`, `name`, `group`, `minimum`, `maximum`, `unit`, `logarithmic`, `discrete` and `master`. Synth entries also list applicable numeric `instruments`: 9 Analog, 10 FM, 11 Wavetable, 12 Additive, 13 Resonator; amplitude fields can also apply to legacy pitched/sample sources 5–8. Engine-specific curves are retained but dormant when the selected instrument does not use the field. Synth targets cannot address buses or Master.

| IDs | Parameter family |
| --- | --- |
| 0–5 | Existing volume, pan, tone filter, channel saturation, legacy send, Master volume |
| 6–9 | `delay.repeats`, `delay.interval`, `delay.fade`, `delay.mix` |
| 10–17 | `distortion.drive`, `.lowCut`, `.tone`, `.presence`, `.asymmetry`, `.cabinet`, `.mix`, `.output` |
| 18–28 | EQ low/mid/high; compressor threshold/ratio/attack/release/makeup; reverb mix/room/damping |
| 29–34 | Chorus mix/rate/depth/delay/feedback/width |
| 35–40 | Flanger mix/rate/depth/centre/feedback/width |
| 41–46 | Phaser mix/rate/depth/centre/feedback/width |
| 47–48 | Stereo width/trim |
| 49–52 | Dynamics range/hysteresis/hold/knee |
| 53–100 | Existing 48 synth editor parameters: shared oscillator/envelope/filter/LFO/FM/wavetable values, sixteen harmonics and seven physical-model fields |

`add_automation.target` accepts the discovered stable **string key** or numeric ID 0–100. Saved projects store numeric IDs. `track` remains the zero-based **Playlist lane**. `targetTrack` is the zero-based **processing channel**, independently of the lane; use **-1** for Master volume or Master-capable effect values (IDs 6–52). Source-only targets 0–4 and synth targets 53–100 cannot address Master. Unknown keys, fractional IDs, invalid destinations, malformed curves and budget overflow reject the entire edit batch.

Point values remain normalized **0–1**, not Hz/dB/ms. For a descriptor `(low, high)`, a linear real value is `low+(high-low)*value`; logarithmic is `low*(high/low)^value`. Discrete values are rounded to integer real units; use held/step segments for deliberate count/family/model changes. Existing point curves/tension, replacement and deterministic generation work unchanged. Supplying a curve does not enable a bypassed effect or replace the static patch. Enable the required effect separately with `set_track.effects` / `set_song.masterEffects`. Song playback evaluates the curve; outside its span and after Stop, the saved static value is restored. Later overlapping Playlist entries win for the same destination.

Example operations (use one fresh `expected_revision`, preview if desired):

```json
[
  {"op":"set_track","track":7,"effects":{"distortion":{"enabled":true,"drive":18}}},
  {"op":"add_automation","track":7,"target":"synth.cutoff","targetTrack":7,"beat":0,"length":8,"points":[{"beat":0,"value":0.9},{"beat":8,"value":0.25,"curve":2}]},
  {"op":"add_automation","track":6,"target":"distortion.drive","targetTrack":7,"beat":0,"length":8,"points":[{"beat":0,"value":0.2},{"beat":8,"value":0.75}]},
  {"op":"add_automation","track":5,"target":"reverb.mix","targetTrack":-1,"beat":0,"length":8,"points":[{"beat":0,"value":0.1},{"beat":8,"value":0.3}]}
]
```

During Song playback `pulse_status` / `pulse_get_project` include **automationReadouts**, a list of currently active values with `target`, `key`, destination `track` (-1 Master), normalized `value` and real-unit `actualValue`. These are callback observations, not committed project edits. Look up the corresponding descriptor's `unit`. The list clears when stopped; the serialized project keeps static settings plus the curve. Native knob readouts use the same values. Read revised project/revision after a successful edit; do not write project JSON behind the application.

Synth parameter targets are evaluated per sample and retain the existing voice control/smoothing behavior. Cached effect updates run on a fixed 16-sample control grid, independent of callback size. Automated delay count/time changes preserve history, and interval sweeps interpolate/smooth the reads, with the expected pitch changes. Manual delay edits keep the earlier clear-history behavior. Live/offline rendering shares this path. The existing 64-clip / 256-point-per-clip / 4096-total-point limits remain. Waveform/bypass/mode selectors and dedicated Touch/Latch passes are available. Multi-control Write/Trim recording, tempo maps and plugin parameters are still pending; do not invent unsupported targets.

## Decoded audio memory status (0.82)

`pulse_status` and `pulse_get_project` include `decodedAudioCache`: `limitBytes`, `residentBytes`, `pinnedBytes`, `reclaimableBytes`, `availableBytes`, `files`, `evictions`. These are decoded stereo-float cache bytes, not total process memory or raw file sizes. `residentBytes = pinnedBytes + reclaimableBytes`; `availableBytes = limitBytes - pinnedBytes`. Unused least-recently-used sources are evicted on native loads under pressure, without deleting source files. Playing/pending snapshots, open editors and queued/active previews retain immutable leases. Prior playback snapshots can remain briefly pinned during publication. No new MCP write command, permission or schema is required. Continue reading revision before musical writes. Native app 0.82+ is needed for these optional status fields; earlier apps omit them. Samplers/editors retain the decoded budget and synchronous loads; native 0.84+ streams long Playlist audio (see below).

## Audio callback/device diagnostics (0.83)

Native 0.83+ adds read-only `audioHealth` to `pulse_status` / `pulse_get_project` and `data/runtime.json`. Earlier apps omit this optional block. No new tool/write operation is added. Fields: schema=1, consistent, running, measurement, resetPending, deviceStarts/deviceStops, callbacks, frames, deadlineMisses, lateCallbackStarts, estimatedMissingBlocks, invalidBlocks, clockErrors, silentCallbacks, lastCallbackMs, meanCallbackMs, worstCallbackMs, meanUtilizationPercent, worstUtilizationPercent, maximumStartGapMs, backend, device, sampleRate, bufferSize, callbacksStalled, inputLatencySamples/outputLatencySamples, inputLatencyMs/outputLatencyMs, outputReady, driverXRunsSupported, driverXRuns (integer or null), managerXRuns, asioCompiled=false and lastInterruption.

Timing measures the complete application callback (including capture/monitoring and DSP), not external driver/hardware latency. Deadline misses use actual callback frame count / sample rate; late starts use >1.5 previous-block periods. Missing intervals are capped estimates, never confirmed driver dropouts. Native driver xrun support is explicit; JUCE managerXRuns also includes JUCE's timing measurer. Driver-reported latencies are not loopback measurements. If consistent=false, read again; snapshot retries are bounded. Measurement resets take effect on the next callback/device start and retain driver/session lifecycle counters. Each device preparation starts a fresh measurement. Native F10 / command search exposes diagnostics, reset, explicit device restart and atomic Save to workspace/data/audio-diagnostics.json. MCP has no driver-configuration command.

On device stop/change, native playback/monitoring/history disarm. Captured takes are finalized on the UI thread; keyboard endpoints use the pre-reconfiguration cursor. Restart/reconnect requires explicit Play, never automatic rearming. Re-read status/revision after any interruption because completing a take may change the project. Finish device recovery in the native UI if outputReady=false; do not keep issuing Play blindly. Broader streaming qualification, ASIO and full hardware/physical-failure qualification remain pending.


## Long Playlist streaming diagnostics (0.84)

Native 0.84+ streams Playlist sources over 16 MiB decoded stereo size, and short clips when decoded capacity is pinned. Already decoded sources can be reused. Samplers/editors still use the 512 MiB decoded pool. Streaming has independent six-page cursors per clip/source offset, one background read-ahead/thumbnail worker and a shared 128 MiB page/metadata reservation; reader/codec/process overhead is outside that reservation. Existing 30-minute, 8–192 kHz source and 4096-beat timeline limits remain. Portable archives/fingerprinting retain their separate bounds. No new MCP tools, file permissions or song schema changes are introduced.

The optional `audioHealth` schema-1 block adds integer `streamingSources`, `streamingBufferBytes`, `streamingBufferLimitBytes` (=134217728), `streamingMissedFrames` and `streamingReadErrors`. The first two include all currently leased real-time/private offline cursors; reads can observe concurrent worker cleanup. Missing frames and errors are monotonic workspace-session counters, retained by Reset measurements; they are unrelated to driver xruns. An unavailable source frame contributes silence while background reads catch up. Failed reads or observed source size/mtime changes at read boundaries mark a cursor unavailable; reopen/republish its source to retry, then inspect diagnostics. Do not claim gap-free arbitrary seeks or sustained hardware qualification from these counters alone.

Offline exports use separate blocking cursors, including normalization/range warm-up, and do not count live read-ahead misses. Source read failure retains the prior export destination. Header/open and initial read-ahead happen on the native control thread; subsequent real-time disk reads and peak generation run in the worker. Native full-song AUDIO takes now budget pages instead of the full decoded take; recording still follows selected duration/timeline/Punch/buffer capacity and disk-recorder limits. Input latency and comping remain pending; native 0.85+ adds acknowledged-prefix audio-take recovery (see below). Continue reading revision before every musical write, and re-read it after take finalization.

Streaming budgets belong to each Assets pool: the live workspace and separate background export/analysis jobs own independent pools. Live `audioHealth` reports the workspace pool; it is not total process residency. Offline mix analysis uses private blocking cursors and reports read failures instead of measuring gaps from real-time read-ahead.


## Interrupted audio-take recovery (0.85)

New native AUDIO takes create managed UUID directories under `data/Recordings`, containing a uniquely named Take WAV, an absolute-asset pre-recording `session.pulse` snapshot and `checkpoint.json` (schema 1). The disk writer acknowledges frames only after WAV-header refresh and explicit OS file flush. It then atomically publishes frame count, rate/channels, original lane/beat/tempo, bounded saved header, session SHA-256 and acknowledged-tail identity. The callback performs no journal/file work. The latest unacknowledged audio is outside the recovery guarantee.

FILE / command search > Recover interrupted audio take reviews acknowledged interrupted takes and builds a separate verified PCM24 WAV plus project in the background. Recovery never patches/deletes originals; it verifies exact copied prefix bytes, handles an interrupted later header using the saved checkpoint, and ignores trailing unacknowledged bytes. Identical-checkpoint recovery verifies/reuses the same copy. Changed/corrupt sources, session tokens, metadata, linked paths or outputs reject; cancel/stale work retains originals and any copy/draft. Explicit Open recovered project uses native unsaved-session backup; an edit while recovery runs prevents automatic Open review. Clean completion saves a verified project before recording its acknowledgement. Startup only announces pending takes; it never records or replaces a session automatically.

The twelve MCP tools are unchanged. Native status/get-project add optional `takeRecoveryBusy` (boolean), `takeRecoveryCopyReady` (boolean) and `takeRecoveryProject` (string, empty without a retained copy); these describe native recovery work, not a new recording/recovery write command. Earlier apps omit them. Models should direct users to the native review and re-read project/revision after any explicit recovered-project Open. Do not attempt to manufacture/edit checkpoint JSON through MCP. Existing recordings before 0.85 have no journals. Discovery is bounded to 4096 entries / 128 candidates, excluding links; original/draft/completion files are retained. Checkpoint retention, input latency, comping, multi-input destinations and physical power-loss qualification remain pending.

## Built-in selector automation (0.86)

Restart **both** the native app and stdio bridge after updating. The twelve tools are unchanged. Native 0.86+/bridge 0.86+ add the following stable targets to `pulse_get_project.automationParameters`; IDs 0–100 keep their previous meanings. `add_automation` accepts IDs **0–117** or discovered keys. The bridge rejects these new keys/IDs before submitting to older apps.

| ID | Key | Actual values / ordered `choices` |
| --- | --- | --- |
| 101–108 | `eq.enabled`, `compressor.enabled`, `reverb.enabled`, `chorus.enabled`, `flanger.enabled`, `phaser.enabled`, `delay.enabled`, `distortion.enabled` | 0 Off, 1 On |
| 109–113 | `stereo.enabled`, `stereo.mono`, `stereo.swap`, `stereo.invertLeft`, `stereo.invertRight` | 0 Off, 1 On |
| 114 | `dynamics.mode` | 0 Compressor, 1 Limiter, 2 Gate, 3 Expander |
| 115 | `distortion.mode` | 0 Soft / tube, 1 Hard clip, 2 Fuzz |
| 116–117 | `synth.wave1`, `synth.wave2` | 0 Sine, 1 Triangle, 2 Saw, 3 Pulse; audible on Pulse Analog (instrument 9) |

Effect selectors (101–115) support `targetTrack:-1` for Master, or a source/bus channel. Waveforms (116–117) require a source channel; on other synth engines the stored waveform remains dormant. The Playlist `track` is independent of `targetTrack`. Other native built-in sound selectors are already represented by `synth.table` and `synth.physical.mode`. Instrument changes, routes and effect order are not automation targets.

All point values remain **normalized 0–1 JSON numbers**, including switches: Off=0 and On=1. For a choice with actual index `k` and maximum `M`, use `k/M`; Gate is `2/3`, Hard clip is `1/2`, Saw is `2/3`. The renderer rounds the bounded actual value to the nearest integer; exact half values round upward. `choices` is an optional ordered string array, indexed by the actual integer value, not a list of normalized values. Native readouts add optional `valueText` alongside `value` (normalized) and `actualValue` (rounded); older apps omit it. Existing physical/wavetable selectors also expose choices. Do not assume every discrete numeric field has labels.

For timed switches, explicitly set `hold:true` on the outgoing segment. Example, after pattern arrangement creation and with a fresh revision:

```json
{"op":"add_automation","track":7,"target":"distortion.enabled","targetTrack":-1,"beat":0,"length":8,"points":[{"beat":0,"value":0,"hold":true},{"beat":4,"value":1,"hold":true},{"beat":8,"value":1}]}
```

This turns Master Distortion on at beat 4 and returns to its saved static setting at beat 8. A linear/generated curve instead walks through the rounded choice thresholds. A mode selector does not enable its effect by itself; author the corresponding `.enabled` curve or static effect setting as well. Stereo transforms need `stereo.enabled`; waveform changes need Analog source audio/notes.

Native Automation > New/link or Shift-right-click the switch/mode/waveform control creates a held clip without overwriting static settings. Select a point, then **Value** for exact named choices. The editor plots effective discrete values. During Song playback, native effect/slot/stereo/synth displays show live values. Stop/clip end return to the static controls. Automated insert bypass fades over 5 ms, keeps internal DSP/history warm even when dry and therefore still uses processing resources; Delay/Reverb history can be heard when re-enabled. Other mode changes retain existing DSP reset/transition rules. Unautomated chains retain their prior behavior.

Current writes use **project schema 30**, reading supported schemas 1–29; a selector ID mislabeled as schema 29 or earlier rejects. Keep pre-update project/History backups for older apps. Normal fresh-revision, atomic-batch, preview, Undo/Redo and independent offline-export rules apply. Recording/write modes, remaining source/Master processing controls (including legacy Master saturation/delay), tempo/meter maps and third-party plugin targets remain unfinished. Tutorials stay last.

## Master processing, mute and numeric point editing (0.87)

Restart both app and stdio bridge. The twelve tools remain unchanged. `pulse_get_project.automationParameters` has **122** stable descriptors; IDs 0–117 retain their meaning. New `add_automation` targets require native/bridge 0.87+:

| ID | Key | Actual range | Destination |
| --- | --- | --- | --- |
| 118 | `master.delayReturn` | 0–1 | Master only: `targetTrack:-1` |
| 119 | `master.delayFeedback` | 0–0.8 | Master only: `targetTrack:-1` |
| 120 | `master.softClip` | 0 Off, 1 On | Master only: `targetTrack:-1` |
| 121 | `channel.mute` | 0 Off/unmuted, 1 On/muted | Source channel or bus; never Master |

`track` is still the independent Playlist lane. The shared delay is the legacy ping-pong effect fed by channel `params.send`, not insert slot 7. `master.delayReturn` has no audible effect without a send/echo; feedback needs an echo history. These curves do not create routing or send signals. Master saturation is the existing post-Master-gain tanh stage. New Master processing links reject any nonnegative `targetTrack`; legacy ID 5 Master volume retains its old accepted aliases for saved-song compatibility.

Descriptors now add optional boolean **`channel`**, indicating whether a channel/bus destination is supported. `master` continues to indicate Master capability. IDs 5 and 118–120 have `channel:false`; synth fields still exclude buses. Older apps omit `channel` and cannot author the new targets. Check native capability discovery rather than assuming every non-synth ID can address both destinations. Source/bus mute remains subject to static Solo/routing exclusions.

Points still contain **normalized** 0–1 numbers. Return 50% uses value 0.5; feedback 40% uses value 0.5 because its actual maximum is 0.8. Soft saturation On uses 1. Muted uses 1, unmuted uses 0. `hold:true` selects the time of a switch. `actualValue` readouts report real units; optional `valueText` names Off/On choices. The bridge rejects new keys/IDs before writing requests to older apps. Preview, one-batch Undo/Redo, atomic destination validation and fresh revision checks apply.

Native Shift-right-click Master return/feedback/saturation or a channel/bus mute lamp links its real destination. New switch curves default to Hold. Native **Value** now works on any selected point: named choices for selectors, or a typed **actual-unit value** for numeric fields. For feedback enter `0.4` for 40%, not `40`; for filter cutoff enter Hz. Invalid, non-finite, out-of-range or fractional discrete values reject rather than clamp. A changed project or selected point rejects a stale editor. This native convenience does not change MCP's normalized point schema.

Return/feedback automate through bounded full-range 10 ms ramps; saturation uses a 5 ms crossfade between dry and the original tanh output. Mute uses the existing channel-gain smoothing. Mute curves retain scheduled/held source notes even when saved static mute is true, so an unmute can restore an in-progress phrase; such channels still consume voice/processing resources while muted. Static Solo exclusions remain authoritative, including isolated exports. At clip end and Stop, readouts clear and the static settings return (with the applicable audio smoothing). Effects and shared-delay history are not rewritten into the song. Unautomated songs retain the previous render path.

Current writes use **schema 31**, reading supported schemas 1–30. New processing/mute IDs mislabeled as schema 30 or earlier reject. Keep pre-update backups for older apps. Full multi-control Write/Trim recording, remaining source controls, tempo/meter maps and plugin hosting are still unfinished; tutorials remain the final ready-candidate step.

## Automation recording passes (0.88)

In the Automation clip editor select **Touch** or **Latch** from **Read**, then **Record pass**. This plays SONG from the selected clip's start through its end. Drag the dedicated **Write** control horizontally to perform that parameter, including named selectors and Master destinations. Touch returns to the exact existing curve on release; Latch holds the last value until Stop or the range end. **Finish pass**, Space or Stop ends early. A selected clip must fit inside any enabled Song loop range.

Review the orange held take over the original curve. **Apply** appends one overlay clip per Touch gesture (one for Latch), as a single Undo/Redo operation. The original curves and static settings remain available underneath; deleting an overlay reveals them. Higher, later clip indices take precedence for the same parameter and destination. **Discard** / Escape keeps the original project. Save after Apply to retain the pass. Unapplied takes are transient, are not recovery recordings, and are never armed on restart.

One pass writes one selected destination through the dedicated control. Values are consumed/stamped at audio callback boundaries and stored as held points, reproducing the audition without curve simplification or quantization. Gestures shorter than a callback may not be captured. Limits: 16 gesture clips and 256 total take points, within the existing 512-clip / 64-automation-clip / 4096-point project budgets and 600-beat automation clip maximum. Capacity overflow invalidates the pass without partial Apply. Untouched and zero-duration takes add no clips. Seek/loop discontinuities and project revision changes invalidate a pass; device interruption discards the transient pass. A pass ending at the loop boundary completes before wrapping.

`pulse_status` / `pulse_get_project` add `automationWritePending`. Native pending capture/review blocks MCP edits, save/export and view changes; inspection remains available and a fresh-revision transport Stop/Pause can finish the pass. The twelve tools do not arm automation recording: MCP can still author the resulting ordinary held automation clips atomically with `add_automation`. Read state again after native Apply. Continuous knobs outside the dedicated control, simultaneous parameter passes, destructive Write/Trim modes, plugin targets and tempo/meter maps remain unfinished. Tutorials stay last.



## Native full-range/ordinary-control recording (0.89)

Native Automation adds **Write**: Record pass starts a full-range held take at the effective value of the most recent active curve. Changes hold after release; an untouched Write pass deliberately records a constant range. Stop retains its performed prefix. Touch/Latch/Write can use the selected parameter's ordinary source/bus/Master knob or fader, synth knob or insert control, with safe panel navigation. Other parameters remain guarded; one destination records per pass. Return to Automation to Finish/review/Apply/Discard. Static values and original curves are retained, with one Apply Undo. Twelve MCP tools, schema 31, automationWritePending and capture/review mutation guards are unchanged. MCP does not arm native recording modes; use existing add_automation for authored curves. Multi-destination/Trim recording remains unfinished.


## Relative Trim layers (0.90)

Native Trim records a signed offset while touched; release returns to the original curve. Centre is neutral. Review/Apply retains original shaped curves and adds relative clips with one Undo. Continuous targets only; selectors, counts and other discrete targets reject Trim.

MCP add_automation accepts optional trim:true (app/server 0.90+). Points retain 0–1 encoding: 0.5 is zero offset, 0/1 subtracts/adds the full normalized range. For +10% range send 0.55, for -10% send 0.45. This is a normalized parameter offset, not dB or hertz; logarithmic targets shift in their logarithmic range. Each sample starts from static controls, folds active clips in saved order, replaces on an absolute clip, adds/clamps on Trim. Multiple relative layers accumulate in order; a later absolute clip replaces their result. Native neutral layers are transparent. Seek/loops use timeline values, with no historical offset accumulation.

Schema 32 persists automationTrim booleans; older supported schemas load with false defaults. Read parameter descriptors' trim capability and fresh revision. Keep original copies for older apps. set_arrangement still cannot represent any automation clip and would discard them. Existing point/generator/section operations preserve mode. Twelve tools and 122 IDs remain; multi-destination capture stays unfinished.


## Source choke membership (0.91)

Use set_track/add_track.chokeGroup integer 0–16 (top-level operation field, not params). 0 is Off; a nonzero value shares one mutually interrupting group across source channels. Buses require 0. For open/closed hats assign the same group to both. An accepted hit fades older voices of that group over 5 ms, including same-channel notes and typing audition. Effect tails and Playlist audio clips are not choked; only generated/sampler note voices participate. Zero-velocity, skipped and statically muted/Solo-excluded hits do not close other voices. Mute-automated channels retain the existing scheduled-note behavior.

Schema 33 persists groups; supported older files load with Off, and nonzero membership cannot be mislabeled as older schema. Sound preset/sample recall retains the selected channel's group; project/channel templates copy it. Fresh revision and existing atomic Undo rules apply. Stem exports retain peer closing-event timing without their audio. Shared-group source bounce rejects replacement because removing notes would remove events closing other channels; Song/stem export remains supported. No sample import operation, new generator family, new tool or new automation ID is added.

## Ripple time (0.92+)

Use pulse_edit with ripple_time only after inspecting every lane and marker/punch/loop range. start and length are beats; remove:false inserts empty time and remove:true deletes it. Preview destructive edits first, inspect candidateProject, then commit with a fresh revision. All lanes participate. Crossing clips reject rather than being truncated. Fully removed sections/cues disappear; deleted loops switch off. A collapsed punch switches off and resets to beats 0-8. Surviving media, notes and automation payloads stay intact. See references/api.md for exact boundary semantics and timeout recovery.

## Shared effects and parallel buses (0.93+)

Use set_track/add_track.auxSends to send a source or bus to up to four existing bus channels. Read the detailed reference before authoring: auxSends replaces all four slots, with target/level/preFader required in every slot. Bus effect mix 1 gives a wet-only shared return. Pre is after inserts/pan and before volume; Mute closes both pre and post. Source Solo retains returns; bus Solo isolates its branch. preview:true validates full acyclic routing before one-Undo commit. Read current track indices again after native bus deletion. Send automation and latency compensation remain unfinished.

## Auxiliary send-level automation (0.94+, project schema 35)

The four existing auxiliary slots now have continuous automation targets. Read `pulse_get_project.automationParameters`: IDs **122–125** map to `aux.send1.level` through `aux.send4.level`. There are **126** descriptors in total; IDs 0–121 keep their previous meaning. Targets accept a source or bus `targetTrack`; Master (`-1`) rejects. Each point's normalized value 0–1 maps to send gain **0–1.5** (0–150%). Native point **Value** edits accept actual gain, for example 0.75 means 75%; native send-knob numeric entry accepts percentages, for example 75.

```json
{"op":"add_automation","track":7,"target":"aux.send1.level","targetTrack":7,"beat":8,"length":8,"points":[{"beat":0,"value":0},{"beat":4,"value":0.6666666667},{"beat":8,"value":0}]}
```

First configure that channel's `auxSends` destination/tap with `set_track`, using the existing complete four-slot array, and create the destination bus if needed in the same atomic batch. An Off slot remains silent even if its level is automated; automation never enables or changes a route. Destinations and pre/post-fader taps remain static and obey existing cycle/Solo validation. Levels retain the same smoothing, Mute handling and common callback/export processing. Curves work with seek, loop and clip-end static restoration, generators, section operations, Trim layers, and one-destination Touch/Latch/Write/Trim passes. Saved static gain remains intact.

Native workflow: Mixer **Sends** opens four live level knobs. **Routing / taps** opens the existing four-slot route editor. Effect panels also expose **Levels**, and Ctrl+P finds **Mixer auxiliary send levels**. Shift-right-click a knob creates its curve; ordinary right-click offers actual percent entry and automation. During a pass, open Levels and drag the matching knob to capture that destination. Simultaneous destination capture remains unfinished.

Both native app and stdio bridge must be 0.94+. The bridge checks the OPEN app before submitting these new IDs/keys. Schema 35 persists them; schemas 1–34 with supported instruments remain readable, while a file carrying these targets under an older schema rejects. Keep original projects for older apps. Twelve tools remain unchanged. This completes auxiliary **level** automation; route/tap automation, arbitrary processor instances, freeze/unfreeze, plugin latency compensation and other v1.0 gates remain open. Tutorials remain the final release step.

## Input recording alignment (0.95+)

This is a local audio-input preference, configured natively in **Options > Tempo and recording > Align input**, or Ctrl+P **Input recording alignment**. It does not change existing songs, notes, PCM files, export rendering, or the project schema. Default **Off** preserves previous capture timing. Four modes are available: 0 Off (ignores retained manual correction), 1 Manual, 2 reported input + manual, 3 reported input and output + manual. Manual correction is -1000 to +1000 milliseconds. Combined correction must be within two seconds and resolves once at REC to an integer sample offset at the current device rate. Driver latency is a report, not an automatic measured loopback calibration; select settings from measurements of the monitoring/playback path you use.

A positive offset delays the capture window and places its first received sample at the intended Song beat. A negative offset captures earlier; when the requested audio precedes Song start, its unavailable prefix remains a gap. Count-in stays outside capture. Punch-in/out and selected take duration shift together, preserving the intended musical placement/length when space is available. Song boundaries can shorten or reject a take with an explanation. The recorder writes unchanged dry mono/stereo PCM. REC/Stop early finalizes the last input already received; no future device input is recovered after Stop. Keyboard NOTES and retrospective input history remain uncompensated.

Settings live in the optional strict `alignment` object inside workspace `data/input-audio.json`, with its own schema 1: `{"schema":1,"mode":3,"manualMs":12.5}`. Supported older preference files without it start Off. Do not replace this file while the app is running; use the native editor. Settings cannot change during takes, automation passes, previews or exports. They do not consume musical Undo. The recovery checkpoint stores the final aligned clip beat before recording; restoring interrupted PCM uses that beat once, independently of the current device/preferences.

`pulse_status`, `pulse_get_project` and runtime diagnostics expose read-only `inputAlignment`: `schema`, `mode`, `modeName`, `manualMs`, `sampleRate`, raw `inputLatencySamples`/`outputLatencySamples`, `canResolve`, `resolvedSamples`, `resolvedMs`, `error`, and `lastTakeSamples`/`lastTakeRate`. The last pair is a transient snapshot from the most recently armed audio take, initially zero. Unavailable resolution returns null samples/ms and an explanation. Status inspection never arms input or changes preferences. There are still twelve MCP tools, 126 automation targets and project schema 35. Automatic loopback calibration, simultaneous recording destinations, take lanes/comping, keyboard/history compensation and physical-driver qualification remain open. Tutorials remain last.
