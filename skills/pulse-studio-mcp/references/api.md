# Pulse Studio local MCP reference (0.37 / eleven-tool base from 0.31)

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
| `pulse_status` | none; no revision | Read current live state |
| `pulse_get_project` | none; no revision | Read committed editable song |
| `pulse_list_sounds` | none; no revision | Read factory names, engine and complete patches |
| `pulse_new_song` | required `title` text 1–200, `bpm` 30–300 | History backup, undoable new Song with 8 tracks and one empty pattern |
| `pulse_edit` | required `operations` array | Atomic model validation, playback compile and one undo commit |
| `pulse_transport` | required `action`: `play`, `pause`, `stop`, `seek`; `beat` for seek | Actual transport; Stop rewinds, pause retains position; seek must fit current song/active pattern |
| `pulse_save_project` | required `name`; optional `overwrite` boolean | Native collected save to `Projects/<name>.pulse`; returns absolute `path` |
| `pulse_export_audio` | required `name`; optional `overwrite` boolean | Start native Song export to `Exports/<name>.wav`; returns before completion |
| `pulse_undo` | none | Undo last whole project change; there is currently no MCP redo tool |
| `pulse_view` | required `panel`; optional `track`, `pattern` indices | Show/maximize `playlist`, `piano_roll`, `mixer`, `synth` or `channel_rack` |
| `pulse_snapshot` | none; no revision | Capture the real workspace as PNG image content |

Save/export basenames match `^[A-Za-z0-9][A-Za-z0-9 _-]{0,79}$`. Do not include a directory, extension, colon, slash, dot or non-ASCII letter. The app appends `.pulse` or `.wav`. Existing destinations are refused unless `overwrite: true` is explicit.

Export settings through this API are fixed: full Song, stereo 48 kHz, 24-bit PCM WAV, two-pass -1 dBFS **sample-peak** normalization, TPDF dither, three-second tail. No LUFS normalization or true-peak guarantee. These settings do not change the live project's gain. Even when PAT mode is selected, MCP export renders the **Playlist Song**; empty Playlist clips produce an empty song. Other native export formats/ranges/stems are available in the UI but not as MCP arguments.

In 0.33+, seek/resume and loop starts restore melodic notes whose gates still cover the playhead, including sampler source offsets and looping. A paused seek stays silent until Play. Fresh synth voices restart their envelopes; historical phase, filter/effect state, release tails and drums are not reconstructed. Matching active voices are reused instead of duplicated. Ordinary parameter edits do not retrigger notes. In 0.35+, extract_notes and merge_patterns expose selected-note extraction and layered/sequential merging. Used-pattern deletion remains a UI action.

## 4. Atomic edit operations

Every operation is an object containing the exact `op` string. Unknown properties are rejected. JSON numbers must be finite; booleans must be JSON booleans. Optional fields retain existing state unless stated below. Operations are evaluated in order on one candidate; an invalid later operation rejects the entire batch. Appends in an earlier operation can be referenced by their new zero-based indices later in the same batch.

### `set_song`

Optional fields: `title` (1–200 printable characters), `notes` (up to 16,000 characters, may be empty), `bpm` (30–300), `swing` (0–0.6), `master` (0–1), `delayFeedback` (0–0.8), `songMode`, `loop`, `metronome` (booleans).

### `add_track` / `set_track`

`add_track` requires `name` (1–80 printable characters). It appends a source channel when `instrument` or `preset` is supplied. A bus uses `bus: true`, may omit instrument/preset, and cannot contain notes. All existing patterns gain an empty lane. `set_track` requires an existing `track` index and cannot change source/bus identity. Neither operation deletes or reorders tracks.

Common optional fields: `name`, `instrument`, `preset`, `synth`, `params`, `effects`, `output`.

- `instrument` string: `kick`, `snare`, `hat`, `clap`, `open_hat`, `bass`, `pad`, `lead`, `analog`, `fm`, `wavetable`. There is no `sample` import operation. `analog`, `fm`, `wavetable` expose editable patches. The earlier `bass`, `pad`, `lead` are simpler generators.
- `preset`: exact case-sensitive factory sound name from `pulse_list_sounds`. Recall sets engine, full patch, amp attack/release and sound label. It preserves existing channel volume/pan/send/effects/routes. If instrument is also supplied it must match the preset's engine. Recall/switching instrument clears an existing sample assignment; Undo restores it.
- `synth`: partial patch fields below. Requires one of the three editable synth engines. Preset is applied before overrides; overrides label the result `Custom MCP sound`.
- `params`: partial channel fields below. Applied after preset; use this to set explicit envelope/gain/send values.
- `effects`: partial built-in effect groups below. Including a group enables it unless `enabled: false` is provided. Omitted settings in that group retain their prior values.
- `output`: -1 for Master, or the index of an existing **bus**. Whole-graph validation rejects cycles/self/invalid routes. Multiple sources can feed one bus. Buses can feed other buses.

Readback uses integer instrument IDs: kick 0, snare 1, hat 2, clap 3, open hat 4, bass 5, pad 6, lead 7, sample 8, Analog 9, FM 10, Wavetable 11. Edit operations use strings. Readback mixer layouts differ from edit payloads; map fields deliberately rather than copying a complete readback track into `set_track`.

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
| `table` | Integer 0 Classic, 1 Hollow, 2 Vocal, 3 Metal, 4 Organ, 5 Glass, 6 Digital, 7 Soft (Wavetable) |
| `tablePosition` | 0..1 between the family's four frames |
| `tableEnv` | -1..1 initial table-position offset |
| `tableDecay` | 0.01..8 seconds |
| `tableLfo` | 0..1 table-position modulation depth |

All engines store the full patch, but FM fields affect FM and table fields affect Wavetable. Setting a stored irrelevant field does not add that synthesis method to Analog. Amplitude attack/release live in `params`, not `synth`.

### `effects` groups (partial overrides)

| Group | Fields |
| --- | --- |
| `eq` | `enabled` boolean; `low`, `mid`, `high`: -18..18 dB |
| `reverb` | `enabled` boolean; `mix`, `room`, `damping`: 0..1 |
| `chorus` | `enabled` boolean; `mix` 0..1, `rate` 0.05..8 Hz, `depth` 0..1, `delay` 5..35 ms, `feedback` -0.8..0.8, `width` 0..1 |
| `stereo` | `enabled`, `mono`, `swap`, `invertLeft`, `invertRight`: booleans; `width` 0..2 (1 neutral); `trim` -24..6 dB |

The native UI also has compressor/flanger/phaser/effect-chain ordering, but current MCP edit schemas do not expose those settings. Do not invent arguments; existing settings are preserved by unrelated edits. There is no per-parameter synth automation in this API yet.

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
- **Recover a previous session:** `pulse_undo` restores one accepted batch at a time and can undo new-song creation. Read status before each Undo. History backups are `data/History/before-mcp-*.pulse`; the returned `backup` path identifies the most recent control backup. Opening one uses the native UI; no MCP open-project tool is currently exposed.

Control is local file IPC, not a security sandbox for programs running as the same OS user. Session metadata contains a local token; do not expose it in chat or copy it into examples. Disabling control invalidates the session. The standard server handles session rotation, per-request expiry and replay protection. Keep source assets available for History snapshots; use native collected saves for portable projects.

Native-only additions in 0.37: waveform sample-rate conversion and cross-rate audio clipboard paste. No waveform-edit MCP operation exists. Do not add unsupported operations to pulse_edit.
