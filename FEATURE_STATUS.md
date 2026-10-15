# Feature status — Pulse Studio 0.12

Full implementation scope remains in IMPLEMENTATION_PLAN.md. Partial means a functioning subset exists; it does not mean the full numbered feature is complete. Planned features have no working implementation yet. None of the full 107-feature roadmap is being declared fully verified by this release.

| Original ID | Feature | Status | Current boundary |
| --- | --- | --- | --- |
| 1 | Project creation, saving, and loading | Partial | Atomic JSON save/load and validation; schemas 1-6 readable; schema 7 adds stable Song markers and linked section loops to recording settings, sampler ranges/loops and mixer effects/routing; plugin states pending |
| 2 | Project templates | Partial | Six built-in starters; searchable local browser; personal full-project or channel/mixer packages with sample collection; independent instances, History backup and undo; recording-specific presets, per-asset selection, layout capture and startup-template preference pending |
| 3 | Autosave and crash recovery | Partial | 15-second local recovery snapshots; journal/crash-injection coverage pending |
| 4 | Project version history | Partial | Named local version files; version browser and comparisons pending |
| 5 | Unlimited undo and redo | Partial | 100-entry undo/redo; disk spill and unlimited history pending |
| 6 | Sample collection and portable projects | Partial | SHA-256 sample collection and relative paths; archive packaging pending |
| 7 | Missing sample and plugin recovery | Partial | Background audio audit, manual/local folder relinking, shared reference updates, ambiguity/checksum/range validation, undo and portable repaired saves; renamed-file fingerprint search and plugin recovery pending |
| 8 | Customizable workspace and window layouts | Partial | Movable, resizable, closable/maximizable editor panels with local stacking/layout persistence; named presets and multiple monitors pending |
| 9 | Themes and interface scaling | Partial | Familiar slate-gray interface and native DPI handling; custom themes and scaling controls pending |
| 10 | Customizable keyboard shortcuts | Partial | FL-style window and editing shortcuts; remapping pending |
| 11 | Command palette and feature search | Planned | See full plan |
| 12 | Contextual help and interactive tutorials | Partial | Local quick-start help; searchable tutorials pending |
| 13 | Audio device configuration | Partial | Windows Audio input/output, rate/buffer and input-channel selection; ASIO pending |
| 14 | Low-latency audio processing | Partial | Preallocated render path and native smoke test; sustained latency benchmarks pending |
| 15 | Multicore audio processing | Planned | See full plan |
| 16 | MIDI devices and controller mapping | Skipped | Excluded by user |
| 17 | MIDI devices and controller mapping | Skipped | Excluded by user |
| 18 | Computer keyboard musical input | Partial | Typing keyboard, virtual audition, and octave controls; global velocity/sustain controls pending |
| 19 | External synchronization and MPE | Skipped | Excluded by user |
| 20 | External synchronization and MPE | Skipped | Excluded by user |
| 21 | Transport controls, looping, and punch recording | Partial | Play/pause/stop, ruler seek, looping, audio and typing punch ranges with pre-roll/count-in and loop restoration; linked Song-section loops and navigation; note chase and advanced punch/loop takes pending |
| 22 | Tempo changes and tempo automation | Partial | Static adjustable tempo; tempo maps and ramps pending |
| 23 | Time signatures and meter changes | Planned | See full plan |
| 24 | Metronome, count-in, and tap tempo | Partial | Accented metronome including first beat, one/two-bar count-in, median tap tempo and gesture undo; click sound/level configuration pending |
| 25 | Channel rack | Partial | Rack with pan/volume controls and channel add/clone/delete; grouping and arbitrary routing pending |
| 26 | Step sequencer | Partial | Grouped 32-step rack with drag painting/erasing and melodic previews; variable lengths pending |
| 27 | Pattern creation and management | Partial | Create/clone/rename/select patterns with picker and navigation; merge/split pending |
| 28 | Probability, ratchets, and conditional steps | Partial | Probability and ratchets; conditional steps pending |
| 29 | Swing and groove templates | Partial | Swing; groove templates/extraction pending |
| 30 | Euclidean rhythm generation | Partial | Fixed local Euclidean rhythm generation; parameter panel pending |
| 31 | Piano roll | Partial | Draw/paint/erase/move/resize/transpose, ghost notes, selection/lasso, clipboard, duplicate, snap and zoom/scroll; advanced editing pending |
| 32 | Note velocity and expression editing | Partial | Velocity editing only; expression lanes pending |
| 33 | Chord, scale, and key tools | Partial | Scale shading and minor chord stamps; key/chord progression tools pending |
| 34 | Quantization and humanization | Partial | Basic quantization; strength/preview/humanization pending |
| 35 | Arpeggiator and MIDI effects | Planned | See full plan |
| 36 | Microtonal tuning | Planned | See full plan |
| 37 | MIDI recording and overdubbing | Partial | Typing-keyboard overdub, count-in, bounded punch recording and grouped take undo; replace/take lanes pending |
| 38 | Retrospective MIDI capture | Planned | See full plan |
| 39 | MIDI import and export | Partial | PPQ paired-note interchange and static tempo; maps/controllers pending |
| 40 | Playlist and song arrangement | Partial | Playlist with picker, pattern placement/movement/painting/deletion/cloning/clipboard and zoom/scroll; ripple editing pending |
| 41 | Audio, MIDI, pattern, and automation clips | Partial | Pattern, waveform audio and linked automation clips; standalone MIDI clips pending |
| 42 | Clip trimming, splitting, fades, and crossfades | Partial | Audio edge trims with source offsets, split at cursor, gain and linear fades; crossfade tools and time stretch pending |
| 43 | Grid snapping and free placement | Partial | Selectable note/clip snap, Alt bypass and free placement; adaptive/event snap pending |
| 44 | Track grouping and folders | Planned | See full plan |
| 45 | Arrangement markers and song sections | Partial | 128 named/coloured beat markers and ranges, Playlist rail, navigator/shortcuts, drag/resize with grouped undo, linked section loops and section export; absolute-time anchors, ripple behavior and section clip-copy/move pending |
| 46 | Alternate arrangements | Planned | See full plan |
| 47 | Clip launching and session view | Planned | See full plan |
| 48 | Live performance mode | Planned | See full plan |
| 49 | Multi-input audio recording | Partial | First/second enabled mono input or stereo to one Playlist track, raw input meters, dry background 24-bit WAV capture and monitoring through selected source effects/buses with independent gain; multiple destinations and latency compensation pending |
| 50 | Recording takes and comping | Partial | Separate retained local take files with undoable Playlist insertion; comping and take-lane UI pending |
| 51 | Retrospective audio capture | Planned | See full plan |
| 52 | Waveform editor | Partial | Stereo selection/zoom, independent preview, reverse/normalize/fades into retained local copies, editor undo, and selected ranges to sampler/Playlist; cut/splice and advanced processing pending |
| 53 | Audio slicing and transient detection | Planned | See full plan |
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
| 65 | Sample chopping and beat slicing | Planned | See full plan |
| 66 | Wavetable synthesizer | Planned | See full plan |
| 67 | Subtractive synthesizer | Partial | Basic polyphonic oscillator/envelope instruments; full subtractive synth editor pending |
| 68 | FM synthesizer | Planned | See full plan |
| 69 | Additive synthesizer | Planned | See full plan |
| 70 | Granular synthesizer | Planned | See full plan |
| 71 | Physical modeling instruments | Planned | See full plan |
| 72 | Built-in instrument and preset library | Partial | Original synthesized sounds and demo; preset browser/asset packs pending |
| 73 | Plugin hosting | Planned | See full plan |
| 74 | Plugin scanning and management | Planned | See full plan |
| 75 | Plugin crash isolation | Planned | See full plan |
| 76 | Plugin preset management | Planned | See full plan |
| 77 | External hardware instruments and effects | Skipped | Excluded by user |
| 78 | Mixer with flexible channel routing | Partial | Single-output insert/bus graph with cycle rejection, ordered processing and upstream/downstream solo; multi-output routing pending |
| 79 | Sends, buses, groups, and sidechains | Partial | Dedicated buses, nested outputs and external compressor detector; send matrices/groups and independent mixer channels pending |
| 80 | Parallel processing | Planned | See full plan |
| 81 | Plugin delay compensation | Planned | See full plan |
| 82 | Equalizer and spectrum analyzer | Partial | Three-band stereo EQ with response display and bypass; variable bands and spectrum analysis pending |
| 83 | Compressor, limiter, gate, and expander | Partial | Stereo-linked peak compressor, attack/release/makeup, external sidechain and gain-reduction meter; limiter/gate/expander pending |
| 84 | Multiband dynamics processing | Planned | See full plan |
| 85 | Reverb, delay, and convolution effects | Partial | Ping-pong delay and per-insert stereo algorithmic reverb; convolution and advanced controls pending |
| 86 | Distortion, saturation, and amp simulation | Partial | Basic saturation; oversampling/amp simulation pending |
| 87 | Chorus, flanger, phaser, and modulation effects | Planned | See full plan |
| 88 | Stereo imaging and spatial audio | Partial | Stereo sampling and panning; width/mid-side/spatial audio pending |
| 89 | Loudness, peak, phase, and correlation meters | Partial | Mixer peak displays plus raw input dBFS sample-peak meters with transient accumulation and clipping hold; loudness/true-peak/phase pending |
| 90 | Reference track comparison | Planned | See full plan |
| 91 | Automation recording and editing | Partial | Draw/drag/delete points, snapping, linear/held segments, clipboard and undo, live control readouts and offline export; recording modes pending |
| 92 | Automation curves and modulation generators | Partial | Linear and held segments for six mixer targets; tension, generators and plugin/tempo targets pending |
| 93 | Macro controls and parameter linking | Planned | See full plan |
| 94 | Modular instrument and effect chains | Planned | See full plan |
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
| 112 | Scripting and extension API | Planned | See full plan |
| 113 | Integrated content downloads | Skipped | Excluded by user |
| 114 | Cross-platform project compatibility | Planned | See full plan |
| 115 | FL Studio project import where technically supported | Planned | See full plan |
