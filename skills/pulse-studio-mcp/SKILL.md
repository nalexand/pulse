---
name: pulse-studio-mcp
description: Compose, edit, design sounds, arrange, play, save and export music in the open local Pulse Studio app through its MCP tools. Use for controlling Pulse Studio from an AI client, including creating editable songs; not for FL Studio automation or application source development.
---

# Pulse Studio through MCP

Control the actual open Pulse Studio workstation, not a project JSON file behind it. This skill is model/client independent: use the tools exposed by your MCP host. A host may prefix names with its server namespace; the underlying tool names are `pulse_status`, `pulse_edit`, etc. No source-code reading is needed.

Read [the connection and API reference](references/api.md) for setup, exact operation fields/ranges, units, routing, error recovery and a complete composition example. Read it before your first composition or when constructing an unfamiliar operation. It describes Pulse Studio 0.76.0 with twelve tools (the eleven-tool base from 0.31 plus offline mix analysis). Audio fade authoring requires app/server 0.54+ and an existing native audio clip; use `set_audio_fades` with a fresh clip index and revision, preserving every unspecified field. Current project writes use schema 25; schemas 1-16 gain neutral harmonic defaults, and schemas 1-15 migrate missing fade shapes to Linear. The optional pattern colour field requires app/server 0.34+; omit it for older versions. section_clips requires app/server 0.36+ and whole clips inside the section; extract_notes and merge_patterns require app/server 0.35+; use fresh lane indices and inspect appended results before retrying a timeout.

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

**TOOLS** and **Ctrl+P** also expose Automation segment shape and Generate automation curve. Song mode plays these six existing mixer targets; synth/plugin/tempo targets, read/touch/latch/write recording, modulation routing and personal generator preset catalogs remain pending.

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
[{"op":"set_song","masterEffects":{"eq":{"low":-1,"mid":0,"high":1},"compressor":{"threshold":-18,"ratio":2,"attack":25,"release":180,"makeup":0},"order":[0,1,2,3,4,5]}}]
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
{"op":"set_song","masterEffects":{"compressor":{"mode":"limiter","threshold":-1,"makeup":0,"release":100},"order":[0,2,3,4,5,1],"stereo":{"enabled":false}},"master":1,"masterSoftClip":false}
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

