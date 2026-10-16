# Pulse Studio changelog

Newest versions appear first. Entries describe the working additions and fixes in each version, not completion of the full DAW roadmap. Everything operates locally; hardware MIDI and cloud services remain excluded. See [FEATURE_STATUS.md](FEATURE_STATUS.md) for current limits.

History through 0.16 was reconstructed from retained executables, editor/native test reports, project migration files and packaged source. Early release dates and complete source snapshots were not retained, so dates are omitted and early entries summarize the supported evidence. The initial 0.1.0 version is recorded in the original CMake configuration.

## 0.37.0 — Background waveform sample-rate conversion

- Added Edit > Convert sample rate and Ctrl+P discovery in the existing waveform editor. Nine rates span 8 / 16 / 22.05 / 32 / 44.1 / 48 / 88.2 / 96 / 192 kHz. Conversion processes the whole file, retains pitch and duration to the nearest output frame (at least one frame), and maps selection endpoints to the new frame grid. The existing 120-second stereo bounds remain. This is sample-rate conversion; time stretching and pitch shifting remain pending.
- Added offline Blackman-windowed sinc conversion with a bounded interpolated polyphase table, a reduced cutoff for downsampling, independent stereo channels, endpoint extension, cancellation and atomic result commit. Same-rate core copies and clipboard splices remain sample exact. Downsampling removes content above the destination bandwidth and attenuates the transition near Nyquist; conversion does not normalize or hard-clip samples.
- Cross-rate Paste insert / Paste replace now automatically convert the clipboard to the destination rate on a background worker. Insert selects just the converted audio; Replace removes just the original destination selection. The clipboard retains its original samples/rate. Oversized pastes reject before conversion; failures never partially replace the editor or selection.
- Added cancellable background processing for conversion and cross-rate paste. The waveform Edit menu and Ctrl+P expose Cancel audio conversion; destructive audio edits and audio Undo/Redo wait until completion/cancellation. Source/selection changes or recording at completion reject stale results. Closing joins a cancelled worker. Source buffers are snapshotted before processing; file writing/cache loading still finalize on the editor thread, and other waveform edits remain synchronous.
- Successful edits save a new float32 WAV in data/Edits with one independent audio Undo/Redo entry, retaining valid zoom and selection times. Undo restores the previous file/sample rate/view; Redo restores the converted copy. Audio changes clear temporary slice markers. Existing source bytes, song references, project revision/history and clipboard format stay intact until an explicit To sampler / To Playlist assignment. Failed local writes retain the previous editor/history. The retained-file, 20-entry audio-history and shared 512 MB cache limits remain.
- Verified 3,976 core checks, 960 editor checks (38 new resampling/editor cases), 115 window checks, 260 native playback/dialog checks, 18 protocol tests and 31 actual MCP checks. Coverage includes pitch/duration at all nine presets, maximum 24:1 conversion, independent stereo/DC/endpoints, fractional/minimum frames, same-rate exact copies, deterministic conversion, cancellation during output, in-place output safety, cross-rate insert/replace boundaries, invalid/nonfinite data, capacity/recording/stale-source/disk failures, stored WAV metadata and native dialog Apply/Cancel/stale/Undo and cross-rate Paste. A 14 kHz source downsampled from 48 to 16 kHz remains below -80 dBFS in the measured interior; this is a fixture measurement, not a general stopband guarantee. Portable runtime, hashes, complete corresponding source and bundled-server save/export are verified for the release.
- Project schema 14, sound/library schema 2, all 56 factory sounds, eleven MCP tools and existing 0.29 recovery/single-instance paths remain. Conversion is a native editor feature; it does not add an MCP waveform operation. The broader roadmap remains unfinished. Hardware MIDI and cloud remain excluded; macOS stays deferred.

## 0.36.0 — Whole-section clip Copy/Move

- Added Copy / move clips in the existing Song marker navigator, section flag context menus and Ctrl+P. The dialog identifies the selected section, pattern/audio/automation clip counts, duration and a 1-based destination bar with fractional support. Copy defaults to the section end; Move relocates the original clip indices. No markers, loop boundaries, patterns, source files or unrelated clips are moved or replaced automatically. Song/PAT mode remains unchanged, and the Playlist reveals the destination and selects its first result.
- Added a shared arrangement transform preserving relative positions, lanes, pattern links, audio source offsets/gain/fades and automation target/point/hold data. Only complete clips inside the half-open source range are included. Any crossing clip, including whole-song automation spanning the range, rejects the complete edit instead of silently cropping or skipping it. Edge-touching clips stay outside. Partial-clip transforms, ripple editing and automatic marker relocation remain pending.
- Added default overlap rejection for existing clips on the same lane and automation targeting the same parameter/channel even across display lanes, with Master semantics handled separately. Explicit Allow layering retains every destination clip and existing automation ordering. Old positions of moved clips are ignored; intentional overlaps already within the source group remain. The full destination span must fit 0–4096 beats and differ from the source start.
- Added one-step Undo/Redo, revision/section-selection checks, recording/export/preview guards, atomic clip/automation-point capacity checks and full Song compilation before checkpointing even in PAT mode. Move can operate at full clip/point capacities because it creates no new slots. Song section edits refresh sequenced voice identities at the current playhead. Cancel and rejected edits leave project data/history unchanged; source audio bytes remain unchanged.
- Added section_clips to the existing eleven-tool MCP pulse_edit API, requiring app/server 0.36+. It accepts a section id, destination in beats and optional boolean move/allowOverlap. Batch ordering, revision checks, rollback, History backup, Undo and export use the real native app; the detailed model-independent skill reference documents units, boundaries, retained data, layering and timeout recovery. Project schema 14, sound/library schema 2, all 56 sounds and existing recovery/layout paths remain unchanged.
- Verified 3,943 core checks, 922 editor checks (27 focused section cases), 115 window checks, 251 native playback/dialog checks, 18 protocol tests and 31 actual MCP checks. Coverage includes mixed-type audio rendering, source offsets/fades/automation links, touching/crossing boundaries, independent and linked automation conflicts, fractional/edge destinations, full capacities, in-place safety, stale dialogs, native Apply/Move/Cancel, Undo/Redo, PAT-hidden Song overflow, live section export and sample-exact held-voice refresh after Copy before the playhead. Portable runtime, hashes, complete corresponding source and bundled-server section arrangement/save/export are verified for this release.
- The broader DAW roadmap remains unfinished. Hardware MIDI and cloud remain excluded; macOS stays deferred.

## 0.35.0 — Selected-note patterns and explicit merge conflicts

- Added PATTERNS / Piano roll window-menu / Ctrl+P “Pattern from selected notes...” with explicit Copy or Move and original beat positions or first-onset alignment. An empty selection never means all notes; a single focused note is supported. The dialog identifies the source channel, selection and shared-instance count. New patterns retain the source length/colour, contain only selected notes on that channel and have independent identities. Timing relationships, long gates, velocities, probability and ratchets remain editable. Move removes only selected indices, preserving unselected identical notes. No Playlist clips are created or relinked automatically.
- Added one-step Undo/Redo, selected-note focus in the new Piano roll, stable source identity/revision/channel/selection validation, recording/preview guards and atomic bank/event-capacity checks. Cancel, stale or invalid selection and capacity failures leave musical data and history intact. Move and PAT restructuring refresh obsolete held voices while retaining the playhead.
- Added “Merge patterns with conflict options...” alongside the existing direct layer/sequential actions. Layered merges can keep every note, remove exact full-field duplicates, prefer the current pattern or prefer incoming notes at identical onset/pitch on the same channel. Other overlaps remain; no fuzzy matching, averaging, quantization or gate trimming occurs. Preference policies retain duplicates within the winning source. Originals and their clips remain intact; the new result inherits the first colour and longer length. Capacity is checked after filtering, allowing matching full lanes to merge when the result fits 512 notes.
- Added extract_notes and merge_patterns operations to the existing eleven-tool MCP API. Extraction uses explicit unique lane indices with optional move/alignStart; merging supports layered conflict policies and sequential keep-all concatenation. Batches can reference earlier appends, reject invalid fields/indices and later failures atomically, and commit one Undo. Both operations require app/server 0.35+; native commits compile Song and newly active PAT capacity before checkpoint/backup. Model-independent skill/API docs include exact fields, policy behavior, examples and timeout recovery.
- Retained project schema 14, sound/library schema 2, all 56 factory sounds, existing workspace panels and 0.29 recovery/single-instance paths. No project migration is needed from 0.34. Multi-channel extraction, fuzzy onset matching and automatic gate-overlap splitting remain outside this implementation; the broader roadmap is unfinished.
- Verified 3,890 core checks, 895 editor checks (29 focused selection/merge cases), 115 window checks, 242 native playback/dialog checks, 17 Python protocol tests and 24 actual MCP checks. Tests cover unsorted selections, source-index identity, original and aligned audio, duplicate articulation, colour/identity inheritance, per-policy audio, post-filter capacity, atomic rollback and native dialog Apply/Move/Cancel/stale behavior. Portable runtime, manifest, complete corresponding source and bundled-server extraction/merge/save/export are verified for the release.
- One initial native run stopped receiving device callbacks after 99,840 frames while editor/window suites also ran; the isolated repeat passed all 242 checks. The cause of that callback loss is unconfirmed. Device tests remain smoke checks, not sustained latency/stress certification. Hardware MIDI and cloud remain excluded; macOS stays deferred.

## 0.34.0 — Pattern colours and configurable metronome

- Added twelve named pattern colours plus Automatic in PATTERNS and Ctrl+P. Browser entries, pattern-menu text, toolbar indicator, rack label, Playlist picker/clip titles and Piano roll notes share the palette while retaining selection highlights and the existing workspace. Automatic preserves the previous appearance. Changes affect every shared instance, leave musical data/audio untouched and support one-step Undo/Redo with recording, preview and stale-menu guards.
- Clone, Make unique and channel splits inherit their source colour. Layered/sequential merges inherit the current (first) pattern's colour. Reordering, removal and Undo retain colour with the pattern. Optional integer `colour` (0–12) is accepted by MCP add_pattern/set_pattern with strict atomic validation; omitted values preserve existing colours or default a new pattern to Automatic. The eleven-tool API remains; this optional field requires app/server 0.34+.
- Added local metronome configuration through right-click CLICK, OPTIONS, Tempo and recording, and Ctrl+P: Classic tone, Wood block, Hi-hat or Rim click; 0–200% level; equal beats or bar accents. The playback click and count-in share fixed-storage deterministic synthesis. Classic/100%/accented retains the previous audio exactly; zero level silences clicks without altering timing. Settings latch at click onset and a current 24 ms tail may finish.
- Added validated atomic data/metronome.json preferences, independent of song data, dirty state, revisions and Undo. Apply/Cancel, invalid-value, recording and stale-confirmation guards preserve current settings. Clicks follow the current Master/output path and remain excluded from dry recorded audio and offline exports. Separate click routing, recording-only mode, arbitrary count-in lengths, custom samples and tempo/meter-map support remain pending.
- New project/template/recovery writes use schema 14 for pattern colours. Schemas 1–13 remain readable with missing colours defaulting to Automatic. Apps 0.33 and earlier need retained pre-update projects/History backups. Sound/library schema 2, all 56 factory sounds and existing 0.29 recovery/layout paths remain. Corrected stale versions in the status bar and MCP session metadata.
- Verified 3,824 core checks, 866 editor checks (26 new focused cases), 115 window checks, the complete native playback/dialog suite including nine new metronome/colour cases, 15 Python protocol tests and 18 actual MCP checks. Tests cover old schemas, invalid fields, colour inheritance and sample-exact audio neutrality, four distinct deterministic click sounds at 8–192 kHz, level/mute/accent behavior, count-in/capture boundaries, callback independence and export exclusion. The portable manifest, complete corresponding source, static runtime and bundled-server native save/export are checked for this release.
- The broader roadmap remains unfinished; selected-event pattern extraction, conflict-aware merging and the other boundaries in FEATURE_STATUS.md remain pending. Hardware MIDI and cloud remain excluded; macOS stays deferred.

## 0.33.0 — Held-note chase and pattern arrangement tools

- Seeking, resuming and section-loop starts now restore melodic notes whose gates are still held. Samplers start at the corresponding source position, respecting pitch, selected ranges, loops and exhausted one-shots. Probability and ratchets retain their sequencer decisions; drums, muted notes and ended gates are excluded. Matching active voices are reused across pause/resume and loop wraps instead of stacking duplicates. Work uses fixed storage and the existing 128-voice limit.
- Chased synth notes restart their envelopes; historical oscillator/filter/effect state and earlier release tails are not reconstructed. Offline range export retains its existing warm-up behavior. Ordinary parameter edits do not retrigger held notes. MCP status adds the cumulative `chasedNotes` diagnostic; all eleven tool inputs remain compatible.
- Added PATTERNS > Append pattern after current (new pattern), also in command search. The second phrase starts after the first pattern's full duration. Original patterns and Playlist instances stay intact, including intentional overlapping gates, velocities, probability and ratchets. Combined duration, per-lane notes, bank size and compiled event capacity are checked atomically; each merge supports one-step Undo/Redo.
- Added explicit confirmed deletion of a used pattern together with all its pattern clips. The dialog identifies the pattern, note count and clip count; audio/automation clips and source files are retained. Remaining references and selections follow index compaction. Stable clipboard identities, stale-confirmation checks, recording/preview guards and Undo/Redo protect subsequent edits. The unused-pattern action remains available separately.
- Verified 3,662 core checks, 840 editor checks, 115 native window checks, 14 Python protocol tests and 16 live MCP checks, including actual audio after seeking into a held note. Portable runtime, manifest, corresponding source and bundled-server save/export are verified with the release. Schema 13, all 56 factory sounds and the existing recovery/layout protocols remain unchanged.
- The full DAW roadmap remains unfinished. Pattern colours, advanced recording takes, tempo maps, third-party plugin hosting and the other boundaries in FEATURE_STATUS.md remain pending. Hardware MIDI, cloud and macOS work remain excluded from this release.

## 0.32.0 — Typing performance controls, retrospective capture and MCP skill

- Added KEYS context/OPTIONS menus and command-search actions for input velocity (1–127), octave, -12..12-semitone transposition, sustain latch, capture on/off and 15/30/60-second history. Velocity/transposition shortcuts affect new notes, and keyboard preferences persist locally without dirtying songs.
- Added sustain release/rearticulation, actual source-channel tracking for held notes, focus/Stop/mode-exit flushing and an emergency release when a note-off cannot enter the bounded audio queue. Keyboard takes store configured velocity and sustain-extended durations with one take undo.
- Added bounded memory-only retrospective typing capture: up to 512 note events and 64 beats, monotonic timing, velocity/duration retention, current-tempo conversion, first-onset alignment and four-beat pattern sizing. Recall adds an editable pattern in PAT mode with one Undo/Redo, preserving existing notes and Playlist clips. Accepted recall consumes the phrase; repeated commands cannot duplicate it. Idle expiry, explicit clear, disable, channel/context, recording/export/preview and pattern-capacity guards apply.
- Added a model-independent project skill with a full MCP connection/API reference, all fields/units/ranges, sound design, arrangement replacement, routing/automation, save/export verification, timeout/revision recovery and a complete original composition example tested against the live app. Both files ship in the portable ZIP and corresponding source; Git includes the project skill directory.
- Corrected the MCP schema's clip-length maximum to 600 beats, matching existing native project validation, for both pattern and automation clips.
- Verified 3,595 core checks, 818 editor checks (including 24 typing/capture cases), 115 native window checks, 14 Python protocol tests, 15 live MCP checks including the skill example, and packaged-server/native save/export checks. The 18-file portable manifest and complete corresponding source were byte-verified; PE imports require no C/C++ runtime DLL. Unrelated local composition scripts are excluded from release sources.
- Kept project schema 13, offline operation, the familiar existing panels, all sound libraries and the 0.29 recovery/single-instance protocol. Hardware MIDI and cloud remain excluded. Sample-offset input compensation, transport/tempo-aware retrospective alignment, controller/expression capture and the broader DAW roadmap remain pending.

## 0.31.0 — Local MCP control and live composition

- Added an opt-in local MCP connection with eleven stdio tools for live state/project inspection, factory sound discovery, new songs, atomic edits, transport, save, WAV export, editor views, screenshots and undo.
- Added explicit OPTIONS > Local MCP control and --enable-control startup. The bridge uses local per-session queues/tokens, bounded requests, expiry and replay rejection, with no network listener, cloud service or arbitrary code execution. Disabling control invalidates the session.
- Added revision-checked UI-thread edits for custom Analog/FM/Wavetable patches, track levels and effects, buses/routing, patterns, complete note lanes, Playlist pattern clips, sections and normalized automation. Invalid batches leave the entire project unchanged; successful batches are one undo step and compile through the real playback engine.
- Added History backups before first control edits/new songs, recording/modal/preview/export guards and workspace-scoped Projects/Exports destinations with explicit overwrite checks. Existing project schema 13 and all sounds/variable pattern lengths remain supported.
- Added a Python-standard-library MCP server/client and an original dark-score recipe that makes all musical changes through MCP, including native save/export/playback. Python 3.10+ is needed for optional MCP, while the workstation itself remains standalone.
- Verified 3,567 core checks, 794 editor checks, 115 native window/zoom checks, 12 Python protocol tests and 12 live MCP integration checks, plus the existing native playback/export suite.
- Local scripting remains limited to these declared operations. Third-party plugins, arbitrary project-file import through MCP, hardware MIDI, cloud support and the rest of the DAW roadmap remain pending.

## 0.30.0 — Personal workspaces and interface scaling

- Added VIEW > Workspaces with Composing, Arranging and Mixing factory layouts, preserving musical selections and project contents.
- Added up to 32 local named layouts with panel positions, visibility, stacking, maximized restore bounds, focused editor and Browser size. Save, explicit update, rename, confirmed delete and .pulseworkspace import/export operate independently of songs and synth libraries.
- Added strict workspace validation, bounded files, atomic replacement, stable identities and stale source/library checks. Invalid or changed files leave the current panels and library intact; factory layouts remain available when the personal library cannot be read.
- Added VIEW > Interface scaling with 75, 90, 100, 110, 125, 150, 175 and 200 percent preferences. Requested zoom is capped to fit the complete workspace inside the available window; native hit testing and menu placement follow the transform. The preference stays local and separate from named layouts and musical zoom.
- Added Ctrl+P entries for workspace management, every factory/personal layout and individual scaling choices. Window fitting and Ctrl+Shift+H recovery remain available.
- Retained schema-1 five-to-fifteen-window layout compatibility, project schema 13, all 56 factory sounds, variable pattern lengths and the existing 0.29 recovery/layout protocols. No musical data migration is required.
- Added core persistence/validation tests, editor recall/selection/preview/recording guards, native save/rename/delete/menu cases and 115 native window checks across automatic/manual zoom and small work areas. The release passes 3,534 core and 776 editor checks, native playback/export/workspace checks and portable package verification.
- Themes, docking, independent editor windows across monitors and the broader unfinished DAW roadmap remain pending.

## 0.29.1 — Fit the workstation to smaller screens

- Fit startup to the current monitor's usable desktop, including the native title bar, borders and taskbar space. Oversized startup and minimum bounds no longer leave resizing edges outside a smaller display.
- Allow the native window to shrink to 640×400, or the available work area when smaller. Automatically scale the complete workspace down when needed, preserving the logical toolbar and panel canvas, mouse coordinates and menu anchors.
- Refit after monitor work-area, DPI or native-frame changes. Ctrl+Shift+H now brings the main window inside the current desktop and resets the internal panel arrangement.
- Retain project schema 13, all sounds and variable pattern lengths, and the existing 0.29 recovery/layout files. No project migration is required for this patch.
- Verified 3,472 core checks, 749 editor checks and 67 window checks, including native fitting, resize constraints, scaled hit testing, menu placement, offscreen recovery and rendered workspaces down to a simulated 640×400 work area.
- Manual interface zoom, custom themes, named workspace presets and independent editor windows across monitors remain pending.

## 0.29.0 — Pulse Wavetable and sixteen more sounds

- Added Pulse Wavetable to ADD, the Browser, command search and the existing generator editor, with a base-waveform display and eight original four-frame families: Classic, Hollow, Vocal, Metal, Organ, Glass, Digital and Soft.
- Added continuous frame position, a signed decaying position envelope, table-envelope decay and shared-rate LFO depth, alongside two-oscillator mix/detune/tuning, sub/noise, ADSR, resonant filter and filter/vibrato modulation.
- Added immutable 1,024-sample tables with eight harmonic mip levels, pitch-dependent mip blending and phase/frame interpolation. Tables prepare before playback; voices use fixed storage, smoothed position and short family/mode transitions.
- Added sixteen original factory bass/lead/pad/keys/pluck/FX patches, bringing the factory library to 56. The original 24 Analog/16 FM patches and favorite identities remain unchanged.
- Added bank-menu dispatch, native drag/wheel/fine/reset and typed controls, grouped undo, Preview, personal sounds/favorites and command indexing. Stale source/dialog, invalid value, bus, recording and note-preview guards protect edits.
- Added project schema 13 for complete table settings in projects/templates/recovery. Schemas 1–12 remain readable with neutral table defaults; older Analog/FM rendering is retained. A downgraded wavetable file is rejected rather than interpreted as another synth.
- Added sound-file/library schema 2 with schema-1 read compatibility. Reading an old library preserves its bytes; explicit writes retain existing sounds, identities and favorites in the new format. Chain-file schema 1 is unchanged.
- Included wavetable voices in polyphonic playback, bus/stereo/effect processing and offline WAV/FLAC rendering, retaining variable step lengths and all fifteen existing windows.
- Verified 3,472 core checks and 749 editor checks plus native bank/typed-value/library/playback/export cases. Tests cover frame/DC bounds, morph/mip continuity, high-note suppression, tuning, maximum polyphony, extreme rates/pitches, block independence, schema validation/migration, old library compatibility and sample-exact export. Retained 0.28 chain/stereo PCM fixtures match byte for byte.
- User table import/authoring, unison, sync, modulation routing, oversampling, synth automation and full wavetable features remain pending; no third-party plugin host or cloud/hardware-MIDI dependency was added.

## 0.28.0 — Stereo tools for inserts and buses

- Added a Stereo tools page inside the existing Mixer effects window, opened beside Analyzer or Chain, with width (0–200%), mono, L/R swap, independent input polarity and output trim (-24 to +6 dB).
- Added a fixed post-chain, pre-pan/fader/send matrix with 20 ms coefficient smoothing, zero added latency, exact neutral/settled bypass and no audio-thread allocation. Mono preserves the stored width and correctly cancels opposite-phase inputs.
- Added native drag/wheel/fine/reset gestures, percent/dB typed values, grouped undo, switch/reset undo, command search, recording locks and cancel/invalid/changed-source guards. The compact controls share the existing fifteen-window workspace.
- Included stereo tools in monitoring, source/bus routing, channel analyzer capture and offline WAV/FLAC rendering; dry input recording remains unchanged. Six-effect chain recalls/copy/paste/reset preserve the separate utility.
- Added project schema 12 with strict complete stereo settings and atomic validation, including project templates and recovery. Schemas 1–11 load with neutral bypassed tools and retain sample-identical legacy rendering; sound/library/chain file schemas remain unchanged.
- Verified 2,573 core checks and 717 editor checks, plus native stereo audio/mono, typed width/trim, undo, invalid/stale/cancel/recording guards, project persistence, chain preservation and float WAV export. Core checks include matrix combinations, smoothing across 8–192 kHz, block independence, monitored cancellation, routed buses, templates and rejected corrupt state.
- Independent mid/side gain/solo/EQ, stereo delay, utility automation, multiple instances and multichannel/spatial formats remain pending.

## 0.27.0 — Live stereo spectrum and meters

- Added Audio analyzer to the existing Mixer effects window, with Master or selected insert/bus sources, a logarithmic stereo spectrum, peak/RMS levels, held peaks, full-scale sample counts and L/R correlation.
- Added a calibrated 2,048-point Hann FFT with stereo power averaging, sine-amplitude dBFS and correct DC/Nyquist scaling; opposite-phase signals remain visible. Device sample-rate changes reset calibration.
- Added 300 ms exponentially weighted RMS, decaying sample-peak displays, persistent peak holds/counters, undefined-correlation handling for silence and explicit sample-peak/true-peak/LUFS distinctions.
- Added Freeze/Resume, Reset, return to the selected effect, command search and local view/source preferences. Resume/source changes/reopening start fresh; frozen state is never persisted. Read-only controls work during recording without changing project/undo/step counts.
- Added a fixed 16,384-frame SPSC capture queue; overflow drops analysis only, with a visible counter. Source generations reject stale audio and all FFT work runs outside the audio callback.
- Master includes preview/metronome/monitoring before hardware mono folding; selected channels are measured after effects/fader/pan and before downstream buses/Master. Closing the window or returning to Effects disables capture.
- Retained project schema 11, fifteen existing windows, variable rack lengths, all sounds/effect chains and static-runtime offline packaging. Increased the Mixer minimum height to keep the new button and channel settings accessible; corrected the export-tail hint.
- Verified 2,335 core checks and 686 editor checks, plus native playback/calibration, opposite-phase spectrum/correlation, Freeze/Resume/Reset, source isolation/following, recording read-only behavior, preferences and capture shutdown.
- Integrated loudness/LUFS, true peak, phase scope, export measurement reports, FFT options/zoom and offline analysis remain pending.

## 0.26.0 — Effect-chain ordering and presets

- Added per-insert ordering of the six built-in processors, stable effect identities, actual ordered audio dispatch and Mixer slots that follow their processing order.
- Added a Chain menu and slot context moves (earlier/later/first/last), retaining effect settings/bypass states, grouped undo and uninterrupted transport.
- Added eight original factory chains, effect-chain copy/paste between channels and buses, clean reset, command search and local .pulseeffects import/export.
- Chain recall replaces effect parameters, bypass states and ordering while preserving instruments, notes, variable step lengths, fader/pan/send, output route and compressor sidechain source. Portable chain files exclude external channel references.
- Added atomic validated chain-file schema 1 (128 KiB limit) and project schema 11 for ordering; schemas 1–10 load in the original processor order and retain their audio.
- Added recording/preview guards, changed-source menu/import rejection and no-op history protection. User projects/samples, generator library formats and offline static-runtime packaging remain supported.
- Verified 2,290 core checks and 667 editor checks, plus native chain recall/undo, live reorder, actual slot/Chain menus, copy/paste, changed-source/recording guards, local files, ordered export, persistence and Stop.
- Duplicate effect instances, arbitrary plugin/parallel graphs, personal chain indexing, effect automation, tempo sync and graph-change crossfades remain pending.

## 0.25.0 — Stereo flanger and phaser

- Added stereo flanger and six-stage phaser as Mixer slots 5 and 6, usable on instruments, samples, monitored inserts and routed buses.
- Added six controls per effect: wet mix, rate, depth, centre delay/frequency, signed feedback and stereo width. The flanger has fractional delay; the phaser has six all-pass sections per channel and softly bounded resonance.
- Added native effect tabs, drag/wheel/fine/reset gestures, physical-unit typed values, command search and grouped undo; recording and changed-source dialogs are guarded.
- Added smoothed controls/bypass, exactly dry zero-mix/bypassed signals, independent stereo state and Stop/reset feedback clearing, with storage allocated before rendering.
- Added project schema 10 for both effects in projects, recovery and local templates. Schemas 1–9 remain readable with the new effects bypassed, preserving chorus and existing generator audio.
- Included modulation in callback audio, routed processing and offline export; kept variable Channel rack lengths, original sound library and static-runtime offline packaging.
- Verified 1,448 core checks and 640 editor checks, plus native slot/playback, typed values, signed feedback, cancel/invalid/stale/recording guards, menus, project persistence and Stop for both effects. Native export also passes with collected sample references resolved.
- Tempo sync, modulation automation, selectable phaser stages, effect presets and reorderable chains remain pending.

## 0.24.0 — Stereo chorus effect

- Added an original stereo variable-delay chorus as the fourth existing Mixer effect slot, available on source channels and routed buses.
- Added wet mix, logarithmic rate, depth, centre delay, signed feedback and stereo width, with smoothed changes/bypass and zero-latency dry signal.
- Added six native controls, physical-unit typed values, reset/fine gestures, command search and one-step undo for gestures, bypass and typed edits; recording and changed-source dialogs are guarded.
- Added project schema 9 with complete chorus state in projects, recovery and local templates. Schemas 1–8 remain readable with chorus bypassed; existing generator sound files/library schemas remain unchanged.
- Integrated chorus into callback audio, monitored inserts, bus routing and WAV/FLAC export, with preallocated delay storage and bounded per-sample processing. Stop invalidates feedback history.
- Retained the offline static-runtime portable package, variable Channel rack step lengths and fifteen-window workspace. Flanger/phaser, tempo sync, effect automation and arbitrary effect ordering remain pending.
- Verified 1,248 core checks and 606 editor checks, plus native chorus slot/playback, typed values, signed feedback, cancel/invalid/stale/recording guards, persistence and Stop/reset.

## 0.23.0 — Personal sound library and favorites

- Added up to 128 named personal Analog/FM sounds with categories and persistent identities, recalled through the existing Sounds menu and Ctrl+P.
- Added factory/personal favorites and searchable favorite markers, plus Save to library in the generator window.
- Added explicit update from the current sound, rename/category, confirmed delete, stored-sound export and individual file import into the library.
- Kept project sounds independent: library changes preserve embedded project settings/audio, notes, variable step lengths, mixer state and project history. Recall remains one undo step.
- Added bounded, validated, atomic local storage with content-token checks, stale dialog/search rejection, recording locks and corrupt-library protection. Factory recall remains available when personal storage is invalid.
- Kept project schema 8, sound-file schema 1 and static-runtime offline packaging. Drum kits, richer tags, pack indexing and bulk/multiple-library workflows remain pending.
- Verified 1,156 core and 588 editor checks, including personal/factory favorites, capacity/Unicode validation, corruption/stale-write protection, search recall and unchanged embedded-project audio after source deletion.

## 0.22.0 — Editable Analog/FM synthesizers and forty factory sounds

- Added Pulse Analog and Pulse FM to ADD, the Browser and channel workflows, with a resizable generator window in the familiar floating workspace.
- Added two-oscillator Analog synthesis with four waves, PolyBLEP saw/pulse, detune/tuning/pulse width/sub/noise, and fixed two-operator FM with ratio, index, decay and operator mix.
- Added amplitude ADSR, resonant low-pass/filter envelope, vibrato/filter LFO, smoothed controls, typed values, timed C4 preview and grouped parameter undo.
- Added forty original categorized factory patches (24 Analog / 16 FM), indexed in Ctrl+P, plus validated atomic local sound-file save/load.
- Added schema 8 for complete synth state in projects, templates and recovery. Schemas 1-7 still load with unchanged legacy generator audio; older executables require their retained pre-update backups.
- Preserved the user's configurable Channel rack step-count implementation and template-driven grid. Added regression coverage, positive cell widths for dense grids and stale/recording guards for its length dialog.
- Third-party VST/CLAP hosting and the larger synth feature set remain in the roadmap; no hardware MIDI or cloud dependency was added.
- Verified 1,108 core and 568 editor checks, including every factory sound, tuning/stability/polyphony, callback-size invariance, local patch and project persistence, legacy audio migration and variable rack lengths.

## 0.21.1 — Configurable Channel rack step count

- The Channel rack step grid now follows the active pattern length instead of a fixed 32 cells: it draws `pattern.length * 4` sixteenth steps, with cell width, beat grouping, the playhead position and the bottom step label all derived from the pattern.
- Added **Set pattern length (steps)** in the pattern menu (and a clickable step label in the rack) accepting 16-64 steps (4-16 beats), rounded to whole beats, committed as one undo step with notes beyond the new length removed.
- Fixed the previous silent-step bug: cells past the pattern length are no longer generated, so every visible step is audible.
- Kept schema 7, local operation and static-runtime packaging; engine, note tools, Playlist, Swing and existing projects are unchanged.

## 0.21.0 — Transient detection and editable slice markers

- Added local stereo amplitude-rise detection in the existing Waveform editor, with noise floor, rise ratio and minimum gap controls. Candidate markers are reviewed before audio is sliced.
- Added up to 256 temporary frame-aligned slice markers, orange overlays, Alt+click add/remove and Clear. Current selection endpoints and interior marks create contiguous irregular Playlist clips at original speed.
- Reused batch project undo/redo, source-file preservation, effects/routing and capacity checks; grid slicing retains the full 512-clip limit.
- Added validated atomic detection, recording locks, Cancel and stale-source/selection rejection. Source edits and audio-history restores clear markers whose positions may no longer apply.
- Kept schema 7, local operation and static-runtime packaging. Slice markers are editor-only; persistent maps, spectral onset analysis, time stretching and sampler pad/MIDI mapping remain pending.
- Verified 668 core checks and 545 editor checks, including stereo attacks, debounce/floor controls, atomic invalid/dense-result handling, manual corrections, selection clipping and continuous irregular-slice audio.

## 0.20.0 — Waveform splicing and Playlist beat slicing

- Added a separate local stereo audio clipboard, cut/copy, insert or replace paste, duplicate, trim to selection, delete and silence in the existing Waveform editor. Keyboard shortcuts and right-click Edit menu follow its active context.
- Added independent twenty-edit audio undo/redo restoring file, selection and zoom; the main Undo/Redo controls follow the active Waveform window. Opening a source resets audio history while retaining the clipboard.
- All waveform edits now write separate float32 WAV copies, preserving source bytes, existing assignments and exact decoded sample values. Failed/empty/over-limit/non-finite edits leave the editor intact; cross-rate paste requires prior conversion.
- Added original-speed Playlist chopping at five project-tempo grids, with frame-aligned source offsets, no gaps, a partial final slice and one-step project undo/redo. Capacity, recording, bus and unresolved-preview checks precede insertion.
- Kept the familiar fourteen-window interface, offline portable static-runtime package and schema 7. Transient detection, time stretching, crossfades, sampler-pad mapping and background waveform processing remain pending.
- Verified 642 core checks and 529 editor checks, including exact stereo splices, failure atomicity, source preservation, bounded independent history and slice rendering against an uninterrupted reference.

## 0.19.0 — Pattern-bank cleanup and ordering

- Added Move earlier/later in the existing pattern menu and command search, preserving the active pattern, note selection, Piano roll view and every Playlist reference.
- Added confirmed deletion of unused patterns to reclaim bank slots. Used patterns and the final remaining pattern are protected; other references remap without moving clips or editing audio/automation.
- Added complete one-step undo/redo, recording/preview locks and stale-confirmation rejection.
- Copied pattern clips now track editor-session identities through reorder, delete, note edits and undo/redo, including duplicate names. An absent copied source blocks Paste; undo restores availability. Clones receive distinct identities.
- Kept these identities outside project files: schema remains 7 and loading starts a new clipboard session. Persistent object identities and deletion of used patterns remain pending.
- Verified 607 core checks and 496 editor checks, plus native clipboard/reorder undo and confirmed delete, Cancel, undo and stale-dialog rejection.

## 0.18.0 — Pattern management

- Added Make unique for a selected Playlist pattern clip: clone the complete pattern and relink only that instance, preserving other shared clips.
- Added Split by channel: create patterns for populated channels and redirect their Playlist instances while retaining the original, silent-lane references, audio and automation.
- Added one-pass layered merging into a new pattern at original note positions, with the longer source length. Source patterns and arrangement clips stay intact.
- Added usage counts and split/merge/unique actions to the existing pattern menus and command search. Alt+right-click opens a clip menu; ordinary right-click still erases.
- Kept plain pattern-name search focused on selecting the pattern; explicit merge searches choose a merge target.
- Added one-step undo/redo, source-preview and recording locks, unique bounded names and pre-commit pattern/note/engine capacity checks. Failed operations retain the project and history.
- Kept project schema 7 and the portable static-runtime build. Updated the version history and feature boundaries.
- Verified 574 core checks and 465 editor checks, plus native unique/split undo, merged Pattern audio and both menus.

## 0.17.0 — Local note-tool presets

- Added a Presets menu to the existing Quantize, Humanize, Euclidean rhythm and Arpeggiator window, plus discovery through Ctrl+P.
- Added twelve original factory presets and up to 128 personal presets, with save, explicit update, rename and delete.
- Added single-preset JSON import/export for moving settings between local installations.
- Recall recomputes the captured originals, preserves the current selection and A/B state, and leaves notes uncommitted until Apply. Apply remains one undo step.
- Euclidean presets retain the destination channel's pitch and selected beat range. Other tools' settings remain independent.
- Added bounded, validated, atomic local storage. Corrupt libraries, duplicate identities/names, unsupported schemas and invalid settings are rejected without overwriting valid data.
- Reset now affects only the active tool; preset JSON retains exact triplet timing on reload.
- Added this version history to the workspace, portable distribution and corresponding-source archive. Project schema remains 7.
- Verified 537 core checks and 436 editor checks, including exact triplet recall, audible preview, recording locks, atomic file failures and preset-based Apply/undo.

## 0.16.0 — Piano roll arpeggiator

- Added Alt+A and the Arpeggiator tab, menu actions, context access and command search.
- Added Up, Down, Up/Down, Down/Up, Source order and seeded Random ordering; straight/triplet rates, octave expansion, transposition, gate and velocity scaling.
- Added held-chord tracking at step starts, rest preservation, chord-change restart and source note-off/pattern-end gate clipping. Out-of-range expanded pitches are skipped.
- Preserved unselected notes and the chosen source's probability/ratchets; restored the captured selection when returning from Euclidean mode.
- Added original-note overlays, audible A/B, Apply/Cancel, one-step undo and atomic invalid/capacity fallback using the existing preview workflow.
- Verified 500 core checks, 402 editor checks and native arpeggiator audio/Apply/undo, plus portable startup/audio. Schema 7 unchanged.

## 0.15.0 — Euclidean rhythm generator

- Added Alt+E, a Euclidean tab and menu/search access. Percussion Generate opens these controls for review.
- Added steps, pulses, rotation, repeats, pitch, beat range, gate, velocity and repeatable velocity variation, including typed numeric controls.
- Added pitch-only replacement inside a half-open range and idempotent Fill empty steps; other pitches and out-of-range notes remain intact.
- Added audible preview, original-note overlays, Apply/Cancel and one-step undo. Invalid density, lane capacity and Song event capacity restore committed playback until corrected.
- Protected generated-note mouse, wheel and double-click handling from indexing into the original source lane. Schema 7 unchanged.

## 0.14.0 — Popup fixes, portable runtime and command search

- Fixed header menus appearing near the bottom by anchoring each popup to its own control. Context menus use the cursor position while retaining the app's DPI scale.
- Reserved menu ID ranges to prevent newer editor actions from colliding with View, Close, instrument and pattern commands.
- Switched release builds to the static MSVC runtime, removing the dependency on VCRUNTIME140_1.dll and other VC/UCRT runtime DLLs.
- Added a portable ZIP builder with normal/delay-load PE dependency checks, executable hashes, notices and complete corresponding source including pinned JUCE.
- Added Ctrl+P action search across commands, channels, patterns, clips, markers, factory instruments, local templates and help, with keyboard navigation and stale/disabled-context guards.
- Added last successfully saved/opened project startup when no recovery is pending. Version boundaries prevent stale older recovery from replacing a saved newer session.
- Retained and strengthened the note preview, save/export and menu regression checks introduced in 0.13.

## 0.13.0 — Quantize and humanize preview tools

- Added a floating Piano roll note-tools window with Alt+Q Quantize and Alt+R Humanize.
- Added quantize grid, position/length strength, swing and optional position window; added timing, duration and velocity humanization with fixed seeds and chord-start grouping.
- Added captured selected/all-lane targets, temporary audible preview, original comparison, Apply/Cancel and one-step undo. Repeated adjustments start from the captured originals.
- Added Ctrl+Q quick quantization, preview/source invalidation and safeguards around saving/exporting pending note changes.
- Kept project schema 7. Popup placement and menu-ID collisions present in this build were addressed in 0.14.

## 0.12.0 — Song markers and sections

- Added up to 128 named, colour-coded beat markers and section ranges with stable identities, a Playlist marker rail and floating navigator.
- Added marker creation/edit/delete, snapped dragging, section-end resizing and grouped gesture undo without moving arrangement clips.
- Added Alt+T, Ctrl+Alt+T, previous/next navigation and ruler/context access; rapid navigation uses the pending seek position.
- Added linked Song section loops and section-to-export range transfer, including live engine loop handling and punch-out precedence.
- Introduced schema 7 for markers and linked section loops; earlier schemas remain readable.

## 0.11.0 — Export settings and background rendering

- Added a floating Export audio settings window before destination selection, shared by Master and isolated-track exports.
- Added WAV/FLAC, 44.1/48/96 kHz, mono/stereo, PCM16/24 and float32 WAV, full/custom/punch ranges and configurable tails.
- Added timeline warm-up for range exports, two-pass sample-peak normalization and deterministic TPDF integer dither; float output excludes dither.
- Added background snapshot rendering while live playback/editing continues, progress/cancellation and atomic per-file finalization before replacing destinations.
- Added isolated source/bus batches with shared encoding settings and local export preferences outside project undo.

## 0.10.0 — Input selection, monitoring and meters

- Added first/second enabled mono input selection or stereo input, raw input peak meters, dBFS readouts and clipping hold.
- Added independent monitor gain and monitoring through the selected source's effects, fader, routing buses and Master while stopped or playing.
- Kept recorded WAVs dry and independent of monitoring gain/effects. Input selection stays locked during a take; monitor controls remain usable.
- Added local input/gain preferences; monitoring always starts off and switches off on input selection or device changes.

## 0.9.0 — Tap tempo, count-in and punch recording

- Added a floating Tempo and recording window, tap-tempo median smoothing/reset and grouped tap undo.
- Added accented metronome behavior, one/two-bar recording count-in and zero/one/two-bar punch pre-roll.
- Added persisted punch ranges, sample-accurate audio capture spans, automatic punch-out, bounded typing-keyboard capture and held-note termination.
- Excluded count-in/pre-roll from take data, restored prior transport mode/looping after takes, and grouped keyboard takes into one undo step.
- Introduced schema 6 for recording settings.

## 0.8.0 — Project templates

- Added a searchable local project-template browser with six built-in starters and personal/full-project versus channel-and-mixer filters.
- Added personal template packages with metadata, collected hashed audio, relative paths and background saving without interrupting the active session.
- Starting a template makes an independent unsaved session, backs up the old project to local History and supports undo/redo without overwriting the template.
- Added package validation, bounded local scans and rejection of invalid/incomplete templates.

## 0.7.0 — Missing audio recovery

- Added a nonmodal Project audio files window with deduplicated source references and background audits.
- Added manual Locate, recursive local folder search, explicit ambiguous-match selection and independent preview.
- Added shared-reference relinking with one undo, source-range/decoder/checksum validation and atomic batch failure behavior.
- Added search cancellation and stale-result protection, plus collected saving of repaired projects.

## 0.6.0 — Local sample browser and waveform editor

- Added background folder browsing, current-folder name search, scrolling, folder bookmarks and original-speed independent sample preview.
- Added a floating stereo waveform editor with bounded frame-aligned selection, zoom/scroll and independent preview/stop.
- Added selected-range transfer to sampler/Playlist, sampler source bounds and held-note looping.
- Added reverse, normalize and fade processing into separate retained local WAV copies, editor undo and source preservation.
- Introduced schema 5 for sampler selections/loops and portable saving of their assets.

## 0.5.1 — Playback repair

- Fixed the Play/Space startup path and keyboard focus handling, including replay after reaching the natural end.
- Added active-output checks and actionable device/callback failure status instead of leaving transport silently stalled.
- Added mono-output stereo folding and native transport diagnostics/regression checks for timeline advance and audible output.

## 0.5.0 — Mixer insert effects and routing

- Added a floating Mixer effects editor with stereo three-band EQ, stereo-linked compressor and algorithmic reverb controls/bypass.
- Added compressor external detector selection, gain-reduction feedback, dedicated buses and ordered output routing with cycle rejection.
- Added routing-aware solo behavior, undoable bus edits and effect/routing persistence in schema 4.

## 0.4.0 — Linked automation

- Added linked Song automation for channel volume, pan, tone, saturation, delay send and Master volume.
- Added a floating envelope editor with snapped point insertion/drag/delete, linear/held segments, point clipboard and grouped drag undo.
- Added independent automation-clip duplication, live control readouts and offline automation rendering.
- Introduced schema 3 for saved automation links and curves.

## 0.3.0 — Playlist audio and recording

- Added audio import/drop into Playlist tracks with waveform display, movement, edge trims, source offsets, gain and linear fades.
- Added split/copy/paste of audio clips while retaining continuous source positions and undo.
- Added local mono/stereo input capture to retained 24-bit WAV takes using a preallocated FIFO and disk worker, followed by undoable Playlist insertion.
- Added collected audio assets/relative references and schema 2 for audio clips.

## 0.2.0 — Familiar multiwindow interface

- Reworked the interface into a compact gray toolbar, Browser and movable/resizable/closable/maximizable Channel rack, Playlist, Piano roll, Mixer and channel settings windows.
- Added local layout persistence, stacking/hit protection and familiar F5/F6/F7/F9 window shortcuts and menus/context actions.
- Expanded note draw/paint/erase, move/resize, selection/lasso, velocity, clipboard/duplicate, transpose, zoom/scroll and typing-keyboard controls.
- Added step drag painting, pattern management, channel add/clone/delete and Playlist placement/clipboard gestures with undo/redo.

## 0.1.0 — Initial workstation prototype

- Established the native C++20/JUCE/CMake Windows application, local project model and audio engine.
- Added the initial pattern/step composition, Piano roll, Playlist and Mixer workflow, synthesized drums/bass/pad/lead, sampler playback and computer-keyboard audition.
- Added basic transport, static tempo/swing, mixer sound controls, local save/load/recovery, undo/redo, MIDI note-file interchange and offline WAV rendering.
- Established offline operation and the implementation roadmap. The more familiar floating interface followed in 0.2.
