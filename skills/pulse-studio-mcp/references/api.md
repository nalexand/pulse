# Pulse Studio local MCP reference (0.72 / twelve tools)

Contents: connection; response handling; tools; edit operations; sound fields; arrangement/automation; worked example; troubleshooting.

## 1. Connect any MCP-capable AI client

Requirements: Windows x64 Pulse Studio running in a writable **local workspace**, Python 3.10+ with its standard library, and an AI host supporting MCP stdio. The application itself needs no Python for ordinary use. No network server, account, subscription, cloud project, API key or MIDI hardware is involved.

1. Open the app using the intended workspace. Enable **OPTIONS > Local MCP control**. It starts disabled in a fresh workspace and persists locally. Alternatively launch the executable with `--workspace "D:\projects\fl_studio" --resume --enable-control`.
2. Configure an MCP **stdio** server in the AI host. In a development checkout the script is `<workspace>/tools/pulse_mcp.py`. In the portable package it is `<package>/mcp/pulse_mcp.py`. Use absolute executable/script/workspace paths. The workspace argument must match the app's workspace, not merely a folder containing a saved song.
3. Start or reconnect that server through the host, list its tools, then call `pulse_status` to verify the open song. The script does not launch the workstation. Some hosts discover newly registered servers only after a client restart.

Common JSON host configuration (adapt the outer configuration keys to the host):

```json
{
  "mcpServers": {
    "pulse-studio": {
      "command": "C:/absolute/path/to/python.exe",
      "args": [
        "D:/projects/fl_studio/tools/pulse_mcp.py",
        "--workspace", "D:/projects/fl_studio"
      ]
    }
  }
}
```

For a host using the Codex CLI's MCP registry:

```powershell
codex mcp add pulse-studio -- C:\absolute\path\to\python.exe D:\projects\fl_studio\tools\pulse_mcp.py --workspace D:\projects\fl_studio
codex mcp get pulse-studio
```

Use the Python executable actually installed on that machine; the placeholder is not a literal executable path. A Python executable path with spaces must be quoted in a shell. In JSON it is a single string. For portable releases substitute the extracted `mcp/pulse_mcp.py` path and the workspace chosen by the package launcher. Do not register a remote HTTP endpoint: none is provided.

The protocol is newline-delimited JSON-RPC over stdin/stdout. The MCP client handles initialization and tool calls. Do not type arbitrary commands into server stdin or mix logs into stdout. `tools/mcp_client.py` is an optional standard-library client for deliberate local scripting; it is not required by AI hosts.

## 2. Responses, revisions and bounds

Tool results contain MCP `content` and, for structured results, `structuredContent`. Check `isError` before assuming success. The text content serializes the same state for clients without structured-result support. `pulse_snapshot` returns an image content block: display it using your host's image support instead of dumping base64.

Status fields include `version`, `revision`, `title`, `bpm`, `songMode`, `songLengthBeats`, track/pattern/clip/marker counts, `playing`, `positionBeats`, `framesProcessed`, `activeOutputs`, `peakOutput`, `recording`, `modalDialog`, `exporting`, `exportProgress`, `dirty`, `currentFile`, `backup`, and a human-readable `status`. `chasedNotes` (0.33+) counts restored held notes cumulatively; it is not the current number of active voices and may be absent in older apps. A peak can be retained from earlier playback, so it alone is not proof of current audio.

`pulse_get_project` returns those fields plus `project`, including complete `tracks`, `patterns[].lanes[track]`, `clips`, `markers` and song settings. Read-only calls accept `{}`. Other calls require `expected_revision`, the integer from a fresh status/project read. A call can succeed without changing revision (transport, save, many view actions). Always reread rather than predicting revision increments. Selecting another active pattern through `pulse_view` changes revision. User UI edits can change it at any time.

Bounds: 16 tracks **including buses**, 32 patterns, 512 notes per pattern/channel lane, 512 clips **including automation/audio**, 128 Song markers, 64 automation clips, 256 points per automation clip, 4,096 total automation points, 32,768 compiled playback note events. Probability/ratchets and repeating clips can increase event usage. Pattern lengths are 1–64 beats. Arrangement positions/endpoints must fit 0–4,096 beats; individual clips are at most 600 beats. Edits accept 1–128 operations per batch; the protocol message limit is 2 MiB. Split large work deliberately, keeping each batch valid by itself.

## 3. Tools

All names below are the unprefixed protocol names. Your AI host may expose names such as `mcp__pulse_studio__pulse_edit`.

| Tool | Arguments besides `expected_revision` | Behavior |
| --- | --- | --- |
| `pulse_status` | optional `include_history` boolean; no revision | Read live state and Undo/Redo counts; optionally return dated session checkpoints |
| `pulse_get_project` | none; no revision | Read committed editable song |
| `pulse_list_sounds` | none; no revision | Read factory names, engine and complete patches |
| `pulse_new_song` | required `title` text 1–200, `bpm` 30–300 | History backup, undoable new Song with 8 tracks and one empty pattern |
| `pulse_edit` | required `operations` array | Atomic model validation, playback compile and one undo commit |
| `pulse_transport` | required `action`: `play`, `pause`, `stop`, `seek`; `beat` for seek | Actual transport; Stop rewinds, pause retains position; seek must fit current song/active pattern |
| `pulse_save_project` | required `name`; optional `overwrite` boolean | Native collected save to `Projects/<name>.pulse`; returns absolute `path` |
| `pulse_export_audio` | required `name`; optional `overwrite` and `normalize` booleans | Start native Song export to `Exports/<name>.wav`; returns before completion |
| `pulse_undo` | optional `direction` undo/redo, integer `steps` 1-100 (0.59+) | Restore retained project checkpoints; defaults to one Undo, range failures preserve the complete history |
| `pulse_view` | required `panel`; optional `track`, `pattern` indices | Show/maximize `playlist`, `piano_roll`, `mixer`, `synth` or `channel_rack` |
| `pulse_snapshot` | none; no revision | Capture the real workspace as PNG image content |
| `pulse_mix_analysis` | action; expected_revision for start/cancel/apply_level | Offline measured report and verified one-fader correction; poll status |

Save/export basenames match `^[A-Za-z0-9][A-Za-z0-9 _-]{0,79}$`. Do not include a directory, extension, colon, slash, dot or non-ASCII letter. The app appends `.pulse` or `.wav`. Existing destinations are refused unless `overwrite: true` is explicit.

Export uses full Song, stereo 48 kHz, 24-bit PCM WAV, TPDF dither and a three-second tail. Two-pass -1 dBFS **sample-peak** normalization is the default; app/server 0.64+ accepts `normalize:false` to retain the mix/limiter level. No LUFS normalization or true-peak guarantee. These settings do not change the live project's gain. Even when PAT mode is selected, MCP export renders the **Playlist Song**; empty Playlist clips produce an empty song. Other native export formats/ranges/stems are available in the UI but not as MCP arguments.

In 0.33+, seek/resume and loop starts restore melodic notes whose gates still cover the playhead, including sampler source offsets and looping. A paused seek stays silent until Play. Fresh synth voices restart their envelopes; historical phase, filter/effect state, release tails and drums are not reconstructed. Matching active voices are reused instead of duplicated. Ordinary parameter edits do not retrigger notes. In 0.35+, extract_notes and merge_patterns expose selected-note extraction and layered/sequential merging. Used-pattern deletion remains a UI action.

## 4. Atomic edit operations

Every operation is an object containing the exact `op` string. Unknown properties are rejected. JSON numbers must be finite; booleans must be JSON booleans. Optional fields retain existing state unless stated below. Operations are evaluated in order on one candidate; an invalid later operation rejects the entire batch. Appends in an earlier operation can be referenced by their new zero-based indices later in the same batch.

### `set_song`

Optional fields: `title` (1–200 printable characters), `notes` (up to 16,000 characters, may be empty), `bpm` (30–300), `swing` (0–0.6), `master` (0–1), `delayFeedback` (0–0.8), `delayReturn` (0–1; 0.61+), `masterSoftClip` (boolean; 0.61+), `songMode`, `loop`, `metronome` (booleans).

### `add_track` / `set_track`

`add_track` requires `name` (1–80 printable characters). It appends a source channel when `instrument` or `preset` is supplied. A bus uses `bus: true`, may omit instrument/preset, and cannot contain notes. All existing patterns gain an empty lane. `set_track` requires an existing `track` index and cannot change source/bus identity. Neither operation deletes or reorders tracks.

Common optional fields: `name`, `instrument`, `preset`, `synth`, `params`, `effects`, `output`.

- `instrument` string: `kick`, `snare`, `hat`, `clap`, `open_hat`, `bass`, `pad`, `lead`, `analog`, `fm`, `wavetable`, `additive` (0.56+), `physical` (0.66+). There is no `sample` import operation. `analog`, `fm`, `wavetable`, `additive`, `physical` expose editable patches. The earlier `bass`, `pad`, `lead` are simpler generators.
- `preset`: exact case-sensitive factory sound name from `pulse_list_sounds`. Recall sets engine, full patch, amp attack/release and sound label. It preserves existing channel volume/pan/send/effects/routes. If instrument is also supplied it must match the preset's engine. Recall/switching instrument clears an existing sample assignment; Undo restores it.
- `synth`: partial patch fields below. Requires one of the four editable synth engines. Preset is applied before overrides; overrides label the result `Custom MCP sound`.
- `params`: partial channel fields below. Applied after preset; use this to set explicit envelope/gain/send values.
- `effects`: partial built-in effect groups below. Including a group enables it unless `enabled: false` is provided. Omitted settings in that group retain their prior values.
- `output`: -1 for Master, or the index of an existing **bus**. Whole-graph validation rejects cycles/self/invalid routes. Multiple sources can feed one bus. Buses can feed other buses.

Readback uses integer instrument IDs: kick 0, snare 1, hat 2, clap 3, open hat 4, bass 5, pad 6, lead 7, sample 8, Analog 9, FM 10, Wavetable 11, Additive 12, Resonator 13. Edit operations use strings. Readback mixer layouts differ from edit payloads; map fields deliberately rather than copying a complete readback track into `set_track`.

### `add_pattern` / `set_pattern`

`add_pattern` requires `name` (1–100 printable characters), optional `length` (1–64 beats; default 8); it appends empty lanes for all tracks. `set_pattern` requires existing `pattern`, optional `name`, `length`. Shortening rejects notes whose start is outside the new pattern. Replace those notes first, then shorten. Length units are beats, not sequencer steps: 16 steps = 4 beats, 32 steps = 8 beats.

In app/server 0.34+, both operations also accept optional `colour`, an integer 0–12: 0 Automatic, 1 Green, 2 Sage, 3 Teal, 4 Sky, 5 Blue, 6 Violet, 7 Lavender, 8 Rose, 9 Coral, 10 Orange, 11 Sand, 12 Grey. New patterns default to Automatic; omission on set_pattern retains the current colour. All shared Playlist instances display the pattern's colour. Colours affect presentation only and share the batch's Undo. Read `version` and omit this field for earlier apps/servers. Projects now save as schema 14; schemas 1–13 still load with missing colours defaulting to Automatic. Keep older project/History backups for use with 0.33 or earlier apps.

Example operation (inside a revision-checked pulse_edit.operations batch): `{"op":"set_pattern","pattern":0,"colour":4}`. Do not use marker colour numbers as pattern palette names: markers have a separate 0–7 palette. Local metronome sound/level/accent preferences are UI-only; set_song.metronome toggles playback clicks, and exports exclude them.

### `extract_notes` (0.35+)

Requires **native app and server 0.35+**. Required fields: pattern (existing 0–31), track (existing source channel 0–15), indices (1–512 **unique** zero-based integers 0–511 into that pattern/channel's current notes array). Use indices from a fresh pulse_get_project readback, in its array order, not time-sorted order or pitch numbers. Optional move is a boolean, default false (Copy); alignStart is a boolean, default false. Empty selection never means all notes; duplicate, stale or invalid indices reject the entire batch. Buses cannot hold notes. Earlier operations changing the lane affect these indices, so order the batch deliberately.

Appends/selects a new pattern with the source length, colour, and selected notes only on the source channel. Notes retain source index order regardless of supplied index order. alignStart subtracts the **earliest selected onset** from all copied notes, preserving relative timing and every other field, including long gates. It does not shrink the pattern. With move true, only selected source indices are removed; unselected identical notes remain. Shared source-pattern clips reflect that edit. No Playlist clips are created/relinked and Song/PAT mode is retained. To hear the new pattern in a Song, deliberately place its new index in the arrangement; avoid accidentally replacing unrelated clips via set_arrangement.

Example operation inside pulse_edit.operations: `{"op":"extract_notes","pattern":0,"track":7,"indices":[0,2],"move":false,"alignStart":true}`. This example assumes those indices exist. It appends one slot, so use the pre-edit pattern count as the new index, then verify readback. It is not idempotent.

### `merge_patterns` (0.35+)

Requires native app/server 0.35+. Required: distinct existing first and second pattern indices (0–31). Optional mode: **layered** (default), **sequential**. Optional policy: **keep_all** (default), **skip_exact**, **prefer_first**, **prefer_second**. Sequential only accepts keep_all; an incompatible pair fails atomically.

Appends/selects a new pattern without editing either source or any Playlist clip. It inherits first's colour. Layered puts one pass of both at original positions and uses their longer length. Sequential shifts second by first's full duration (including silence), retains all notes and uses the sum of lengths, at most 64 beats. Neither repeats a shorter source or time-stretches notes.

For layered policies, conflicts are per channel:

| policy | Exact behavior |
| --- | --- |
| keep_all | Keep every note and intentional double |
| skip_exact | Keep the first occurrence matching beat, length, pitch, velocity, probability and ratchets; remove exact duplicates across and within both sources |
| prefer_first | Keep every first-source note; omit second notes having exactly the same beat and pitch as any first note |
| prefer_second | Omit first notes matching any second note's exact beat and pitch; retain every second-source note |

Preference modes preserve duplicates within each retained source; changed velocity/gates/probability do not prevent an onset/pitch conflict. Different onsets, pitches and channels remain independent even when gates overlap. There is no fuzzy tolerance, averaging, quantization, or overlap splitting. Stable output order is retained first notes followed by retained second notes. Note capacity is checked **after** filtering; patterns still require one free slot and the native commit checks the current Song and the newly active PAT against 32,768 events before checkpointing. No partial edits or Undo entries on failure. Batch operations later can reference the appended index; a successful whole batch is one Undo.

Example operation: `{"op":"merge_patterns","first":0,"second":1,"mode":"layered","policy":"prefer_second"}`. Do not blindly replay after a timeout: inspect counts, activePattern and notes first, because each accepted call appends another independent pattern.

### `set_notes`

Required: `pattern`, `track`, `notes` array (0–512). This replaces the entire lane. Each note requires:

| Field | Range / meaning |
| --- | --- |
| `beat` | Start >=0 and strictly < pattern length, in beats |
| `length` | 0.01–64 beats |
| `pitch` | Integer 0–127; MIDI 60 = middle C (C4 here); +12 = octave |
| `velocity` | 0–1, not integer MIDI velocity; divide 1–127 by 127 |
| `probability` | Optional 0–1, default 1 |
| `ratchets` | Optional integer 1–8, default 1; subdivides the note gate |

Use explicit note lengths and rests; the API does not infer rhythm from a melody name. Notes do not belong on a bus. Note gates can extend past their pattern, but use intentionally bounded gates for predictable looping/clip tails.

### `set_arrangement`

Required: `clips` array (0–512), replacing the entire Playlist. Pattern clip fields are all required: `track` (existing source), `pattern` (existing pattern), `beat` (0–4096), `length` (0.0625–600). `beat + length` must not exceed 4096. A clip repeats its pattern lane for its duration; only onsets inside the clip are scheduled. Overlapping clips layer their events.

This operation currently cannot represent audio or automation clips read from `project.clips`; it would remove those when replacing the list. Do not feed readback fields such as `audioPath`, `sourceOffset`, `gain`, `points`, `automationTarget` or `automationTrack` into it. An empty list explicitly clears the Playlist.

### `set_markers`

Required: `markers` array (0–128), replacing all Song markers. Each requires `id` (unique 1–80 ASCII letters/digits/underscore/hyphen only), `name` (1–80), `beat` (0–4096), `colour` (integer 0–7). Optional `endBeat` defines a section if >=0; section duration must be at least 0.25 beats, endpoint <=4096. Omit it or use -1 for a point marker. Replacing markers clears a linked section loop. Names need not be unique; IDs must be.

### `section_clips` (0.36+)

Requires **native app and server 0.36+**. Required fields: section (existing Song section **id**, printable string 1–80; use the id from project.markers, not its display name) and destination (finite start **beat**, 0–4096). Optional move is boolean, default false (Copy); allowOverlap is boolean, default false. No other properties are accepted. The UI uses 1-based bars; this operation uses beats: beat 16 is bar 5 in 4/4.

The operation selects all pattern/audio/automation clips wholly inside the source section's half-open [beat,endBeat) interval. A clip touching only the outer edges is outside. **Any crossing clip rejects the whole batch**, including whole-song automation spanning the section: adjust/split the source through a supported UI workflow first. It never silently crops or skips a crossing clip. Point markers and sections with no included clips fail. The full destination span must fit 0–4096 and differ from the source start.

Copy appends independent clip objects in original source-index order. Move retains their original indices and changes positions without allocating clip slots. Every included clip retains its relative beat offset, display lane and duration; pattern references remain shared; audio file references, second-based source offsets, gain and fades remain; automation targetTrack/parameter, local points and hold segments remain. No source files, notes/instruments, markers or loop boundaries are edited. Song/PAT mode and active pattern are retained. To move the section marker itself use a separate supported marker edit; this operation deliberately edits clips only.

Default overlap rejection checks any existing clip on the same Playlist lane and any automation clip targeting the same parameter/channel even on a different display lane. Master automation matches Master regardless of its unused automationTrack. Edge-touching is allowed; the old positions of moved source clips are ignored. Intentional overlaps already within the source group are retained. With allowOverlap true, all destination clips are retained and layering is permitted, with existing automation ordering: copied clips append and later automation wins, while moved clips keep their original ordering. This flag never means replacement, erasure or ripple editing.

Copy must fit 512 total clips, 64 automation clips and 4096 total points; Move can operate at those capacities. Native commit compiles the complete resulting **Song even when PAT is selected** and rejects event overflow before backup/checkpoint. Recording/modal/preview/export guards, current expected_revision, atomic batch rollback and one Undo still apply. Later operations see the transformed arrangement; a later set_arrangement replaces it entirely, so sequence the batch deliberately. Result counts/positions are inspected with pulse_get_project; there is no new tool or extra result-statistics contract.

Example operation inside pulse_edit.operations: `{"op":"section_clips","section":"verse","destination":16,"move":false,"allowOverlap":false}`. It assumes a valid whole-clip verse range exists and its destination is empty. Read fresh project state first. Copy is non-idempotent: after a timeout inspect clip positions/counts before retrying. Multi-section selection, partial-clip transforms, ripple edits and automatic marker relocation are not supported.

### `add_automation`

Appends one automation clip. Required fields: `track` (Playlist display lane, including a bus), `target`, `targetTrack`, `beat` (0–4096), `length` (0.0625–600, clip end <=4096), and `points` (2–256). Each point requires local `beat` and normalized `value` (0–1), optional `hold` boolean. Point times must strictly increase and include exactly 0 and clip length. `hold: true` on a point holds its value until the next point; otherwise the segment interpolates linearly. At most 64 automation clips and 4096 total points.

| `target` | Parameter | `targetTrack` | Normalized value mapping |
| --- | --- | --- | --- |
| 0 | Volume | source or bus index | 0–1 maps 0–1.5 channel gain |
| 1 | Pan | source or bus index | 0 left, 0.5 center, 1 right |
| 2 | Channel cutoff | source or bus index | 60–20000 Hz logarithmically |
| 3 | Saturation/drive | source or bus index | 0–1 |
| 4 | Delay send | source or bus index | 0–1 maps 0–0.8 |
| 5 | Master volume | use -1 | 0–1 master gain |

Playlist `track` and linked `targetTrack` have different roles. Automation applies in Song mode only, only within its clip. The last-added overlapping automation clip for a parameter wins. A later `set_arrangement` discards previously appended automation, so order operations carefully.

## 5. Sound parameter reference

### Channel `params`

| Field | Range / unit |
| --- | --- |
| `volume` | 0–1.5 linear gain |
| `pan` | -1 left to +1 right |
| `cutoff` | 60–20000 Hz, separate post-generator channel tone filter |
| `drive` | 0–1 saturation |
| `send` | 0–0.8 built-in delay send; not a bus send matrix |
| `attack` | 0.001–2 seconds |
| `release` | 0.02–3 seconds |

In 0.45+, optional `sampleRoot` is an integer 0–127 and `sampleTune` is -100..100 cents, positive raising sampler playback. Defaults are 60 and 0. These stored fields affect only sampler channels; no sample file path or native pitch-detection operation is exposed.

New default channels can have nonzero delay sends. Set `send: 0` explicitly for a dry sound. Changing synth `cutoff` does not change channel `params.cutoff`; both filters affect the result.

### `synth` fields (partial overrides)

| Fields | Range / meaning |
| --- | --- |
| `wave1`, `wave2` | Integer 0 sine, 1 triangle, 2 saw, 3 pulse (Analog) |
| `coarse` | Oscillator 2 offset, integer -24..24 semitones |
| `detune` | Oscillator 2 detune -50..50 cents |
| `mix` | 0..1 oscillator/operator mix |
| `pulse` | 0.05..0.95 pulse duty |
| `sub` | 0..1 sub oscillator amount |
| `noise` | 0..0.5 noise amount |
| `decay` | 0.01..4 seconds amplitude decay |
| `sustain` | 0..1 amplitude sustain level |
| `cutoff` | 40..20000 Hz generator filter |
| `resonance` | 0..1 |
| `filterEnv` | -6..6 octaves of initial filter offset |
| `filterDecay` | 0.01..4 seconds |
| `fmRatio` | 0.25..16 modulation frequency ratio (FM) |
| `fmIndex` | 0..12 modulation depth (FM) |
| `fmDecay` | 0.01..4 seconds modulation decay (FM) |
| `lfoRate` | 0.1..20 Hz |
| `lfoPitch` | 0..100 cents vibrato depth |
| `lfoFilter` | 0..4 octaves |
| `harmonics` | App/server 0.56+: exactly 16 finite numbers 0-1; complete replacement, harmonic 1 to 16, Additive only |
| `table` | Integer 0 Classic, 1 Hollow, 2 Vocal, 3 Metal, 4 Organ, 5 Glass, 6 Digital, 7 Soft (Wavetable) |
| `tablePosition` | 0..1 between the family's four frames |
| `tableEnv` | -1..1 initial table-position offset |
| `tableDecay` | 0.01..8 seconds |
| `tableLfo` | 0..1 table-position modulation depth |

All engines store the full patch, but FM fields affect FM, table fields affect Wavetable harmonics affect Additive, and the complete `physical` group affects Resonator (see Physical sound design below). Setting a stored irrelevant field does not add that synthesis method to Analog. Amplitude attack/release live in `params`, not `synth`.

### `effects` groups (partial overrides)

| Group | Fields |
| --- | --- |
| `eq` | `enabled` boolean; `low`, `mid`, `high`: -18..18 dB |
| `reverb` | `enabled` boolean; `mix`, `room`, `damping`: 0..1 |
| `chorus` | `enabled` boolean; `mix` 0..1, `rate` 0.05..8 Hz, `depth` 0..1, `delay` 5..35 ms, `feedback` -0.8..0.8, `width` 0..1 |
| `stereo` | `enabled`, `mono`, `swap`, `invertLeft`, `invertRight`: booleans; `width` 0..2 (1 neutral); `trim` -24..6 dB |

App/server 0.62 exposes compressor/flanger/phaser/order for channels and Master, as detailed in the Master insert section below. Older servers do not expose those fields; restart after updating. Existing settings are preserved by unrelated edits. There is no per-parameter synth automation in this API yet.

## 6. Worked new-song example

This example is an original four-bar synth phrase. It does not transcribe an existing song. Run the steps only for an authorized new-song request. The previous session is backed up; the calls still change the visible session.

1. `pulse_status({})` -> read its revision R. Call `pulse_new_song({"expected_revision": R, "title": "Evening lights", "bpm": 120})`.
2. Read `pulse_status({})` again. Pass its revision to `pulse_edit` with the following `operations` array:

```json
[
  {"op":"set_song","songMode":true,"loop":false,"metronome":false,"master":0.7},
  {"op":"set_pattern","pattern":0,"name":"Main phrase","length":8},
  {"op":"set_track","track":7,"name":"Soft lead","preset":"Soft whistle","params":{"volume":0.38,"send":0,"attack":0.01,"release":0.18}},
  {"op":"set_track","track":5,"name":"Bass","preset":"Round sub","params":{"volume":0.45,"send":0}},
  {"op":"set_notes","pattern":0,"track":7,"notes":[
    {"beat":0,"length":0.8,"pitch":72,"velocity":0.8},
    {"beat":1,"length":0.8,"pitch":76,"velocity":0.72},
    {"beat":2,"length":1.6,"pitch":79,"velocity":0.8},
    {"beat":4,"length":0.8,"pitch":77,"velocity":0.75},
    {"beat":5,"length":0.8,"pitch":76,"velocity":0.7},
    {"beat":6,"length":1.6,"pitch":72,"velocity":0.8}
  ]},
  {"op":"set_notes","pattern":0,"track":5,"notes":[
    {"beat":0,"length":1.8,"pitch":36,"velocity":0.65},
    {"beat":2,"length":1.8,"pitch":43,"velocity":0.6},
    {"beat":4,"length":1.8,"pitch":41,"velocity":0.65},
    {"beat":6,"length":1.8,"pitch":36,"velocity":0.6}
  ]},
  {"op":"set_arrangement","clips":[
    {"track":7,"pattern":0,"beat":0,"length":16},
    {"track":5,"pattern":0,"beat":0,"length":16}
  ]},
  {"op":"set_markers","markers":[{"id":"main","name":"Main phrase","beat":0,"endBeat":16,"colour":2}]},
  {"op":"add_automation","track":6,"target":5,"targetTrack":-1,"beat":14,"length":2,"points":[{"beat":0,"value":0.7},{"beat":2,"value":0}]}
]
```

3. `pulse_get_project({})`: verify two populated note lanes, two pattern clips and one Master fade, 16-beat arrangement. The 8-beat pattern repeats twice because each clip lasts 16 beats. Track 6 is merely the fade's Playlist display lane; its notes are not required.
4. Fresh status -> `pulse_view` with `panel: "playlist"`; optional fresh status -> `panel: "piano_roll", track: 7, pattern: 0`. `pulse_snapshot({})` shows the result.
5. Fresh status -> `pulse_save_project` with `name: "Evening lights"`. Fresh status -> `pulse_export_audio` with the same name. Poll read-only status with a reasonable delay until complete. Return the reported absolute paths.
6. Fresh status -> `pulse_transport` with `action: "play"`. Check position advances. `stop` rewinds; `pause` retains the current beat. If restarting from another position, read status and `seek` to 0 before Play.

For a bus, append `{"op":"add_track","name":"Space bus","bus":true,"params":{"volume":1,"cutoff":20000,"send":0},"effects":{"reverb":{"mix":0.2,"room":0.7}}}`. It becomes index 8 immediately after `pulse_new_song`'s eight sources. Route a source with `{"op":"set_track","track":7,"output":8}`. Leave the bus output at -1 for Master. This routes the source's full output; it is not an extra parallel send. Do not put pattern clips or notes on the bus.

For larger songs, plan bar/beat positions, chord progression, source sounds, note lanes, sections and dynamic transitions first. Use separate patterns where notes differ, repeat clips where they do not, then verify compiled event capacity. Avoid piling every instrument onto one identical melody or treating a visually populated Playlist as evidence of musical quality.

## 7. Troubleshooting and recovery

- **No tools / server unavailable:** check Python executable, absolute script path, host registration and server logs; reconnect host if needed. Confirm the script is run by Python, not treated as a network URL.
- **Open app / enable control error:** enable OPTIONS > Local MCP control and verify the workspace. The client reads a session for that workspace; opening a `.pulse` file in another directory does not change the app workspace.
- **Revision changed:** reread the project, preserve user changes and rebuild only the intended modification. Do not repeatedly submit the previous revision.
- **Timed out:** first read status/project. The app may already have committed. Compare title, counts, specific notes/clips and save/export state. Retry only work proven not applied. Do not automatically repeat new-song, append operations or Undo.
- **Busy / take / modal / preview / export:** read-only status/project still work. Let the existing action finish; an unapplied note preview needs the user's Apply/Cancel decision. Do not bypass guards through files.
- **Silent Song:** inspect clips, Song mode, correct channel lane, notes/pitches/velocity/probability, mute/solo, gain, bus routes and missing sample assignments. A source connected to a muted bus can be silent despite an active source meter.
- **Playback does not start:** check `activeOutputs`, active device in F10, callbacks/position and current status. The server cannot configure the audio device. At the end of a non-looping song, rewind/seek before checking again.
- **Wrong melody:** compare intended pitches, onsets and lengths with readback. If they match the sent payload, correct the transcription/rhythm. Output timbre, gate/release, tempo and overlapping lanes can also change perceived phrasing.
- **Export path but no completed file:** poll `exporting`, `exportProgress`, `status`. Cancellation/failure is not completion. Do not call it delivered until native completion; optionally inspect the WAV read-only for duration, sample format, finite data and clipping.
- **API cannot do the desired edit:** state the boundary. There is no MCP arbitrary code, filesystem/path import, track/pattern deletion, redo, mute/solo setter, audio-clip editor, tempo map, personal-library manager, general plugin host or keyboard-capture control. Use supported operations or an explicitly authorized UI workflow; do not claim unsupported arguments worked.
- **Recover a previous session:** `pulse_undo` defaults to restoring one accepted batch and can undo new-song creation. App/server 0.59+ can read dated states with `pulse_status(include_history=true)` and move several steps in either direction with `pulse_undo`; see Session history below. Read status before every move. History backups are `data/History/before-mcp-*.pulse`; the returned `backup` path identifies the most recent control backup. Opening one uses the native UI; no MCP open-project tool is currently exposed.

Control is local file IPC, not a security sandbox for programs running as the same OS user. Session metadata contains a local token; do not expose it in chat or copy it into examples. Disabling control invalidates the session. The standard server handles session rotation, per-request expiry and replay protection. Keep source assets available for History snapshots; use native collected saves for portable projects.

Native-only additions in 0.37: waveform sample-rate conversion and cross-rate audio clipboard paste. No waveform-edit MCP operation exists. Do not add unsupported operations to pulse_edit.

Native-only additions in 0.39: Copy notes to pattern, Paste at original positions and Paste at start. No new MCP operation is introduced. For lane copying use pulse_get_project followed by set_notes with a fresh revision, remembering that set_notes replaces the entire destination lane; explicitly include pre-existing destination notes when appending.

Native-only additions in 0.40: Browser sample-file favorites, sample context actions, local-file drag assignment and name/category/path filtering. No new MCP tools or operations; the local favorites catalog is separate from project state.

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

Choose the actual track from readback; do not reuse an example index or revision blindly. Instrument and preset must match; preset recall runs before overrides. Read back all 16 levels with float tolerance and verify notes/routes remain as intended. Save/export/Undo use existing tools; resolve native note previews first. Current project writes use schema 23 and require app 0.65+, reading schemas 1-22 with neutral defaults for newly introduced fields; pre-17 synths gain neutral harmonic defaults. Additive sounds/custom harmonic data and libraries containing them or additive favorites require sound/library schema 3, reading 1-2; older-engine files without new harmonic data still write schema 2. Keep pre-update project/History backups for old apps. Harmony presets retain their separate schema 1/2 format. The tool count remains eleven.


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



## Offline mix analysis (0.68+)

Use `pulse_mix_analysis(action="start", expected_revision=<fresh status revision>)`, then poll `pulse_mix_analysis(action="status")` until `mixAnalysis.busy` is false. It measures a snapshot of the current Song/PAT, not a selected clip or export range. Analysis does not edit project data or add Undo. A successful report has `current:true`; failed/cancelled jobs report their message and must not be treated as completed measurements. `pulse_status.mixAnalysisBusy` is a compact worker flag.

The full state includes `busy`, `applying`, `progress` (0–1), `sourceRevision`, `current`, `canApplyLevel`, `report`, `message` and the local `reportPath`. A report contains `schema:1`, algorithm, 48 kHz sample rate, frame count/duration including its two-second tail, Song/PAT flag, complete `projectHash`, raw source SHA-256 identities, per-channel linear `peak`, `rms`, `dc`, integer `fullScaleSamples`, stereo `correlation`/`correlationValid`, three normalized `bandFraction` values (20–200/200–4000/4000–20000 Hz), Master gain before/after and positive `reductionDb`. Its `summary` supplies readable units and observations. Silence gives zero linear levels; the summary floors dBFS at -160. Invalid correlation is explicitly flagged. Report `canReduce` describes that measured snapshot; state `canApplyLevel` additionally checks live eligibility.

Explain the proposed reduction before applying within the user's authorized mix-editing scope. For an eligible current report, read fresh status and call `pulse_mix_analysis(action="apply_level", expected_revision=...)`. Poll until not busy, require an applied message and read the project back. Apply performs a fresh render, compares the original source bytes and project identity, verifies a -1 dBFS sample ceiling, then changes only base Master gain as one Undo. It never changes EQ, dynamics, notes, instruments or arrangement. Saturation or Master-gain automation blocks this linear correction. Any intervening edit invalidates the result. Cancel uses `action="cancel"` and a fresh revision; it does not revert an already completed Apply. A timeout is ambiguous: inspect state/project before retrying.

Reports persist outside project files and reopen read-only after restart. Analyze again to enable correction. This is a bounded ten-minute, local sample-peak/RMS/FFT check, not LUFS, true peak, masking, genre judgment, mastering-chain generation or audio listening. It excludes monitoring/click/Browser previews. No audio-file path argument or external service is accepted. Use `pulse_export_audio(normalize=false)` to preserve verified mix levels in export; the default normalized export performs its own peak normalization.


## Native waveform restoration (0.69)

The native waveform editor adds DC-offset removal, 50/60 Hz hum filtering and isolated-click repair, with background preview, Original/Restored audition and one waveform Undo on Apply. It does not add a remote processing tool or file-path capability. `pulse_status` exposes boolean `restorationBusy` and `restorationPreviewReady` diagnostics. Existing project operations, schema 24 and the twelve-tool list remain intact. Do not simulate restoration by rewriting source audio or mailbox/project files. Use native controls for this operation and explicitly assign the edited copy to a sampler/Playlist before expecting it in music/export. Details and limits are in the README's **Audio restoration (0.69)** section.

## Native Browser tempo audition (0.70)

Sample context menus add source BPM, project-tempo speed and repeat; Local-file clicks use saved per-file settings; favorites/project samples retain click-to-assign and offer saved-settings context audition. This uses resampling, so pitch follows speed. Preview is independent of the musical project, input recordings and file exports. It does not add a remote preview tool, sample-stretch operation or file-path access. Do not change channel tuning or rewrite song/mailbox files to simulate these preferences. Use native controls; see README **Tempo-synced sample audition (0.70)** for exact interaction and limits. The twelve tools and schema 24 stay unchanged.

## Native reference comparison (0.71)

TOOLS / Ctrl+P > Reference track comparison selects local reference audio and equal-duration mix/reference passages. Native A/B starts each snapshot from its beginning, with RMS attenuation matching or original relative levels and shared headroom protection. It never changes the song or includes reference audio in an export. Saved reports reopen read-only; rebuild before audition. `pulse_status` adds boolean `referenceComparisonBusy` and `referenceComparisonReady` diagnostics. There is no reference-authoring or arbitrary file-path MCP tool: use native controls and do not rewrite project/mailbox files to simulate comparison. See README **Reference track comparison (0.71)** for limits; schema 24 and twelve tools stay unchanged.

## Native audio-to-notes conversion (0.72)

Waveform / TOOLS / Ctrl+P > Audio to notes (monophonic) analyzes a selected clean melody, reviews periodicity confidence, auditions Original/Notes and explicitly applies a new editable PAT pattern. This is approximate monophonic transcription, not exact extraction from mixed recordings, drums or chords. Existing music and source files stay intact; confidence/source selection lives in a separate local report. `pulse_status` adds boolean `transcriptionBusy` and `transcriptionPreviewReady`. There is no transcription-authoring or arbitrary-path MCP operation; use native controls and do not rewrite project/mailbox files to simulate the feature. After native Apply, ordinary `pulse_get_project` / `pulse_edit` / MIDI export can inspect/edit/use its notes with fresh revision and indices. See README **Audio to notes / monophonic transcription (0.72)** for declared limits and synthetic evaluation targets. Twelve tools and schema 24 stay unchanged.


## Native spectral band editing (0.73)

Waveform > Spectral / Edit > Spectral band edit / Ctrl+P opens a native frequency band, attenuation, FFT resolution and strength editor for the current waveform time selection. Process preview builds independent float32 audio and Original/Edited stereo-power spectrograms; audition/Stop and explicit waveform Apply/Undo do not change music. To sampler/To Playlist assigns audio through ordinary musical edits. This is rectangular attenuation, not source separation, painting or spectral healing. See README **Spectral band editing (0.73)** for resolution, selection, source and persistence limits. The existing boolean `restorationBusy` / `restorationPreviewReady` cover all four restoration modes, including spectral. There is no spectral-authoring or arbitrary-path MCP operation. Use native controls; do not rewrite files/mailboxes to imitate an unsupported operation. After native assignment, obtain a fresh project/revision before ordinary MCP edits. Twelve tools and schema 24 remain unchanged.


## Native pattern notation (0.74)

TOOLS / Piano roll menu / Ctrl+P > Pattern notation opens a read-only current-pattern channel snapshot. Native selectors change the viewed channel/clef independently of actual project selection; playback may continue. Export MusicXML or standalone SVG through the local native chooser. The declared sixteenth-note 4/4 conversion can change written timing and reports its rounding/clipping; authored music is unchanged. Probability/ratchets/conditions/swing/tuning/expression are not realized into a performance score. See README **Pattern notation / sheet music (0.74)** for pitch, voice, size and future-work bounds. There is no notation read/write/view/export MCP operation. Use native controls; do not manufacture an unsupported tool call or rewrite mailbox/project files to emulate one. Twelve tools and schema 24 remain unchanged. Native notation/export does not increment the musical revision; still read fresh status before subsequent MCP writes.

## Native retrospective input history (0.75)

TOOLS / Recording settings / Ctrl+P > Retrospective audio history provides explicit native opt-in dry input retention. It is OFF at every startup; preferences never enable the microphone. Source, duration and preallocated ring budget are native controls. Complete contiguous Song callback ranges can be frozen, reviewed, auditioned and explicitly placed as one Undo; see README **Retrospective audio history (0.75)** for exact limits and future work. Native capture/freeze/audition alone does not change the musical revision; To Playlist does. Read fresh state before subsequent MCP writes.

`pulse_status` exposes boolean `audioHistoryEnabled`, nonnegative integer `audioHistoryFrames`, boolean `audioHistoryBusy` and boolean `audioHistoryReady`. Enabled means retention was explicitly armed, not that transport/input is currently eligible; Frames counts the latest retained segment. Busy is a freeze worker; Ready is a loaded unapplied preview (source/revision guards may still prevent Apply). A paused buffer has Enabled false and can still have Frames > 0. These are diagnostics only. There is no MCP enable/start/stop/freeze/recall operation; use native controls. Do not manufacture tool calls or rewrite preference/mailbox/project files to imitate unsupported capture. Twelve tools and schema 24 remain unchanged.
