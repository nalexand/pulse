# Pulse Studio local MCP

For model-independent operating instructions, use [skills/pulse-studio-mcp/SKILL.md](skills/pulse-studio-mcp/SKILL.md) together with its [detailed connection/API reference](skills/pulse-studio-mcp/references/api.md). These files are included in the portable package and corresponding source. They document every accepted field/range, replacement behavior, bus/automation semantics, error handling and a tested composition example. No application source reading is needed. Version 0.69 retains offline mix analysis as a twelfth tool alongside the eleven 0.31 tools, reads schemas 1–24 and writes schema 24 projects with pattern colours and independent audio fade shapes; pattern/automation clip lengths cap at the native 600-beat limit. Status adds `chasedNotes` (0.33+), a cumulative count of restored held notes, not a current voice count.

Pulse Studio 0.69 provides twelve tools for controlling the **open application**. The app applies edits on its message thread, validates the complete candidate, compiles playback, then publishes one undoable change. The client does not replace project files behind the app.

Enable **OPTIONS > Local MCP control**, or start the app with `--workspace "D:\projects\fl_studio" --resume --enable-control`. The setting persists in `data/mcp-settings.json`; disabling it invalidates the current connection. Control starts disabled in a fresh workspace.

The optional server requires Python 3.10+ and its standard library. The workstation itself remains standalone. For this development checkout, register with Codex:

```powershell
codex mcp add pulse-studio -- python D:\projects\fl_studio\tools\pulse_mcp.py --workspace D:\projects\fl_studio
codex mcp get pulse-studio
```

For an extracted portable release, use its `mcp/pulse_mcp.py` and the same workspace supplied by **Run Pulse Studio.cmd**. Absolute paths avoid working-directory surprises. Registration follows the [official Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp?surface=cli). The server uses newline-delimited JSON-RPC over [MCP stdio](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports); stdout contains protocol messages only.

## Tools

| Tool | Purpose |
| --- | --- |
| `pulse_status` | Current revision, song, audio, transport, export, save and backup state |
| `pulse_get_project` | Committed tracks, sounds, patterns/notes, clips, sections and project settings |
| `pulse_list_sounds` | 72 factory patches with complete editable synth settings |
| `pulse_new_song` | Back up the previous session and create a new Song-mode project, retaining Undo |
| `pulse_edit` | Apply 1–128 operations atomically as one project undo |
| `pulse_transport` | Play, pause, stop/rewind and beat seek |
| `pulse_save_project` | Native save with asset collection under `Projects/<name>.pulse` |
| `pulse_export_audio` | Background Song WAV export under `Exports/<name>.wav` |
| `pulse_undo` | Undo one complete project edit |
| `pulse_view` | Show/maximize Playlist, Piano roll, Mixer, Synth or Channel rack |
| `pulse_snapshot` | PNG image of the live workspace |
| `pulse_mix_analysis` | Background offline mix report and independently verified, undoable Master-gain reduction |

Call status/get-project before **every write** and supply its `revision` as `expected_revision`. A UI edit can invalidate it; reread and reconcile before retrying. Do not blindly replay timed-out mutations, since an edit may have completed before its response was lost.

Seek, resume and loop starts restore melodic notes whose gates are still held. Samplers use the corresponding source position; fresh synth voices restart their envelopes. Historical synth/effect state, release tails and drums are not reconstructed. A paused seek remains silent until Play. In app/server 0.35+, extract_notes and merge_patterns expose selected-note extraction and layered/sequential merging as pulse_edit operations. Confirmed used-pattern deletion remains UI-only.

Edits support `set_song`, `add_track`, `set_track`, `add_pattern`, `set_pattern`, `extract_notes`, `merge_patterns`, `set_notes`, `set_arrangement`, `set_markers`, `section_clips` and `add_automation`. Indices are zero-based; add operations append. In 0.34+, add_pattern/set_pattern accept optional `colour`: integer 0–12 (0 Automatic; 1 Green, 2 Sage, 3 Teal, 4 Sky, 5 Blue, 6 Violet, 7 Lavender, 8 Rose, 9 Coral, 10 Orange, 11 Sand, 12 Grey). Omission retains an existing colour or defaults a new pattern to Automatic. It changes every instance through the shared pattern, with normal atomic validation/Undo. Omit this field when controlling older versions. `set_notes` replaces one pattern/channel lane, and `set_arrangement` explicitly replaces the entire clip list. Start a separate song when composing from scratch. `pulse_new_song` creates eight default source channels and one pattern; customize them and append channels as needed.

Track settings include Analog/FM/Wavetable/Additive/Resonator patches, named factory sounds, gain/pan/filter/saturation/send/envelopes, EQ/reverb/chorus/stereo and output buses. Synth/effect objects accept partial overrides; unknown or invalid fields are rejected. Existing limits remain: 16 tracks, 32 patterns, 512 notes per lane, 512 clips, 128 markers and 32,768 compiled events. Larger edits can be divided into deliberate batches. Buses process audio and have no note lanes.

Automation target numbers are 0 volume, 1 pan, 2 cutoff, 3 saturation, 4 delay send, 5 Master. Values are normalized 0–1; points must increase in time and include both endpoints. Track automation needs a `targetTrack`; Master uses -1. The Playlist lane is supplied separately as `track`.

Writes are blocked during a take, active recording, modal dialog, unapplied note preview or export. Saves/exports accept a simple basename and refuse existing destinations unless `overwrite: true` is explicit. Export returns immediately; poll status until `exporting` is false and confirm the completion message. Exports use stereo 48 kHz/24-bit WAV, TPDF dither and a three-second tail. The default is -1 dBFS **sample-peak** normalization; app/server 0.64+ accepts `normalize:false` to preserve mix/limiter levels. This is not loudness normalization or true-peak limiting.

## Local access and recovery

There is no TCP listener, remote server, account, upload, telemetry or arbitrary code tool. The bridge uses `data/control/session.json` and per-session request/response folders. These contain a local session token. Any process with access to this workspace can read it: use ordinary OS account/file permissions to control local access. Session rotation, request expiry and replay protection prevent stale or duplicate commands; they do not isolate untrusted programs running as your account.

Before the first control edit, and before starting another song, the app saves an absolute-asset project snapshot under `data/History/before-mcp-*.pulse`. This preserves references without copying external assets; keep the original audio files available. New-song creation and every accepted batch are also in normal Undo. The current project schema is 24; schemas 1-23 remain readable. Older apps require a retained pre-update project/History backup. Control keeps existing 0.29 recovery/control filenames; current project writes require app 0.56+. Retained schema 16 backups remain readable by app 0.54+.

## Verification and example

`python -m unittest discover -s tests -p test_mcp.py -v` tests the stdio protocol and schemas. Core/editor/native tests cover validation, replay/expiry, stale revisions, busy guards, backup, undo, audio and native save/export. `tools/test_mcp_live.py` is restricted to an isolated workspace inside `test-output`.

`tools/compose_vault.py` (portable: `mcp/compose_vault.py`) is an original dark-score recipe using only MCP for song changes. It designs eleven source sounds and one reverb bus, writes ten 16-beat patterns, arranges a 64-bar/76-BPM soundtrack with six sections and Master fades, saves/exports it and starts playback. Its melody and patches are original; it includes no extracted game music or samples. The script starts a separate song after the automatic History backup. Inspect the recipe before rerunning it in a working session.

Third-party plugins, arbitrary audio/sample import through MCP, general scripting, resource subscriptions, HTTP transport and remote control remain outside this initial API.

Metronome sound, level and bar-accent are local UI preferences in data/metronome.json; this release does not expose MCP writes for them. The existing set_song.metronome toggle still works. Offline exports exclude clicks.

## Pattern extraction and merge operations (0.35+)

extract_notes requires pattern, track and 1–512 unique zero-based indices into the current lane readback. Optional move defaults false (Copy); alignStart defaults false (keep source beat positions). True alignStart subtracts the earliest selected onset; source length, colour and all note articulation remain. Move changes the shared source pattern. It appends/selects one new pattern, creates no Playlist clips and leaves Song/PAT mode unchanged. Empty, repeated, stale, negative and out-of-range indices fail atomically. Buses cannot contain notes.

merge_patterns requires distinct first/second pattern indices. Optional mode is layered (default) or sequential; optional policy is keep_all (default), skip_exact, prefer_first or prefer_second. Sequential requires keep_all and sums full lengths up to 64 beats. Layered uses the longer length: skip_exact removes full-field duplicates across both sources; preferences resolve only identical onset/pitch on the same channel, retaining the chosen source's notes. Other gate overlaps remain. Source patterns and Playlist clips are preserved; the new result inherits the first colour and becomes active. Each operation needs a free pattern slot; capacity is checked after filtering, and native commit compiles both the current Song and newly active PAT before Undo/checkpoint.

Both operations require native app and server 0.35+. A successful batch is one Undo; later operations can reference its appended indices. Read back the complete resulting project. Appending twice creates two new patterns, so inspect after timeout before retrying. See the skill API reference for all ranges and worked operation examples.

## Whole-section clip operations (0.36+)

Inside pulse_edit, section_clips requires section (an existing section marker id, not its display name) and destination (beats, 0–4096). Optional move defaults false (Copy); allowOverlap defaults false. It operates on every pattern/audio/automation clip wholly inside the source range. A crossing clip rejects the entire operation; point markers and empty sections are invalid. No clips, files, patterns, markers or loops are implicitly trimmed, deleted, replaced or ripple-shifted.

Relative positions, lanes, pattern links, audio source offsets/gain/fades and automation target/points are retained. Copy appends clip objects; Move changes original positions without new slots. The full destination range must fit and differ from the source start. Default overlap guards check Playlist lanes and linked automation parameters across lanes; explicit allowOverlap retains all destination clips. Existing automation priority remains: Copy appends, Move retains order. Native commit checks Song event capacity even in PAT mode before checkpoint/backup. Normal revision, busy, batch rollback and one-Undo semantics apply. Both app/server must be 0.36+.

Example operation: {"op":"section_clips","section":"verse","destination":16,"move":false,"allowOverlap":false}. The UI accepts 1-based bars; this API uses beats (16 = bar 5 in 4/4). Read the project first to identify the section id and clip boundaries. After timeout inspect positions/counts before retrying, since another successful Copy creates additional clips.

Version 0.37 adds native waveform sample-rate conversion and automatic cross-rate paste. These editor operations are not exposed by the eleven MCP tools; do not invent waveform API operations. Existing composition, section, save and export APIs are unchanged.

Version 0.38 changes native typing input and Browser access only. All eleven MCP tools and persisted schemas remain unchanged.

Version 0.39 adds native note-copy/paste UI workflows. It does not add new MCP operations. To copy a channel lane through MCP, read the current project, construct the intended destination notes and use set_notes with a fresh expected_revision; set_notes replaces the complete destination lane.

Version 0.40 adds native Browser sample favorites, file-drag assignment, sample context actions and filtering. These UI/library preferences have no MCP operations. All eleven tools, project schema 14 and sound/library schema 2 remain unchanged; favorite sample paths live separately in data/sample-favorites.json.

Version 0.41 adds native portable project ZIP export/import, collection/checksums, cancellation and guarded backup/Undo when opening imported songs. No new MCP operation or persisted song/sound schema. Native project opening now advances revision, so always read status again after a user opens a different song. ZIP packaging is available in the native File menu, not through pulse_export_audio.

Version 0.42 adds native scale/key guides, pitch fitting and chord-building previews. No new MCP tools or operation fields. Guide preferences stay outside projects. For the equivalent note changes, read the current lane and use set_notes with a fresh expected_revision; preserve the full destination lane when appending chords because set_notes replaces it. Native unapplied harmony previews use the existing busy guard for MCP writes.

Version 0.43 adds native quick/named version snapshots, local version browsing, structural comparison and guarded restore with backup/Undo. No new MCP tools, operations or song/sound schema. Snapshots are standard .pulse files with collected relative assets in data/Versions. A native restore changes the project revision, so read pulse_status or pulse_get_project again before a write. Comparison dialogs use the existing modal busy guard for MCP writes.

Keyboard shortcuts (0.44) are native local preferences: OPTIONS > Keyboard shortcuts supports remapping, conflict checks, disable/defaults and .pulseshortcuts import/export. They do not change the eleven MCP tools or project revisions. Key examples use factory defaults; command/menu hints show live bindings. MCP clients should use tool operations rather than keyboard simulation.

Sampler tuning (0.45+): set_track.params.sampleRoot is an integer MIDI note 0–127 describing the recorded pitch (C4=60, A 3=57); sampleTune is a playback correction from -100 to +100 cents, positive raising the result. A sharp A 3 sample by 23.5 cents needs {"sampleRoot":57,"sampleTune":-23.5}; then write intended Piano roll pitches normally. These fields affect only sampler playback, not Playlist audio. Use them on an already loaded sampler, inspect readback and use a fresh revision for each write; arbitrary sample loading remains unavailable. Schema 15 persists calibration; schemas 1–14 migrate to C4/0. Native Channel settings > Tune > Detect pitch proposes a monophonic estimate for review; the detector has no MCP operation. Eleven tools remain.

App 0.47 displays Automatic patterns with distinct swatches by pattern order; explicit palette values retain their existing meaning. Native ruler/selected-clip loop selection creates or updates the ordinary section `pulse-playlist-loop` and uses `loopSection`. It plays all Song tracks in that time span. Existing MCP marker/section operations remain available; no new tool or operation was added. Read fresh state after native edits.

App 0.48 adds native selected-audio crossfade fitting and fade clearing using existing schema 15 fade fields. These UI actions are not new MCP operations. Read fresh state after native edits; set_arrangement still replaces the entire clip list and does not expose arbitrary audio file loading or fade authoring.

App 0.49 adds native harmony preset management and .pulseharmony files. Recall previews settings and a temporary key; Apply commits generated/fitted notes normally. Presets do not introduce MCP operations or alter project schema 15. Resolve pending native previews before MCP writes, and read fresh state after Apply.

App 0.50 adds native diatonic progression generation, triads/sevenths and nearest-chord voice leading in the harmony preview. The eleven MCP tools and schema 15 remain unchanged. MCP composition can use `set_notes` for generated progressions; read the full lane and preserve existing events because this operation replaces it. Resolve native previews with Apply/Cancel before writes, then read a fresh revision. No progression-specific MCP operation was added.

App 0.51 hardens the shared project save path with validated/flushed/read-back-verified staging and exact `.pulse.bak` previous saves. MCP saves keep the same eleven-tool protocol, schema 15, explicit overwrite and revision rules. Recovery and backup files stay local; a native previous-copy restore opens an unsaved draft, so read fresh project/revision state before editing.

App 0.52 moves automatic recovery validation and disk I/O to one background worker. Status adds `autosaveBusy`, `autosavedRevision` (the project revision captured by the last successfully completed recovery write) and `autosaveError` (empty after success). Autosave does not clear `dirty`; only a permanent project save does. Changes during a successful write are captured by a coalesced followup. Failed writes retry at the next interval. Explicit save/new-song/open/restore/close synchronize with the worker; a completed earlier snapshot cannot reappear after recovery cleanup. Existing revision and overwrite rules still apply.

App 0.53 adds native recovery-history limits and cleanup; `recoveryCleanupBusy` is an additive status/runtime boolean. The eleven MCP tools do not expose cleanup/deletion commands. The native FILE menu supports exact-list review or an explicitly enabled automatic policy; defaults retain every archived copy until manual cleanup. Cleanup never edits project data, revision, dirty state or Undo.

### Existing audio fades (app/server 0.54+)

`set_audio_fades` is an operation inside `pulse_edit`; the tool count remains eleven. Restart the stdio MCP server and reconnect the client after updating to 0.54 so the advertised operation schema refreshes; older running app/server processes do not support this operation. Read `pulse_get_project` immediately beforehand and use the zero-based array index of an existing clip whose `audioPath` is nonempty. Required: `op: "set_audio_fades"` and `clip` (0–511, existing audio only). Supply at least one optional field: `fadeIn`/`fadeOut` (finite beats from 0 through that clip's `length`), `fadeInCurve`/`fadeOutCurve` (integer 0 Linear, 1 Equal power, 2 S-curve). Missing fields retain their current values, including hidden zero-length-end shapes. The operation cannot change paths, load audio, move clips or change gain/source offsets. Both ends may overlap inside one clip; their gains multiply. Batches still validate and commit atomically as one Undo; invalid/stale/busy calls retain the entire song.

Example after reading an existing audio clip at index 3:

```json
{"expected_revision": 42, "operations": [{"op": "set_audio_fades", "clip": 3, "fadeIn": 0.5, "fadeInCurve": 1, "fadeOut": 1, "fadeOutCurve": 2}]}
```

Do not reuse the example revision or clip index: derive both from fresh readback, verify each requested duration fits, then read the updated project after applying. `set_arrangement` still replaces the full Playlist with pattern clips, so use this targeted operation to preserve imported/recorded audio. Schema16 persists both shapes; schemas 1–15 migrate missing shapes to Linear. Newly saved projects require app 0.54+, while pre-update `.pulse.bak`/History files can retain the previous format. The native crossfade and clip-shape dialogs use the same envelopes.


App 0.55 adds native personal progression presets and six complete factory starters in Chord progression > Presets. Presets retain key/scale, degrees, octave, step, gate, velocity, triads/sevenths, voice leading and duplicate policy; recall keeps the target beat and Preview/Original mode, previews a temporary guide, and Apply remains one Undo after PAT/Song validation. Import/export uses .pulseharmony; the shared 128-entry catalog uses harmony schema 2 when progressions are present and reads schema 1. Scale/chord-only exports remain schema 1. This is separate from project schema 17, the instrument sound catalog and sound format. There are no new MCP tools or operations for preset management. Apply/Cancel pending native previews before MCP writes, then read fresh state; do not interpret progression presets as instrument sounds or write catalog files to bypass the enabled MCP boundary.


### Pulse Additive (app/server 0.56+)

Restart both native app and stdio server after updating. The Additive release expanded pulse_list_sounds to 64 factory sounds; current 0.66 lists 72, including eight Resonator sounds. Use instrument string additive in add_track/set_track; project readback uses integer12. In synth overrides, harmonics is exactly 16 finite JSON numbers from 0 to 1, ordered fundamental through harmonic 16. Providing it replaces the complete array; omission preserves every level. Unknown fields, booleans, strings, NaN/infinity, wrong lengths and out-of-range values reject atomically. The full patch stays embedded; there are no external wavetable/spectrum assets. Other synth fields keep their documented ranges. Mix/coarse/detune control two oscillators using the same spectrum; sub/noise are independent, while FM/table/wave-selection fields are stored but do not add those engines. Harmonic levels smooth, normalize by at least their sum, and upper oscillator partials fade below Nyquist. All-zero harmonics silence the harmonic oscillators; disable sub/noise too when silence is required.

Read fresh pulse_get_project first and confirm app version 0.56+ and an existing source-channel index. A sample operation for an existing source track 7 inside a revision-checked pulse_edit.operations batch is:

```json
{"op":"set_track","track":7,"instrument":"additive","preset":"Glass partial keys","synth":{"harmonics":[1,0,0.5,0,0.25,0,0.12,0,0,0,0,0,0,0,0,0.08]},"params":{"volume":0.4}}
```

Choose the actual track from readback; do not reuse an example index or revision blindly. Instrument and preset must match; preset recall runs before overrides. Read back all 16 levels with float tolerance and verify notes/routes remain as intended. Save/export/Undo use existing tools; resolve native note previews first. Current project writes use schema 22 and require app 0.64+, reading schemas 1-21 with neutral defaults for newly introduced fields; pre-17 synths gain neutral harmonic defaults. Additive sounds/custom harmonic data and libraries containing them or additive favorites require sound/library schema 3, reading 1-2; older-engine files without new harmonic data still write schema 2. Keep pre-update project/History backups for old apps. Harmony presets retain their separate schema 1/2 format. The tool count remains eleven.


The 0.56 bridge retries temporary Windows sharing locks while reading session metadata or the same response, within the original timeout. It submits each request once. A response-cleanup failure does not turn a known result into a failed mutation; a locked leftover response can remain in its session folder. Metadata timeout explicitly means no request was submitted. Response timeout can still mean an edit applied: inspect fresh status/project state before deciding what to do, and never blindly repeat a mutation.


App 0.57 adds native timing/velocity groove templates and extraction. TOOLS > Groove templates or Note tools > Groove opens selected/all-lane preview; Presets offers six factories, a separate 128-entry local library and .pulsegroove import/export. A 2-32-cell profile repeats from pattern beat 0 with per-cell offsets (-49..49% of grid), velocity targets and active masks; grid 0.0625-4 beats, cycle<=64 beats, timing/velocity strength 0-100%, optional straight-grid window 0-50% of a cell. Capture reads one explicit full cycle from original selected/source notes, averages offsets/velocities and leaves empty cells inactive. Apply validates PAT/Song event capacity and bakes changed note beats/velocities as one Undo; pitch, gate, probability/ratchets and source order remain. Global Song swing still applies on top. Native capture/catalog/cell editing are not new MCP operations: resolve any pending preview, then read fresh committed notes using pulse_get_project. Do not assume a profile is linked to notes or repeat an Apply blindly. Current project schema 18, sound/library schema 3, harmony schema 1/2, 64 instrument sounds and eleven tools stay unchanged. Groove files use their own schema 1 and contain no source notes/audio/capture range. Persistent per-pattern groove assignments, broader phase alignment, multichannel capture and third-party formats remain pending.

## Automation shapes and generators (0.58)

Create a clip by right-clicking a mixer volume, pan, cutoff, drive, send or master control and choosing **Create automation clip**. Double-click a Playlist automation clip to open the editor. **Shape...** sets the selected point's outgoing segment, or every segment, to **Linear**, **Smooth**, **Ease in**, **Ease out** or **Hold**. Tension **-1 to +1** warps the time within a segment without overshooting its endpoints; positive values delay movement, negative values advance it. Hold ignores tension and jumps at the next point. The existing STEP switch toggles Hold while retaining the other shape settings.

**Generate...** opens a compact draft graph for **Sine**, **Triangle**, **Square**, **Step sequence**, **Seeded random hold** or **Seeded random smooth**. Set period in beats (0.0625-600), cycle phase (0-1; 1 aliases 0), normalized minimum/maximum, even resolution 4-64, unsigned seed 0-4294967295 and 2-32 comma-separated step values. Minimum must not exceed maximum. Triangle and Square use two cells per cycle; Step sequence uses its entered list; sine and random modes use Resolution. Sine is sampled into linear segments. Random smooth joins its seeded knots with Smooth segments; truncated first/last segments are fitted between the generated endpoint values. Phase offsets the knot grid. Random modes produce new seeded knots across periods; Period controls their density rather than repeating a fixed random cycle. Resolution/steps controls irrelevant to a wave are retained for switching waves and still require valid values.

The graph previews the draft without changing playback, the song revision or Undo. **Apply** replaces only that clip's points as one Undo; **Cancel** keeps the original curve. A successful Apply remembers the controls for this app session. Generated points, including endpoints and shape metadata, are the saved musical result: they remain editable and reproduce the same seeded curve in playback, export, recovery and templates. Generator settings are not a persistent live modulation processor. Change the draft and Apply again to regenerate. Increase period or reduce resolution if the result exceeds 256 points; the whole project remains limited to 64 automation clips / 4096 points. Invalid values, recording, pending note previews and stale clip/revision callbacks retain existing data.

**TOOLS** and **Ctrl+P** also expose Automation segment shape and Generate automation curve. Song mode plays these six existing mixer targets; synth/plugin/tempo targets, read/touch/latch/write recording, modulation routing and personal generator preset catalogs remain pending.

Automation segment metadata was introduced with **schema 18**; current writes use schema 22, reading schemas 1-21. Legacy segments gain neutral Linear/tension-zero defaults and retain their previous audio. Modern points save required `curve` (0 Linear, 1 Smooth, 2 Ease in, 3 Ease out), `tension` (-1..1) and `hold` (boolean); Hold takes precedence. Curved content cannot be mislabeled as an older schema. Keep old project/History backups for apps before 0.58.

The eleven MCP tools remain. App/server **0.58+** extends `add_automation` point fields and adds `set_automation_points` and `generate_automation` inside `pulse_edit`. Read a fresh project/revision and an existing automation clip index before editing. Replacement needs the complete point list, including both clip endpoints. Generator edits preserve the clip's position, length and linked parameter. Both operations use the native validation/Undo transaction and cannot open arbitrary files.

Read back points after generation and inspect the linked target and clip timing. Save/export/Undo use the existing tools. Generator periods and point beats use quarter-note beats, not bars or seconds. Reconcile a stale revision; never replay an edit blindly after a timeout.

Example operations, using a freshly read existing automation clip index (1 here):

```json
[{"op":"generate_automation","clip":1,"wave":"random_smooth","period":4,"phase":0,"low":0.2,"high":0.8,"resolution":8,"seed":12345}]
```

`wave` IDs are `sine`, `triangle`, `square`, `steps`, `random_hold`, `random_smooth`. Optional fields default to phase 0, low 0, high 1, resolution 16, seed 12345 and steps [0,1,0.25,0.75]. `wave` and `period` are required. For `set_automation_points`, supply `clip` and `points`; each point requires `beat` and `value`, with optional hold=false, curve=0 and tension=0. Missing endpoints, non-automation targets, invalid ranges/types and excess point counts reject the complete batch. With `add_automation`, the same optional shape fields apply.

## Session history (app/server 0.59+)

Call `pulse_status` with `{"include_history":true}` to inspect the active session journal without changing it. The normal status remains compact and includes `undoEdits` and `redoEdits`. The optional `history` object contains `currentState` (an integer chronological state index), `sessionOnly:true` and `states`, oldest first. Each state has `state`, `editTimeUnixMs`, `summary` and multiline `detail` with affected objects. State 0 is the oldest retained baseline and has time 0 (unknown), not Unix epoch as an actual edit. Other times belong to the original checkpoint and are retained across navigation. No extra project snapshots, notes or audio blobs are returned.

`pulse_undo` accepts `expected_revision`, optional `direction:"undo"|"redo"` (default undo) and optional integer `steps:1..100` (default 1). To restore a selected journal state, compute the difference from the freshly read `currentState`: negative means Undo, positive means Redo, and zero needs no write. Request the absolute difference as steps with the corresponding direction. A complete multi-step move changes revision once and publishes only the final playback state. A request larger than the available side rejects without moving even one step. The usual recording, note-preview, dialog and export locks apply.

Example after a fresh status shows currentState 8 and desired state 5:

```json
{"expected_revision":42,"direction":"undo","steps":3}
```

Use the actual returned revision, not the illustrative 42. Read back the project/status before continuing. Redo uses the same tool with direction redo. Do not keep state indices across edits: retention can remove old states and a new branch clears Redo. Never repeat a navigation blindly after a response timeout. A fresh revision plus current song/history is the evidence needed to decide another request. Omit these optional fields for app/server versions before 0.59. The bridge checks the open app version before submitting any new history-field request; an older or unknown version rejects without writing a request. This prevents an older app from ignoring direction/steps and performing a different one-step Undo. Default one-Undo calls still work with older apps.

The native equivalent is EDIT > Project edit history / Ctrl+Alt+H (remappable), with all-word search, Undo/Redo filters and an inspect-before-Restore list. It is distinct from the persistent FILE > Project version history and from waveform edit Undo. Session history does not survive app restart, change the current save destination or write any file until an ordinary save/recovery operation.


## Missing audio and content identity (0.60)

Native FILE > Project audio files learns valid referenced originals in workspace/data/asset-fingerprints.json, a separate schema-1 cache with at most 1024 absolute source paths / 4 MiB. Each record contains complete-file SHA-256, byte size and modification/observation dates, not raw audio. Source inventory changes request background learning; Refresh re-verifies within budget. Ordinary audits reuse unchanged size/date observations. The cache describes last observed content and is not a filesystem watcher; missing ordinary files need a prior observation, while a collected hash filename supplies identity independently on another PC.

Search folder checks the selected ordinary folder/nonhidden subfolders for exact raw-byte matches regardless of the old filename. Uncached collected references consider supported audio extensions; known sizes can identify other renamed candidates. Re-encoding/tag edits change the digest. Unobserved ordinary paths retain labelled filename-only proposals. Identical copies stay ambiguous. Apply unique matches requires a complete applicable scan; explicit Choose match/Locate remains available for incomplete proposals. Decoding/range/hash rechecks precede one atomic shared-reference edit and Undo/Redo. Musical notes, calibration, selections, loops, clip offsets/gain/fade shapes remain; current saved source files are retained.

Bounds: 20,000 entries, 16 displayed proposals per source, 512 MiB/file and 2 GiB read per pass, streamed cancellation and no traversal through links. Incomplete reads/limits disable automatic unique matching for affected results. Corrupt/stale/failed cache writes retain previous bytes; collected files with a mismatching SHA-256 name are flagged. Missing references remain editable placeholders. There is no plugin recovery yet, no changed song schema, and no arbitrary-file MCP command. After native relinking, re-read revision/project and use existing tools for save/play/export. Native cache observations remain independent of project Undo and are not included as a musical dependency in project ZIPs.

## Master processing and clear sample playback (0.61)

Click the **Master** name at the top of the Mixer strip, or search **Master processing** with Ctrl+P. This opens the actual shared delay return/feedback, master volume and soft saturation controls. **Dry master** bypasses saturation and sets the delay return to zero with one Undo. Channel/bus inserts, delay sends, notes, source files and feedback settings are retained. It does not remove reverb already recorded into a source file.

New projects bypass master saturation. A new sampler channel has zero delay send and bypassed EQ/compressor/reverb/chorus/flanger/phaser. Replacing the sound of an existing channel retains that channel's processing and routing; check its Mixer delay send and routed bus if it still sounds wet. Feedback zero gives a single echo when return/send are nonzero; set the return or that channel's send to zero for no echoes. The sampler still uses its amplitude envelope, channel filter, gain and pan. Master bypass allows unclipped floating-point levels, so lower the faders when the output peaks exceed full scale.

Browser/waveform preview remains available while a song plays when explicitly requested, but an earlier preview stops on Play, a played note, recording, or sample assignment. This avoids the original sample playing over its channel copy. Preview cancellation also works when its command queue is full.

Version 0.61 introduced **schema 19**, reading schemas 1-18. Current writes use schema 22 and require app 0.64+. Older projects retain the original master tanh saturation and 50% shared delay return on migration, so opening them does not silently remix them. Choose Dry master explicitly to hear an old project without that master processing, then save. These output controls were introduced in 0.61; 0.62 adds the independent Master insert chain described below.

MCP still has eleven tools. On app 0.61+, `pulse_edit` / `set_song` accepts `masterSoftClip` (boolean; new-song default false), `delayReturn` (0-1, default 0.5), and the existing `delayFeedback` (0-0.8, default 0.3). For a dry master use `{"op":"set_song","masterSoftClip":false,"delayReturn":0}` with a freshly read revision. `pulse_get_project` returns both new fields. They participate in the existing atomic edit, history, save, recovery and render behavior. Older apps reject these new edit fields; do not omit them and claim a dry master on an old app. Version 0.62 adds the six-effect Master chain described below; arbitrary plugin insert chains remain pending.

## Master insert effects (0.62)

In **F9 Mixer**, click **Master** and choose **Insert effects...**, or click one of the six slots in the Master sidebar. The existing EQ, compressor, reverb, chorus, flanger and phaser editors now work on an independent Master chain. **Master stereo tools** adds width/mono/swap/input-polarity/output-trim processing after those six effects. Master compression uses an internal stereo-peak detector and has its own gain-reduction meter. Channel selection and the Master target stay separate; typed dialogs, slot menus and chain-load callbacks reject a changed target or stale revision.

Right-click a Master slot to reorder it, use **Chain...** for the eight factory chains, copy/paste between Master/channels/buses, or save/load the existing `.pulseeffects` files. Chain recall retains the destination's stereo utility, routing and sidechain settings; stereo controls remain separate. Each accepted change is undoable. **Dry master** bypasses all Master inserts, its stereo utility and soft saturation and mutes the shared delay return in one edit, retaining effect values/order and every channel/bus setting. Undo restores the complete Master state.

Signal order is channel/bus processing and routing → shared delay return plus metronome → Master inserts in their saved order → Master stereo → Master volume → optional soft saturation. Live playback, input monitoring and offline exports use the same DSP path. Raw recorded WAV takes remain dry. Explicit Browser/waveform preview stays at original-speed dry audition gain outside the Master chain. Effects are fixed, one instance of each; third-party plugins, duplicate instances, Master sidechain routing, latency compensation and effect-parameter automation are still pending.

New projects bypass all Master inserts and stereo tools. **Schema 20** embeds the full Master chain and stereo state and reads schemas 1-19 with neutral, bypassed Master insert defaults, preserving their previous sound. Existing 0.61 master saturation/delay settings retain their meanings; older pre-0.61 songs still migrate with their original saturation/return. Master insert fields were introduced in app 0.62; current schema 22 files require app 0.64+. Keep pre-update backups for older executables. Complete and channels/mixer templates, history, recovery, collected saves and portable ZIPs retain Master state.

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

Conditional notes were introduced in **schema 21**, requiring app 0.63+. Current writes use schema 22 and require app 0.64+. Schemas 1-20 load missing conditions as Always without changing prior audio. Schema 21 requires both fields on every stored note; malformed fields and conditional notes mislabeled as an older schema reject atomically. Keep pre-update `.pulse.bak`/History backups for older executables. Master processing introduced in schemas 19/20 retains its existing behavior.

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

Current writes use **project schema 22**, reading schemas 1-21 with legacy compressor/knee-zero defaults. New effect-chain exchange files use **schema 2**, reading schema 1 with the same defaults. Old projects/presets retain their audio; new modes cannot be mislabeled as old schemas. Templates, collected saves, autosave/recovery, version snapshots and full exports retain the state. Keep pre-update backups for older executables and restart the app/MCP server after updating. Native status, MCP metadata and runtime reports now derive the release version from CMake rather than retaining stale 0.60 labels.

Sidechain filtering, optional lookahead, oversampling/true-peak measurement, and smoother bypass transitions remain pending; the overall dynamics roadmap is still partial.

MCP export level (0.64+): `pulse_export_audio` optionally accepts `normalize` (boolean, default true). Set **`normalize:false`** to preserve mix/limiter levels in the existing 48 kHz/24-bit dithered WAV with a three-second tail; this overrides no project or native-export preference and still requires a fresh revision. Omitting the flag retains the original -1 dBFS peak-normalized export. The updated bridge refuses this explicit option on older apps rather than silently letting them ignore it. Poll until exporting=false and inspect completion before using the file.


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



## Offline mix analysis (0.68+)

Use `pulse_mix_analysis(action="start", expected_revision=<fresh status revision>)`, then poll `pulse_mix_analysis(action="status")` until `mixAnalysis.busy` is false. It measures a snapshot of the current Song/PAT, not a selected clip or export range. Analysis does not edit project data or add Undo. A successful report has `current:true`; failed/cancelled jobs report their message and must not be treated as completed measurements. `pulse_status.mixAnalysisBusy` is a compact worker flag.

The full state includes `busy`, `applying`, `progress` (0–1), `sourceRevision`, `current`, `canApplyLevel`, `report`, `message` and the local `reportPath`. A report contains `schema:1`, algorithm, 48 kHz sample rate, frame count/duration including its two-second tail, Song/PAT flag, complete `projectHash`, raw source SHA-256 identities, per-channel linear `peak`, `rms`, `dc`, integer `fullScaleSamples`, stereo `correlation`/`correlationValid`, three normalized `bandFraction` values (20–200/200–4000/4000–20000 Hz), Master gain before/after and positive `reductionDb`. Its `summary` supplies readable units and observations. Silence gives zero linear levels; the summary floors dBFS at -160. Invalid correlation is explicitly flagged. Report `canReduce` describes that measured snapshot; state `canApplyLevel` additionally checks live eligibility.

Explain the proposed reduction before applying within the user's authorized mix-editing scope. For an eligible current report, read fresh status and call `pulse_mix_analysis(action="apply_level", expected_revision=...)`. Poll until not busy, require an applied message and read the project back. Apply performs a fresh render, compares the original source bytes and project identity, verifies a -1 dBFS sample ceiling, then changes only base Master gain as one Undo. It never changes EQ, dynamics, notes, instruments or arrangement. Saturation or Master-gain automation blocks this linear correction. Any intervening edit invalidates the result. Cancel uses `action="cancel"` and a fresh revision; it does not revert an already completed Apply. A timeout is ambiguous: inspect state/project before retrying.

Reports persist outside project files and reopen read-only after restart. Analyze again to enable correction. This is a bounded ten-minute, local sample-peak/RMS/FFT check, not LUFS, true peak, masking, genre judgment, mastering-chain generation or audio listening. It excludes monitoring/click/Browser previews. No audio-file path argument or external service is accepted. Use `pulse_export_audio(normalize=false)` to preserve verified mix levels in export; the default normalized export performs its own peak normalization.


## Native waveform restoration (0.69)

The native waveform editor adds DC-offset removal, 50/60 Hz hum filtering and isolated-click repair, with background preview, Original/Restored audition and one waveform Undo on Apply. It does not add a remote processing tool or file-path capability. `pulse_status` exposes boolean `restorationBusy` and `restorationPreviewReady` diagnostics. Existing project operations, schema 24 and the twelve-tool list remain intact. Do not simulate restoration by rewriting source audio or mailbox/project files. Use native controls for this operation and explicitly assign the edited copy to a sampler/Playlist before expecting it in music/export. Details and limits are in the README's **Audio restoration (0.69)** section.

Version 0.73 adds native waveform spectral band attenuation and paired spectrogram review. Existing `restorationBusy` / `restorationPreviewReady` status fields include spectral processing; there is no new spectral-authoring operation. Native Apply changes waveform history only; To sampler / To Playlist explicitly assigns its copy. Read fresh state before subsequent MCP music edits. Twelve tools and schema 24 remain unchanged.

Version 0.74 adds native read-only pattern notation and local MusicXML/SVG export. This has no MCP authoring/view/export operation; twelve tools and schema 24 remain unchanged. Notation choices do not change musical revision or actual channel selection. See the model-independent API reference and README for the declared sixteenth-grid score limits.

## Native retrospective input history (0.75)

TOOLS / Recording settings / Ctrl+P > Retrospective audio history provides explicit native opt-in dry input retention. It is OFF at every startup; preferences never enable the microphone. Source, duration and preallocated ring budget are native controls. Complete contiguous Song callback ranges can be frozen, reviewed, auditioned and explicitly placed as one Undo; see README **Retrospective audio history (0.75)** for exact limits and future work. Native capture/freeze/audition alone does not change the musical revision; To Playlist does. Read fresh state before subsequent MCP writes.

`pulse_status` exposes boolean `audioHistoryEnabled`, nonnegative integer `audioHistoryFrames`, boolean `audioHistoryBusy` and boolean `audioHistoryReady`. Enabled means retention was explicitly armed, not that transport/input is currently eligible; Frames counts the latest retained segment. Busy is a freeze worker; Ready is a loaded unapplied preview (source/revision guards may still prevent Apply). A paused buffer has Enabled false and can still have Frames > 0. These are diagnostics only. There is no MCP enable/start/stop/freeze/recall operation; use native controls. Do not manufacture tool calls or rewrite preference/mailbox/project files to imitate unsupported capture. Twelve tools and schema 24 remain unchanged.
