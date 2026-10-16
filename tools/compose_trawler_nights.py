#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Compose an ORIGINAL 90s happy-hardcore / Eurodance instrumental in the OPEN app
using ONLY MCP calls.

This is an original track in a genre, not a transcription of any existing song.
Melody, harmony, bassline and sound design here are written for this file.
160 BPM, A minor, ten 16-beat sections: four-on-the-floor kick, offbeat synth
bass, supersaw stabs, gated noise sweeps and an original lead hook.
"""
from __future__ import annotations
import argparse
import base64
from datetime import datetime
import json
from pathlib import Path
import time

from mcp_client import PulseClient

TITLE = "Trawler Nights"
BPM = 160
SECTION = 16
SONG_BEATS = 160
BUS = 10
ROUTES = [1, 2, 3, 6, 7, 8, 9]  # wet sources feed the hall bus; kick/bass stay dry

# Am - F - C - G, one chord per bar. (bass root, triad, sub root)
CHORDS = [(45, [57, 60, 64], 33), (41, [53, 57, 60], 29), (48, [60, 64, 67], 36), (43, [55, 59, 62], 31)]

SECTIONS = [("intro", "Intro", 1), ("build-a", "Build up", 2), ("drop-1", "Drop one", 3),
            ("drop-1b", "Drop one again", 4), ("break", "Breakdown", 5), ("build-b", "Build again", 2),
            ("drop-2", "Drop two", 3), ("drop-2b", "Drop two again", 4), ("peak", "Full speed", 6),
            ("outro", "Outro", 7)]
PATTERN_NAMES = ["Intro", "Build up", "Drop one", "Drop one var", "Breakdown",
                 "Build again", "Drop two", "Drop two var", "Full speed", "Outro"]

# Original lead hooks (A minor). HOOK_A/B/C are distinct motifs written for this track.
HOOK_A = [(0, .5, 69), (0.5, .5, 72), (1, 1, 76), (2.5, .5, 74), (3, 1, 72),
          (4, .5, 65), (4.5, .5, 69), (5, 1, 72), (6.5, .5, 72), (7, 1, 69),
          (8, .5, 67), (8.5, .5, 72), (9, 1, 76), (10.5, .5, 76), (11, 1, 79),
          (12, .5, 74), (12.5, .5, 71), (13, 1.5, 67), (14.5, 1.5, 69)]
HOOK_B = [(0, .5, 81), (0.5, .5, 84), (1, 1, 88), (2.5, .5, 86), (3, 1, 84),
          (4, .5, 77), (4.5, .5, 81), (5, 1, 84), (6.5, .5, 84), (7, 1, 81),
          (8, .5, 79), (8.5, .5, 84), (9, 1, 88), (10.5, .5, 88), (11, 1, 91),
          (12, .5, 86), (12.5, .5, 83), (13, 1.5, 79), (14.5, .5, 81), (15, .5, 84)]
HOOK_C = [(0, 2, 76), (2, 2, 72), (4, 2, 77), (6, 2, 72), (8, 2, 79), (10, 2, 76), (12, 3, 72), (15, 1, 69)]


def n(beat, length, pitch, velocity):
    return {"beat": round(beat, 4), "length": length, "pitch": pitch, "velocity": round(velocity, 4)}


def sound_design():
    return [
        {"op": "set_song", "title": TITLE, "bpm": BPM, "master": .72, "delayFeedback": .22, "swing": 0,
         "songMode": True, "loop": False, "metronome": False,
         "notes": "Original happy-hardcore / Eurodance instrumental written for this file. 160 BPM, A minor, "
                  "Am - F - C - G loop, ten 16-beat sections: Intro, Build, Drop, Drop var, Breakdown, Build, "
                  "Drop, Drop var, Full speed, Outro. Four-on-the-floor kick, offbeat synth bass, supersaw "
                  "stabs, gated noise sweeps and an original lead hook. Genre homage only - this contains no "
                  "melody, hook or bassline from any existing recording."},
        {"op": "set_track", "track": 0, "name": "Kick", "instrument": "kick",
         "params": {"volume": .95, "pan": 0, "cutoff": 130, "drive": .16, "send": 0, "attack": .005, "release": .26},
         "effects": {"eq": {"low": 3, "mid": -2, "high": -4}}},
        {"op": "set_track", "track": 1, "name": "Clap", "instrument": "clap",
         "params": {"volume": .5, "pan": 0, "cutoff": 3600, "drive": .06, "send": .18, "attack": .003, "release": .3},
         "effects": {"reverb": {"mix": .22, "room": .7, "damping": .6}}},
        {"op": "set_track", "track": 2, "name": "Closed hat", "instrument": "hat",
         "params": {"volume": .3, "pan": .22, "cutoff": 6200, "drive": 0, "send": .06, "attack": .002, "release": .06},
         "effects": {"eq": {"low": -8, "mid": 0, "high": 3}}},
        {"op": "set_track", "track": 3, "name": "Open hat", "instrument": "open_hat",
         "params": {"volume": .26, "pan": -.26, "cutoff": 7200, "drive": 0, "send": .14, "attack": .002, "release": .3},
         "effects": {"reverb": {"mix": .18, "room": .6, "damping": .55}}},
        {"op": "set_track", "track": 4, "name": "Offbeat bass", "instrument": "analog",
         "synth": {"wave1": 2, "wave2": 1, "mix": .3, "coarse": 0, "detune": 0, "sub": .3, "noise": 0,
                   "cutoff": 900, "resonance": .4, "filterEnv": 1.2, "filterDecay": .25, "decay": .25,
                   "sustain": .3, "lfoRate": .1, "lfoFilter": 0},
         "params": {"volume": .62, "pan": 0, "cutoff": 1100, "drive": .18, "send": 0, "attack": .004, "release": .12},
         "effects": {"eq": {"low": 1, "mid": 2, "high": -3}}},
        {"op": "set_track", "track": 5, "name": "Sub bass", "instrument": "analog",
         "synth": {"wave1": 0, "wave2": 0, "mix": 0, "coarse": 0, "detune": 0, "sub": .85, "noise": 0,
                   "cutoff": 180, "resonance": .05, "filterEnv": 0, "filterDecay": 1, "decay": 2,
                   "sustain": .95, "lfoRate": .1, "lfoFilter": 0},
         "params": {"volume": .5, "pan": 0, "cutoff": 220, "drive": .05, "send": 0, "attack": .02, "release": .2}},
        {"op": "set_track", "track": 6, "name": "Supersaw stabs", "instrument": "analog",
         "synth": {"wave1": 2, "wave2": 2, "mix": .5, "coarse": 0, "detune": 35, "sub": .25, "noise": 0,
                   "cutoff": 3000, "resonance": .15, "filterEnv": .8, "filterDecay": 1.2, "decay": .6,
                   "sustain": .7, "lfoRate": .1, "lfoFilter": 0},
         "params": {"volume": .5, "pan": .1, "cutoff": 4200, "drive": .1, "send": .16, "attack": .006, "release": .35},
         "effects": {"reverb": {"mix": .26, "room": .8, "damping": .55},
                     "chorus": {"mix": .14, "rate": .5, "depth": .2, "width": .9}, "stereo": {"width": 1.3}}},
        {"op": "set_track", "track": 7, "name": "Lead hook", "instrument": "wavetable",
         "synth": {"table": 0, "tablePosition": .3, "tableEnv": .2, "tableDecay": 1.5, "tableLfo": .1,
                   "wave1": 2, "wave2": 3, "mix": .3, "detune": 12, "coarse": 0, "sub": .15, "noise": 0,
                   "cutoff": 4500, "resonance": .2, "filterEnv": .6, "filterDecay": .8, "decay": .5,
                   "sustain": .75, "lfoRate": 5.5, "lfoPitch": 8, "lfoFilter": .2},
         "params": {"volume": .42, "pan": -.08, "cutoff": 5200, "drive": .12, "send": .2, "attack": .01, "release": .3},
         "effects": {"reverb": {"mix": .3, "room": .82, "damping": .5},
                     "stereo": {"width": 1.25}}},
        {"op": "add_track", "name": "Arp pluck", "instrument": "fm",
         "synth": {"mix": 0, "sub": 0, "fmRatio": 2, "fmIndex": 1.5, "fmDecay": .15, "decay": .35,
                   "sustain": 0, "cutoff": 5000, "resonance": .1, "noise": 0},
         "params": {"volume": .3, "pan": .3, "cutoff": 5600, "drive": .04, "send": .24, "attack": .003, "release": .2},
         "effects": {"reverb": {"mix": .3, "room": .78, "damping": .5}}},
        {"op": "add_track", "name": "Noise sweep", "instrument": "analog",
         "synth": {"wave1": 0, "wave2": 0, "mix": 0, "coarse": 0, "detune": 0, "sub": 0, "noise": .5,
                   "cutoff": 700, "resonance": .35, "filterEnv": -1.2, "filterDecay": 4, "decay": 4,
                   "sustain": .9, "lfoRate": .1, "lfoFilter": 1.6},
         "params": {"volume": .22, "pan": 0, "cutoff": 900, "drive": 0, "send": .2, "attack": .4, "release": 1.2},
         "effects": {"eq": {"low": -8, "mid": -1, "high": 2},
                     "reverb": {"mix": .35, "room": .9, "damping": .6}, "stereo": {"width": 1.4}}},
        {"op": "add_track", "name": "Hall bus", "bus": True,
         "params": {"volume": .8, "cutoff": 20000, "send": 0},
         "effects": {"eq": {"low": -2, "mid": 0, "high": -1},
                     "reverb": {"mix": .3, "room": .85, "damping": .5}, "stereo": {"width": 1.2}}},
    ] + [{"op": "set_track", "track": t, "output": BUS} for t in ROUTES]


def kick_lane(gain=1):
    return [n(b, .2, 36, .9 * gain) for b in range(SECTION)]


def clap_lane(gain=1):
    return [n(b, .18, 39, .62 * gain) for b in range(1, SECTION, 2)]


def hat_lane(gain=1):
    return [n(b, .08, 42, .42 * gain) for b in [x + .5 for x in range(SECTION)]]


def ohat_lane(gain=1):
    return [n(b, .3, 46, .34 * gain) for b in (.5, 2.5, 6.5, 10.5, 14.5)]


def obass_lane(gain=1):
    out = []
    for bar in range(4):
        root = CHORDS[bar][0]
        for beat in [bar * 4 + o for o in (.5, 1.5, 2.5, 3.5)]:
            out.append(n(beat, .22, root, .68 * gain))
    return out


def sub_lane(gain=1):
    return [n(bar * 4, 3.9, CHORDS[bar][2], .6 * gain) for bar in range(4)]


def chord_lane(gain=1, offbeats=True):
    out = []
    for bar in range(4):
        triad = CHORDS[bar][1]
        beats = [bar * 4 + o for o in (.5, 1.5, 2.5, 3.5)] if offbeats else [bar * 4]
        length = .2 if offbeats else 3.9
        for pitch in triad:
            for beat in beats:
                out.append(n(beat, length, pitch, (.4 if offbeats else .34) * gain))
    return out


def arp_lane(gain=1):
    out = []
    for bar in range(4):
        triad = CHORDS[bar][1]
        seq = triad + [triad[0] + 12] + triad[::-1]
        for i, beat in enumerate([bar * 4 + o for o in (0, .5, 1, 1.5, 2, 2.5, 3, 3.5)]):
            out.append(n(beat, .18, seq[i % len(seq)], .34 * gain))
    return out


def lead_lane(hook, gain=1):
    return [n(b, l, p, .58 * gain) for b, l, p in hook]


def noise_lane(gain=1):
    return [n(0, 15.9, 60, .3 * gain)]


def score():
    """Ten sections of lane content, keyed by track index."""
    patterns = []

    def lanes(mapping):
        return {t: notes for t, notes in mapping.items() if notes}

    patterns.append(lanes({0: kick_lane(.7), 5: sub_lane(.8), 9: noise_lane(.5)}))
    patterns.append(lanes({0: kick_lane(), 1: clap_lane(.6), 2: hat_lane(.7), 4: obass_lane(.7),
                           5: sub_lane(), 9: noise_lane(1)}))
    patterns.append(lanes({0: kick_lane(), 1: clap_lane(), 2: hat_lane(), 3: ohat_lane(), 4: obass_lane(),
                           5: sub_lane(), 6: chord_lane(), 7: lead_lane(HOOK_A), 8: arp_lane(.7)}))
    patterns.append(lanes({0: kick_lane(), 1: clap_lane(), 2: hat_lane(), 3: ohat_lane(), 4: obass_lane(),
                           5: sub_lane(), 6: chord_lane(), 7: lead_lane(HOOK_B), 8: arp_lane()}))
    patterns.append(lanes({1: clap_lane(.4), 5: sub_lane(.6), 6: chord_lane(.8, offbeats=False),
                           7: lead_lane(HOOK_C, .8), 8: arp_lane(.5), 9: noise_lane(.4)}))
    patterns.append(lanes({0: kick_lane(.9), 1: clap_lane(1.2), 2: hat_lane(1), 4: obass_lane(.6),
                           6: chord_lane(.7), 9: noise_lane(1.2)}))
    patterns.append(lanes({0: kick_lane(), 1: clap_lane(), 2: hat_lane(), 3: ohat_lane(), 4: obass_lane(),
                           5: sub_lane(), 6: chord_lane(), 7: lead_lane(HOOK_A), 8: arp_lane()}))
    patterns.append(lanes({0: kick_lane(), 1: clap_lane(), 2: hat_lane(), 3: ohat_lane(), 4: obass_lane(1.1),
                           5: sub_lane(), 6: chord_lane(1.1), 7: lead_lane(HOOK_B, 1.1), 8: arp_lane(1)}))
    patterns.append(lanes({0: kick_lane(1.1), 1: clap_lane(1.1), 2: hat_lane(1.1), 3: ohat_lane(1.1),
                           4: obass_lane(1.15), 5: sub_lane(1.1), 6: chord_lane(1.15), 7: lead_lane(HOOK_B, 1.2),
                           8: arp_lane(1.15), 9: noise_lane(.5)}))
    patterns.append(lanes({0: kick_lane(.8), 2: hat_lane(.6), 4: obass_lane(.7), 5: sub_lane(.8),
                           6: chord_lane(.7, offbeats=False), 9: noise_lane(.7)}))
    return patterns


def automation():
    def clip(track, target, target_track, points):
        return {"op": "add_automation", "track": track, "target": target, "targetTrack": target_track,
                "beat": 0, "length": SONG_BEATS,
                "points": [{"beat": b, "value": v} for b, v in points]}
    return [
        clip(BUS, 5, -1, [(0, .3), (16, .72), (32, .8), (56, .78), (64, .5), (80, .62), (96, .85),
                          (128, .82), (144, .6), (160, .12)]),
        clip(9, 2, 9, [(0, .18), (16, .18), (28, .6), (32, .2), (80, .2), (92, .65), (96, .22), (160, .18)]),
        clip(6, 2, 6, [(0, .3), (32, .3), (64, .18), (96, .38), (144, .3), (160, .2)]),
    ]


def compose(workspace: Path, report_dir: Path):
    report_dir.mkdir(parents=True, exist_ok=True)
    history = []
    with PulseClient(workspace) as client:
        def call(tool, **arguments):
            result = client.call(tool, **arguments)
            history.append({"tool": tool, "arguments": arguments,
                            "result": result if tool != "pulse_snapshot" else {"image": True}})
            return result

        def write(tool, **arguments):
            return call(tool, expected_revision=call("pulse_status")["revision"], **arguments)

        before = call("pulse_get_project")
        (report_dir / "before-song-mcp.json").write_text(json.dumps(before, indent=2), encoding="utf-8")
        write("pulse_transport", action="stop")
        assert len(call("pulse_list_sounds")["factorySounds"]) == 56

        write("pulse_new_song", title=TITLE, bpm=BPM)
        print("MCP: prior session backed up to History; new Song-mode project started.", flush=True)

        write("pulse_edit", operations=sound_design())
        print("MCP: designed ten source voices and one shared hall bus.", flush=True)

        write("pulse_edit", operations=[{"op": "set_pattern", "pattern": 0, "name": PATTERN_NAMES[0],
                                         "length": SECTION}] +
              [{"op": "add_pattern", "name": name, "length": SECTION} for name in PATTERN_NAMES[1:]])

        parts = score()
        for start, stop in ((0, 4), (4, 7), (7, 10)):
            write("pulse_edit", operations=[{"op": "set_notes", "pattern": p, "track": t, "notes": notes}
                                            for p in range(start, stop) for t, notes in parts[p].items()])
        print("MCP: wrote ten sections of instrument parts.", flush=True)

        clips = [{"track": t, "pattern": p, "beat": p * SECTION, "length": SECTION}
                 for p in range(len(parts)) for t in parts[p]]
        write("pulse_edit", operations=[{"op": "set_arrangement", "clips": clips},
                                        {"op": "set_markers",
                                         "markers": [{"id": i, "name": nm, "beat": p * SECTION,
                                                      "endBeat": (p + 1) * SECTION, "colour": c}
                                                     for p, (i, nm, c) in enumerate(SECTIONS)]}])
        print(f"MCP: arranged {len(clips)} pattern clips across ten sections.", flush=True)

        write("pulse_edit", operations=automation())
        print("MCP: added the master ride and sweep/stab filter moves.", flush=True)

        write("pulse_view", panel="playlist")
        project = call("pulse_get_project")
        assert project["tracks"] == 11 and project["patterns"] == 10 and project["markers"] == 10
        assert project["project"]["schema"] == 13 and project["project"]["songMode"]
        assert project["project"]["bpm"] == BPM
        (report_dir / "composed-through-mcp.json").write_text(json.dumps(project, indent=2), encoding="utf-8")

        taken = (workspace / "Projects" / f"{TITLE}.pulse").exists()
        basename = TITLE + ("" if not taken else " " + datetime.now().strftime("%Y%m%d-%H%M%S"))
        saved = write("pulse_save_project", name=basename)
        image = call("pulse_snapshot")
        (report_dir / "trawler-nights-playlist.png").write_bytes(base64.b64decode(image["content"][0]["data"]))

        export = write("pulse_export_audio", name=basename)
        export_path = export["path"]
        deadline = time.monotonic() + 180
        state = call("pulse_status")
        while state["exporting"] and time.monotonic() < deadline:
            time.sleep(.5)
            state = call("pulse_status")
        if state["exporting"] or "Export complete" not in state["status"]:
            raise RuntimeError("Song export did not complete: " + state["status"])

        write("pulse_transport", action="seek", beat=0)
        write("pulse_transport", action="play")
        time.sleep(1)
        live = call("pulse_status")
        assert live["playing"] and live["positionBeats"] > 0 and live["activeOutputs"] > 0

        summary = {"title": TITLE, "bpm": BPM, "beats": SONG_BEATS,
                   "seconds": round(SONG_BEATS / BPM * 60, 1), "sourceTracks": 10, "buses": 1,
                   "patterns": 10, "patternClips": len(clips), "automationClips": len(automation()),
                   "sections": [nm for _, nm, _ in SECTIONS], "harmony": "Am - F - C - G",
                   "originalComposition": True, "project": saved["path"], "wav": export_path,
                   "previousSessionBackup": state["backup"], "allSongChangesThroughMcp": True,
                   "mcpCalls": len(history), "live": live}
        (report_dir / "mcp-composition-log.json").write_text(json.dumps(history, indent=2), encoding="utf-8")
        (report_dir / "song-result.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        print(json.dumps(summary, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--report-dir", required=True, type=Path)
    args = parser.parse_args()
    compose(args.workspace.resolve(), args.report_dir.resolve())
