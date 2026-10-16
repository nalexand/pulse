---
name: pulse-studio-mcp
description: Compose, edit, design sounds, arrange, play, save and export music in the open local Pulse Studio app through its MCP tools. Use for controlling Pulse Studio from an AI client, including creating editable songs; not for FL Studio automation or application source development.
---

# Pulse Studio through MCP

Control the actual open Pulse Studio workstation, not a project JSON file behind it. This skill is model/client independent: use the tools exposed by your MCP host. A host may prefix names with its server namespace; the underlying tool names are `pulse_status`, `pulse_edit`, etc. No source-code reading is needed.

Read [the connection and API reference](references/api.md) for setup, exact operation fields/ranges, units, routing, error recovery and a complete composition example. Read it before your first composition or when constructing an unfamiliar operation. It describes Pulse Studio 0.37 and the eleven-tool base introduced in 0.31. The optional pattern colour field requires app/server 0.34+; omit it for older versions. section_clips requires app/server 0.36+ and whole clips inside the section; extract_notes and merge_patterns require app/server 0.35+; use fresh lane indices and inspect appended results before retrying a timeout.

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

## Discovery and delivery

For another AI client, provide this whole skill directory and configure that client's MCP stdio server using the reference. A project-local skill is not automatically installed in every AI application. The portable Pulse Studio package includes this skill and the Python MCP server. Keep its `references` directory beside `SKILL.md` when copying it.

Deliver a concise description of what actually changed, links to saved project/audio files, and any meaningful unverified point. Technical waveform/level checks verify rendering and signal health; they do not prove musical accuracy or that the result was heard by ear.
