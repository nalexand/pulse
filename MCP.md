# Pulse Studio local MCP

For model-independent operating instructions, use [skills/pulse-studio-mcp/SKILL.md](skills/pulse-studio-mcp/SKILL.md) together with its [detailed connection/API reference](skills/pulse-studio-mcp/references/api.md). These files are included in the portable package and corresponding source. They document every accepted field/range, replacement behavior, bus/automation semantics, error handling and a tested composition example. No application source reading is needed. Version 0.37 retains the eleven 0.31 tools, reads schemas 1–13 and writes schema-14 projects with pattern colours; pattern/automation clip lengths cap at the native 600-beat limit. Status adds `chasedNotes` (0.33+), a cumulative count of restored held notes, not a current voice count.

Pulse Studio 0.31 provides eleven tools for controlling the **open application**. The app applies edits on its message thread, validates the complete candidate, compiles playback, then publishes one undoable change. The client does not replace project files behind the app.

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
| `pulse_list_sounds` | 56 factory patches with complete editable synth settings |
| `pulse_new_song` | Back up the previous session and create a new Song-mode project, retaining Undo |
| `pulse_edit` | Apply 1–128 operations atomically as one project undo |
| `pulse_transport` | Play, pause, stop/rewind and beat seek |
| `pulse_save_project` | Native save with asset collection under `Projects/<name>.pulse` |
| `pulse_export_audio` | Background Song WAV export under `Exports/<name>.wav` |
| `pulse_undo` | Undo one complete project edit |
| `pulse_view` | Show/maximize Playlist, Piano roll, Mixer, Synth or Channel rack |
| `pulse_snapshot` | PNG image of the live workspace |

Call status/get-project before **every write** and supply its `revision` as `expected_revision`. A UI edit can invalidate it; reread and reconcile before retrying. Do not blindly replay timed-out mutations, since an edit may have completed before its response was lost.

Seek, resume and loop starts restore melodic notes whose gates are still held. Samplers use the corresponding source position; fresh synth voices restart their envelopes. Historical synth/effect state, release tails and drums are not reconstructed. A paused seek remains silent until Play. In app/server 0.35+, extract_notes and merge_patterns expose selected-note extraction and layered/sequential merging as pulse_edit operations. Confirmed used-pattern deletion remains UI-only.

Edits support `set_song`, `add_track`, `set_track`, `add_pattern`, `set_pattern`, `extract_notes`, `merge_patterns`, `set_notes`, `set_arrangement`, `set_markers`, `section_clips` and `add_automation`. Indices are zero-based; add operations append. In 0.34+, add_pattern/set_pattern accept optional `colour`: integer 0–12 (0 Automatic; 1 Green, 2 Sage, 3 Teal, 4 Sky, 5 Blue, 6 Violet, 7 Lavender, 8 Rose, 9 Coral, 10 Orange, 11 Sand, 12 Grey). Omission retains an existing colour or defaults a new pattern to Automatic. It changes every instance through the shared pattern, with normal atomic validation/Undo. Omit this field when controlling older versions. `set_notes` replaces one pattern/channel lane, and `set_arrangement` explicitly replaces the entire clip list. Start a separate song when composing from scratch. `pulse_new_song` creates eight default source channels and one pattern; customize them and append channels as needed.

Track settings include Analog/FM/Wavetable patches, named factory sounds, gain/pan/filter/saturation/send/envelopes, EQ/reverb/chorus/stereo and output buses. Synth/effect objects accept partial overrides; unknown or invalid fields are rejected. Existing limits remain: 16 tracks, 32 patterns, 512 notes per lane, 512 clips, 128 markers and 32,768 compiled events. Larger edits can be divided into deliberate batches. Buses process audio and have no note lanes.

Automation target numbers are 0 volume, 1 pan, 2 cutoff, 3 saturation, 4 delay send, 5 Master. Values are normalized 0–1; points must increase in time and include both endpoints. Track automation needs a `targetTrack`; Master uses -1. The Playlist lane is supplied separately as `track`.

Writes are blocked during a take, active recording, modal dialog, unapplied note preview or export. Saves/exports accept a simple basename and refuse existing destinations unless `overwrite: true` is explicit. Export returns immediately; poll status until `exporting` is false and confirm the completion message. Exports use stereo 48 kHz/24-bit WAV, -1 dBFS **sample-peak** normalization, TPDF dither and a three-second tail. This is not loudness normalization or true-peak limiting.

## Local access and recovery

There is no TCP listener, remote server, account, upload, telemetry or arbitrary code tool. The bridge uses `data/control/session.json` and per-session request/response folders. These contain a local session token. Any process with access to this workspace can read it: use ordinary OS account/file permissions to control local access. Session rotation, request expiry and replay protection prevent stale or duplicate commands; they do not isolate untrusted programs running as your account.

Before the first control edit, and before starting another song, the app saves an absolute-asset project snapshot under `data/History/before-mcp-*.pulse`. This preserves references without copying external assets; keep the original audio files available. New-song creation and every accepted batch are also in normal Undo. The current project schema is 14; schemas 1–13 remain readable. Older apps require a retained pre-update project/History backup. Control leaves the existing 0.29 recovery protocol compatible.

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
