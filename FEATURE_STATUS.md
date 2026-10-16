# Feature status — Pulse Studio 0.37

Full implementation scope remains in IMPLEMENTATION_PLAN.md. Partial means a functioning subset exists; it does not mean the full numbered feature is complete. Planned features have no working implementation yet. The complete roadmap is not being declared finished by this release.

| Original ID | Feature | Status | Current boundary |
| --- | --- | --- | --- |
| 1 | Project creation, saving, and loading | Partial | Atomic JSON save/load and validation, safe last-saved/opened project startup; schemas 1-13 readable; schema 14 stores pattern colours and complete wavetable patches and per-insert stereo width/mono/swap/polarity/trim alongside complete Analog/FM patch state and ordered EQ/compression/reverb/chorus/flanger/phaser settings alongside Song markers, recording settings, sampler ranges/loops and mixer effects/routing; plugin states pending |
| 2 | Project templates | Partial | Six built-in starters; searchable local browser; personal full-project or channel/mixer packages with sample collection; independent instances, History backup and undo; recording-specific presets, per-asset selection, layout capture and startup-template preference pending |
| 3 | Autosave and crash recovery | Partial | 15-second local recovery snapshots; journal/crash-injection coverage pending |
| 4 | Project version history | Partial | Named local version files; version browser and comparisons pending |
| 5 | Unlimited undo and redo | Partial | 100-entry undo/redo; disk spill and unlimited history pending |
| 6 | Sample collection and portable projects | Partial | SHA-256 sample collection and relative paths; archive packaging pending |
| 7 | Missing sample and plugin recovery | Partial | Background audio audit, manual/local folder relinking, shared reference updates, ambiguity/checksum/range validation, undo and portable repaired saves; renamed-file fingerprint search and plugin recovery pending |
| 8 | Customizable workspace and window layouts | Partial | Movable, resizable, closable/maximizable editor panels with local stacking/layout persistence; main window fits monitor work areas and Ctrl+Shift+H recovers offscreen bounds; three factory and up to 32 named personal workspaces with safe recall/update/rename/delete, local file exchange and command search; docking and independent editor windows across monitors pending |
| 9 | Themes and interface scaling | Partial | Familiar slate-gray interface, native DPI handling and automatic workspace downscaling on small windows plus saved 75–200% requested scaling capped to fit, transformed input and menu anchors; custom themes and contrast/accessibility verification pending |
| 10 | Customizable keyboard shortcuts | Partial | FL-style window and editing shortcuts; remapping pending |
| 11 | Command palette and feature search | Partial | Ctrl+P token search over editor actions, channels, patterns, clips, markers, factory instruments, factory/personal sounds, templates and help; shared action dispatch, keyboard navigation, disabled/stale context protection; plugin/full-help indexing and fuzzy matching pending |
| 12 | Contextual help and interactive tutorials | Partial | Local quick-start help; searchable tutorials pending |
| 13 | Audio device configuration | Partial | Windows Audio input/output, rate/buffer and input-channel selection; ASIO pending |
| 14 | Low-latency audio processing | Partial | Preallocated render path and native smoke test; sustained latency benchmarks pending |
| 15 | Multicore audio processing | Planned | See full plan |
| 16 | MIDI devices and controller mapping | Skipped | Excluded by user |
| 17 | MIDI devices and controller mapping | Skipped | Excluded by user |
| 18 | Computer keyboard musical input | Partial | Typing/virtual audition, saved octave/velocity/semitone transposition, sustain latch, source-correct note releases and focus/Stop flushing, velocity/sustain-aware takes; sample-offset input compensation and virtual-keyboard recording remain pending |
| 19 | External synchronization and MPE | Skipped | Excluded by user |
| 20 | External synchronization and MPE | Skipped | Excluded by user |
| 21 | Transport controls, looping, and punch recording | Partial | Play/pause/stop, ruler seek, looping, audio and typing punch ranges with pre-roll/count-in and loop restoration; linked Song-section loops and navigation; held melodic gate chase on seek/resume/loop starts with sampler source offsets and duplicate-voice protection; historical synth/effect-state reconstruction, release-tail chase and advanced punch/loop takes pending |
| 22 | Tempo changes and tempo automation | Partial | Static adjustable tempo; tempo maps and ramps pending |
| 23 | Time signatures and meter changes | Planned | See full plan |
| 24 | Metronome, count-in, and tap tempo | Partial | Accented metronome including first beat, one/two-bar count-in, median tap tempo and gesture undo; Classic/Wood/Hi-hat/Rim clicks, 0–200% level, equal/bar-accent choice shared by playback/count-in, atomic local preferences and export exclusion; recording-only mode, separate click routing, arbitrary count-in lengths and tempo/meter maps pending |
| 25 | Channel rack | Partial | Rack with pan/volume controls and channel add/clone/delete; grouping and arbitrary routing pending |
| 26 | Step sequencer | Partial | Pattern-driven 4-256-step display, template-length recall and 16-64-step whole-beat length dialog with grouped undo and stale/recording guards; per-channel polymetric lengths pending |
| 27 | Pattern creation and management | Partial | Create/clone/rename/select patterns, usage counts, selected-clip Make unique, channel split with playback-preserving instance relinking, layered and sequential merges into new patterns, atomic capacity guards and grouped undo; reorder, confirmed unused-pattern deletion and explicit confirmed used-pattern/instance deletion with safe clipboard identities, selection remapping and undo; twelve saved pattern colours plus Automatic across Browser/rack/pickers/Playlist/Piano roll, inheritance and MCP edits; explicit Copy/Move of selected notes from one channel, optional first-onset alignment, keep-all/exact-duplicate/current/incoming merge policies and atomic MCP equivalents; capacity checked after conflict handling, with source/selection guards and one undo; multi-channel extraction and fuzzy/gate-overlap resolution outside the current scope |
| 28 | Probability, ratchets, and conditional steps | Partial | Probability and ratchets; conditional steps pending |
| 29 | Swing and groove templates | Partial | Swing; groove templates/extraction pending |
| 30 | Euclidean rhythm generation | Partial | Euclidean steps/pulses/rotation/repeats, typed counters, pitch/range, velocity/gate/seeded variation, fill or pitch-only replacement, live A/B preview, original-note overlay and one-step Apply/undo; invalid/dense/Song-capacity guards; local named/factory rhythm presets with JSON import/export; multichannel and live generative processors pending |
| 31 | Piano roll | Partial | Draw/paint/erase/move/resize/transpose, ghost notes, selection/lasso, clipboard, duplicate, snap and zoom/scroll; advanced editing pending |
| 32 | Note velocity and expression editing | Partial | Velocity editing only; expression lanes pending |
| 33 | Chord, scale, and key tools | Partial | Scale shading and minor chord stamps; key/chord progression tools pending |
| 34 | Quantization and humanization | Partial | Selected/all-lane quantize with grid/strength/swing/window and independent duration strength; seeded timing/length/velocity humanize with chord timing; audible A/B, original-note overlay, explicit apply/cancel, one-step undo and save/export guards; local named/factory presets and JSON import/export; groove files, exclusion rules and expression handling pending |
| 35 | Arpeggiator and MIDI effects | Partial | Selected/all-note held-chord arpeggiator with six orders, straight/triplet rates, octave expansion, transpose, gate/velocity, chord restart, seeded random order, audible A/B, atomic capacity guards and one-step commit/undo; unselected notes and source probability/ratchets preserved; local named/factory arpeggiator presets and JSON import/export; live event processors and general MIDI-effects chains pending |
| 36 | Microtonal tuning | Planned | See full plan |
| 37 | MIDI recording and overdubbing | Partial | Typing-keyboard overdub, count-in, bounded punch recording and grouped take undo; replace/take lanes pending |
| 38 | Retrospective MIDI capture | Partial | Memory-only computer-keyboard phrase capture with enable/disable and 15/30/60-second history, 512-event/64-beat bounds, performed pitches/timing/velocity/sustain, current-tempo conversion and first-onset alignment; one-undo new-pattern recall preserves original notes/Playlist and consumes successful captures. Transport/tempo context, alternate alignment and controller/expression capture pending |
| 39 | MIDI import and export | Partial | PPQ paired-note interchange and static tempo; maps/controllers pending |
| 40 | Playlist and song arrangement | Partial | Playlist with picker, pattern placement/movement/painting/deletion/cloning/clipboard and zoom/scroll; ripple editing pending |
| 41 | Audio, MIDI, pattern, and automation clips | Partial | Pattern, waveform audio and linked automation clips; standalone MIDI clips pending |
| 42 | Clip trimming, splitting, fades, and crossfades | Partial | Audio edge trims with source offsets, split at cursor, gain and linear fades; crossfade tools and time stretch pending |
| 43 | Grid snapping and free placement | Partial | Selectable note/clip snap, Alt bypass and free placement; adaptive/event snap pending |
| 44 | Track grouping and folders | Planned | See full plan |
| 45 | Arrangement markers and song sections | Partial | 128 named/coloured beat markers and ranges, Playlist rail, navigator/shortcuts, drag/resize with grouped undo, linked section loops and section export; whole-clip section Copy/Move includes pattern/audio/automation with offset/fade/point/link retention, explicit lane/automation-target overlap policy, boundary/selection/capacity guards, native and MCP atomic Undo; absolute-time anchors, ripple behavior, crossing-clip transforms and automatic marker relocation pending |
| 46 | Alternate arrangements | Planned | See full plan |
| 47 | Clip launching and session view | Planned | See full plan |
| 48 | Live performance mode | Planned | See full plan |
| 49 | Multi-input audio recording | Partial | First/second enabled mono input or stereo to one Playlist track, raw input meters, dry background 24-bit WAV capture and monitoring through selected source effects/buses with independent gain; multiple destinations and latency compensation pending |
| 50 | Recording takes and comping | Partial | Separate retained local take files with undoable Playlist insertion; comping and take-lane UI pending |
| 51 | Retrospective audio capture | Planned | See full plan |
| 52 | Waveform editor | Partial | Stereo selection/zoom and preview; cut/copy/insert or replace paste/duplicate/trim/delete/silence plus reverse/normalize/fades in retained float32 copies; separate 20-edit undo/redo including selection/view; selected ranges to sampler/Playlist. Band-limited offline sample-rate conversion (8-192 kHz) and automatic cross-rate insert/replace paste preserve pitch/timing, use a cancellable background worker and retained float32 files, with stale-source/selection/recording/write guards and independent audio Undo/Redo. Existing source files and clipboard format retained; other background edits, persistent edit sessions and advanced processing pending |
| 53 | Audio slicing and transient detection | Partial | Stereo amplitude-rise detection with floor/ratio/minimum gap, 256 temporary editable frame markers and contiguous original-speed grid/irregular Playlist slicing with batch undo/redo; spectral detection, persistent maps and advanced marker editing pending |
| 54 | Time stretching and pitch shifting | Planned | See full plan |
| 55 | Audio warping and tempo matching | Planned | See full plan |
| 56 | Pitch correction and vocal editing | Planned | See full plan |
| 57 | Noise reduction and audio restoration | Planned | See full plan |
| 58 | Spectral audio editing | Planned | See full plan |
| 59 | Audio-to-MIDI conversion | Planned | See full plan |
| 60 | Drum sampler and drum pads | Partial | Basic synthesized drums and sample channels; pad/choke/layer UI pending |
| 61 | Multisample instruments and sample mapping | Planned | See full plan |
| 62 | Sample browser, tagging, and favorites | Partial | Factory/project Browser plus background local folder navigation, current-folder name search, original-speed previews and 32 folder bookmarks; recursive metadata index, file favorites and tagging pending |
| 63 | Sample preview synchronized to project tempo | Planned | Independent stereo preview works at original speed; synchronization/stretching remains pending |
| 64 | Sample recording and resampling | Partial | Local input takes can be loaded in samplers; internal resampling capture pending |
| 65 | Sample chopping and beat slicing | Partial | Waveform selection to Playlist at five beat grids or detected/manually corrected attack markers, preserving exact source offsets and short final slices; sampler pad distribution, choke groups and persistent slice mapping pending |
| 66 | Wavetable synthesizer | Partial | Pulse Wavetable: eight original four-frame families, harmonic mipmaps, position/envelope/LFO morphing, two oscillators, shared ADSR/filter/LFO, waveform display, native controls/undo, project/template/export and sound/library persistence; user table import/authoring, unison, sync, oversampling and automation pending |
| 67 | Subtractive synthesizer | Partial | Pulse Analog two-oscillator editor, four waveforms with PolyBLEP saw/pulse, detune/coarse/pulse width/sub/noise, ADSR, resonant low-pass/envelope and vibrato/filter LFO, smoothed controls and patch persistence; unison, sync, modulation matrix, polyphony modes and synth automation pending |
| 68 | FM synthesizer | Partial | Pulse FM fixed two-operator sine engine with ratio/index/decay and operator mix, shared ADSR/filter/LFO editor and patches; more operators/algorithms, feedback, oversampling and synth automation pending |
| 69 | Additive synthesizer | Planned | See full plan |
| 70 | Granular synthesizer | Planned | See full plan |
| 71 | Physical modeling instruments | Planned | See full plan |
| 72 | Built-in instrument and preset library | Partial | 56 original factory sounds across Analog/FM/Wavetable plus up to 128 personal sounds, stable identities, categories/favorites, native library management and Ctrl+P search, explicit update/rename/delete, atomic local library and .pulsesound import/export; sound/library schema 2 reads schema 1; complete synth state stays embedded in projects/templates/recovery. Drum kits, pack/recursive-folder indexing, richer tags, multi-library/bulk merge and asset packs pending |
| 73 | Plugin hosting | Planned | See full plan |
| 74 | Plugin scanning and management | Planned | See full plan |
| 75 | Plugin crash isolation | Planned | See full plan |
| 76 | Plugin preset management | Planned | See full plan |
| 77 | External hardware instruments and effects | Skipped | Excluded by user |
| 78 | Mixer with flexible channel routing | Partial | Single-output insert/bus graph with cycle rejection, ordered processing and upstream/downstream solo; multi-output routing pending |
| 79 | Sends, buses, groups, and sidechains | Partial | Dedicated buses, nested outputs and external compressor detector; send matrices/groups and independent mixer channels pending |
| 80 | Parallel processing | Planned | See full plan |
| 81 | Plugin delay compensation | Planned | See full plan |
| 82 | Equalizer and spectrum analyzer | Partial | Three-band stereo EQ plus live Master/selected-channel 2048-point stereo Hann spectrum, calibrated amplitude dBFS, bounded capture and freeze/reset; variable bands, pre/post selection and FFT options pending |
| 83 | Compressor, limiter, gate, and expander | Partial | Stereo-linked peak compressor, attack/release/makeup, external sidechain and gain-reduction meter; limiter/gate/expander pending |
| 84 | Multiband dynamics processing | Planned | See full plan |
| 85 | Reverb, delay, and convolution effects | Partial | Ping-pong delay and per-insert stereo algorithmic reverb; convolution and advanced controls pending |
| 86 | Distortion, saturation, and amp simulation | Partial | Basic saturation; oversampling/amp simulation pending |
| 87 | Chorus, flanger, phaser, and modulation effects | Partial | Stereo chorus, flanger and six-stage phaser on source/bus inserts; six editable/smoothed controls per effect, native typed values, bypass, undo, project/template persistence and offline rendering; ordering, eight factory chains, copy/paste and local chain-file exchange also work; tempo sync, effect automation, selectable stage counts and duplicate instances pending |
| 88 | Stereo imaging and spatial audio | Partial | Stereo sampling/pan plus source/bus width (mid preserved, 0–200% side), mono, input polarity, L/R swap and output trim with smoothing, typed controls, undo and schema-12 project/template persistence; independent mid/side processing, stereo delay and spatial formats pending |
| 89 | Loudness, peak, phase, and correlation meters | Partial | Live stereo peak/RMS, held peaks, full-scale sample counts and L/R correlation with freeze/reset; raw input peak/clip hold; loudness, true peak, phase scope and export reports pending |
| 90 | Reference track comparison | Planned | See full plan |
| 91 | Automation recording and editing | Partial | Draw/drag/delete points, snapping, linear/held segments, clipboard and undo, live control readouts and offline export; recording modes pending |
| 92 | Automation curves and modulation generators | Partial | Linear and held segments for six mixer targets; tension, generators and plugin/tempo targets pending |
| 93 | Macro controls and parameter linking | Planned | See full plan |
| 94 | Modular instrument and effect chains | Partial | Reorder the six built-in insert effects with stable identities and undo, eight factory chains, clipboard and atomic validated .pulseeffects files; preserve source/mixer/routing state on recall. One instance each; arbitrary processors, duplicate instances, parallel graphs and personal chain indexing pending |
| 95 | Track freezing, flattening, and bouncing | Planned | See full plan |
| 96 | Audio export and batch rendering | Partial | Background snapshot WAV/FLAC at 44.1/48/96 kHz, mono/stereo, PCM16/24 or float32 WAV, ranges with timeline warm-up and configurable tails; atomic per-file finalization and cancellation; other codecs, real-time and job queue pending |
| 97 | Stem and multitrack export | Partial | Isolated source/bus batches share encoding/range/normalization options and include routed processing; additive stems and solo-independent sidechain rendering pending |
| 98 | Export normalization, dithering, and metadata | Partial | Two-pass sample-peak normalization (-24 to 0 dBFS), deterministic TPDF before final integer quantization, float dither exclusion and automated output checks; metadata, loudness normalization, true-peak limiting and noise shaping pending |
| 99 | Video playback and scoring synchronization | Planned | See full plan |
| 100 | Musical notation view and export | Planned | See full plan |
| 101 | Project notes, lyrics, and track comments | Partial | Local project notes and naming; per-track comments/timed lyrics pending |
| 102 | Collaboration, cloud backup, and synchronization | Skipped | Excluded by user |
| 103 | Collaboration, cloud backup, and synchronization | Skipped | Excluded by user |
| 104 | Stem separation | Planned | See full plan |
| 105 | Assisted chord, melody, and rhythm generation | Partial | Local editable rule-generated notes; richer controls and models pending |
| 106 | Natural-language project commands | Planned | See full plan |
| 107 | Mix analysis and mastering assistance | Planned | See full plan |
| 108 | Searchable action history | Partial | Undo history exists internally; searchable history UI pending |
| 109 | CPU, memory, and audio dropout diagnostics | Partial | DSP load/peak/device local reports; detailed resource diagnostics pending |
| 110 | Accessibility and screen reader support | Planned | See full plan |
| 111 | Touchscreen support | Planned | See full plan |
| 112 | Scripting and extension API | Partial | Opt-in local MCP stdio API: revision-checked live sound/track/pattern/note/Playlist/section/automation edits, backup/undo, transport, save/export, views and screenshots; declared operations only, including selected-note Copy/Move, layered/sequential conflict-aware pattern merging and guarded whole-section clip Copy/Move, with local session/expiry/replay guards; model-independent project skill and complete API reference with a verified composition example included in portable/source packages. General scripting, plugin automation and extension hosting pending |
| 113 | Integrated content downloads | Skipped | Excluded by user |
| 114 | Cross-platform project compatibility | Planned | See full plan |
| 115 | FL Studio project import where technically supported | Planned | See full plan |

Release packaging: `package.cmd` creates an offline Windows x64 ZIP with a statically linked MSVC runtime, PE dependency verification, hashes, notices and complete corresponding source including JUCE. Header menus anchor to their controls; context menus anchor to the cursor with target DPI retained. Clean-machine testing on the reported second PC, signed installers and non-Windows binaries remain pending.
