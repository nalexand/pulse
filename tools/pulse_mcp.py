#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Local MCP stdio server for the live Pulse Studio UI. Python standard library only."""
from __future__ import annotations
import argparse
import base64
import json
import math
import os
from pathlib import Path
import re
import sys
import time
import uuid

VERSION = "0.95.0"
MAX_MESSAGE = 2 * 1024 * 1024

def obj(properties: dict, required=()) -> dict:
    return {"type": "object", "properties": properties, "required": list(required), "additionalProperties": False}

def num(low: float, high: float, integer=False) -> dict:
    return {"type": "integer" if integer else "number", "minimum": low, "maximum": high}

def string(limit=80, **kwargs) -> dict:
    return {"type": "string", "minLength": 1, "maxLength": limit, **kwargs}

def array(items: dict, maximum: int, minimum=0) -> dict:
    return {"type": "array", "items": items, "maxItems": maximum, "minItems": minimum}

BOOL = {"type": "boolean"}
TRACK = num(0, 15, True)
PATTERN = num(0, 31, True)
REVISION = num(0, 2**53 - 1, True)
NAME = string(pattern=r"^[A-Za-z0-9][A-Za-z0-9 _-]{0,79}$")
SYNTH = obj({
    "physical": obj({"mode": num(0,1,True), "decay": num(.1,20), "damping": num(0,1), "brightness": num(.05,1), "position": num(.05,.95), "body": num(0,1), "stiffness": num(0,1)}, ("mode","decay","damping","brightness","position","body","stiffness")),
    "harmonics": array(num(0, 1), 16, 16),
    **{k: num(0, 3, True) for k in ("wave1", "wave2")}, "coarse": num(-24, 24, True),
    "table": num(0, 7, True), "tablePosition": num(0, 1), "tableEnv": num(-1, 1),
    "tableDecay": num(.01, 8), "tableLfo": num(0, 1), "mix": num(0, 1), "detune": num(-50, 50),
    "pulse": num(.05, .95), "sub": num(0, 1), "noise": num(0, .5), "decay": num(.01, 4),
    "sustain": num(0, 1), "cutoff": num(40, 20000), "resonance": num(0, 1),
    "filterEnv": num(-6, 6), "filterDecay": num(.01, 4), "fmRatio": num(.25, 16),
    "fmIndex": num(0, 12), "fmDecay": num(.01, 4), "lfoRate": num(.1, 20),
    "lfoPitch": num(0, 100), "lfoFilter": num(0, 4),
})
TUNING = obj({"enabled": BOOL, "name": string(100), "referenceHz": num(220,880), "cents": array(num(-24000,24000),128,128)}, ("enabled","name","referenceHz","cents"))
PARAMS = obj({"volume": num(0, 1.5), "pan": num(-1, 1), "cutoff": num(60, 20000),
              "drive": num(0, 1), "send": num(0, .8), "attack": num(.001, 2), "release": num(.02, 3), "sampleRoot": num(0, 127, True), "sampleTune": num(-100, 100)})
EFFECTS = obj({
    "delay": obj({"enabled": BOOL, "repeats": num(1,16,True), "interval": num(20,1000), "fade": num(0,1), "mix": num(0,1)}),
    "distortion": obj({"enabled": BOOL, "mode": num(0,2,True), "drive": num(0,48), "lowCut": num(20,1000), "tone": num(800,12000), "presence": num(-12,12), "asymmetry": num(0,1), "cabinet": num(0,1), "mix": num(0,1), "output": num(-24,6)}),
    "eq": obj({"enabled": BOOL, **{k: num(-18, 18) for k in ("low", "mid", "high")}}),
    "order": {**array(num(0, 7, True), 8, 8), "uniqueItems": True},
    "compressor": obj({"enabled": BOOL, "threshold": num(-60, 0), "ratio": num(1, 20), "attack": num(.1, 100), "release": num(5, 1000), "makeup": num(-12, 24), "mode": {"enum":["compressor","limiter","gate","expander"]}, "range":num(0,96), "hysteresis":num(0,24), "hold":num(0,1000), "knee":num(0,24)}),
    **{name: obj({"enabled": BOOL, "mix": num(0, 1), "rate": num(.02, 5), "depth": num(0, 1),
                 "centre": num(.2, 8) if name=="flanger" else num(100, 4000),
                 "feedback": num(-.85, .85), "width": num(0, 1)}) for name in ("flanger", "phaser")},
    "reverb": obj({"enabled": BOOL, **{k: num(0, 1) for k in ("mix", "room", "damping")}}),
    "chorus": obj({"enabled": BOOL, "mix": num(0, 1), "rate": num(.05, 8), "depth": num(0, 1),
                   "delay": num(5, 35), "feedback": num(-.8, .8), "width": num(0, 1)}),
    "stereo": obj({**{k: BOOL for k in ("enabled", "mono", "swap", "invertLeft", "invertRight")},
                   "width": num(0, 2), "trim": num(-24, 6)}),
})
NOTE = obj({"beat": num(0, 64), "length": num(.01, 64), "pitch": num(0, 127, True),
            "velocity": num(0, 1), "probability": num(0, 1), "ratchets": num(1, 8, True),
            "conditionEvery": num(1, 16, True), "conditionStep": num(1, 16, True)},
           ("beat", "length", "pitch", "velocity"))
CLIP = obj({"track": TRACK, "pattern": PATTERN, "beat": num(0, 4096), "length": num(.0625, 600)},
           ("track", "pattern", "beat", "length"))
MARKER = obj({"id": string(), "name": string(), "beat": num(0, 4096), "endBeat": num(-1, 4096),
              "colour": num(0, 7, True)}, ("id", "name", "beat", "colour"))
POINT = obj({"beat": num(0, 4096), "value": num(0, 1), "hold": BOOL, "curve": num(0, 3, True), "tension": num(-1, 1)}, ("beat", "value"))

def operation(kind: str, props: dict, required=()) -> dict:
    return obj({"op": {"enum": [kind]}, **props}, ("op", *required))

AUX_SEND = obj({"target": num(-1,15,True), "level": num(0,1.5), "preFader": BOOL}, ("target","level","preFader"))
track_properties = {"auxSends": array(AUX_SEND,4,4),"chokeGroup": num(0, 16, True), "mute": BOOL, "solo": BOOL,"name": string(), "instrument": {"enum": ["kick", "snare", "hat", "clap", "open_hat", "bass", "pad", "lead", "analog", "fm", "wavetable", "additive", "physical"]},
                    "preset": string(), "synth": SYNTH, "tuning": TUNING, "params": PARAMS, "effects": EFFECTS, "output": num(-1, 15, True)}
OPERATIONS = {"oneOf": [
    operation("set_song", {"title": string(200), "bpm": num(30, 300), "swing": num(0, .6), "master": num(0, 1),
                           "delayFeedback": num(0, .8), "delayReturn": num(0, 1), "masterSoftClip": BOOL, "masterEffects": EFFECTS, "songMode": BOOL, "loop": BOOL, "metronome": BOOL,
                           "notes": {"type": "string", "maxLength": 16000}}),
    operation("add_track", {**track_properties, "bus": BOOL}, ("name",)),
    operation("set_track", {"track": TRACK, **track_properties}, ("track",)),
    operation("project_commands", {"text": string(4096)}, ("text",)),
    operation("add_pattern", {"name": string(100), "length": num(1, 64), "colour": num(0, 12, True)}, ("name",)),
    operation("set_pattern", {"pattern": PATTERN, "name": string(100), "length": num(1, 64), "colour": num(0, 12, True)}, ("pattern",)),
    operation("extract_notes", {"pattern": PATTERN, "track": TRACK,
                                "indices": {**array(num(0, 511, True), 512, 1), "uniqueItems": True},
                                "move": BOOL, "alignStart": BOOL}, ("pattern", "track", "indices")),
    operation("merge_patterns", {"first": PATTERN, "second": PATTERN,
                                 "mode": {"type": "string", "enum": ["layered", "sequential"]},
                                 "policy": {"type": "string", "enum": ["keep_all", "skip_exact", "prefer_first", "prefer_second"]}}, ("first", "second")),
    operation("set_notes", {"pattern": PATTERN, "track": TRACK, "notes": array(NOTE, 512)}, ("pattern", "track", "notes")),
    operation("set_arrangement", {"clips": array(CLIP, 512)}, ("clips",)),
    operation("set_audio_fades", {"clip": num(0, 511, True), "fadeIn": num(0, 600), "fadeOut": num(0, 600), "fadeInCurve": num(0, 2, True), "fadeOutCurve": num(0, 2, True)}, ("clip",)),
    operation("set_markers", {"markers": array(MARKER, 128)}, ("markers",)),
    operation("ripple_time", {"start": num(0,4096), "length": num(.25,4096), "remove": BOOL}, ("start","length","remove")),
    operation("section_clips", {"section": string(80), "destination": num(0, 4096), "move": BOOL, "allowOverlap": BOOL}, ("section", "destination")),
    operation("set_automation_points", {"clip": num(0, 511, True), "points": array(POINT, 256, 2)}, ("clip", "points")),
    operation("generate_automation", {"clip": num(0, 511, True), "wave": {"enum": ["sine", "triangle", "square", "steps", "random_hold", "random_smooth"]}, "period": num(.0625, 600), "phase": num(0, 1), "low": num(0, 1), "high": num(0, 1), "resolution": num(4, 64, True), "seed": num(0, 4294967295, True), "steps": array(num(0, 1), 32, 2)}, ("clip", "wave", "period")),
    operation("add_automation", {"track": TRACK, "target": {"oneOf": [num(0,125,True),string(64)]}, "targetTrack": num(-1, 15, True),
                                 "beat": num(0, 4096), "length": num(.0625, 600), "points": array(POINT, 256, 2), "trim": BOOL},
              ("track", "target", "targetTrack", "beat", "length", "points")),
]}

def tool(name: str, description: str, schema: dict, read_only=False) -> dict:
    return {"name": "pulse_" + name, "description": description, "inputSchema": schema,
            "annotations": {"readOnlyHint": read_only, "destructiveHint": not read_only,
                            "idempotentHint": read_only, "openWorldHint": False}}

TOOLS = [
    tool("status", "Read the OPEN app's revision, title, audio, transport and export status. Optional include_history=true returns dated session checkpoints, currentState and affected objects. Read before every write; user edits change revision.", obj({"include_history": BOOL}), True),
    tool("get_project", "Read the committed live song, all track sounds, note lanes, clips and sections with revision. No project-file replacement.", obj({}), True),
    tool("list_sounds", "List original factory sounds with complete editable Analog/FM/Wavetable/Additive/Resonator patches. Synth overrides can create new sounds on each track.", obj({}), True),
    tool("new_song", "Start a new Song-mode composition in the open app with eight default channels and one pattern. Back up the previous session to History and retain it in Undo.",
         obj({"expected_revision": REVISION, "title": string(200), "bpm": num(30, 300)}, ("expected_revision", "title", "bpm"))),
    tool("edit", "Apply 1-128 atomic operations as ONE project undo. preview:true validates and returns candidateProject without committing. project_commands accepts bounded local text commands with visible one-based target numbers and returns resolved actions in preview. Ordinary structured operation indices remain zero-based. Indices are zero-based. add_track/add_pattern append; extract_notes copies or moves selected indices into a new pattern; merge_patterns layers with conflict handling or sequences phrases; set_notes replaces one lane; set_arrangement replaces all clips. set_audio_fades edits existing audio clip lengths/shapes (0 Linear, 1 Equal power, 2 S-curve) without loading files. ripple_time inserts or deletes time across all lanes (start/length in beats; remove boolean required). Boundary-crossing clips reject; markers/loop/punch follow; fully deleted loop/punch ranges switch off. Use preview:true before deletion. section_clips copies/moves whole clips inside a named section with destination overlap guards. set_track/add_track accept optional auxSends: exactly four objects with target (-1 Off or another existing bus index), level 0-1.5 and preFader boolean. This replaces all slots; omission preserves existing sends. Feedback cycles reject even at level 0. Pre taps are after inserts/pan, before channel volume, and obey Mute; post taps also follow volume. Bus Solo isolates its return branch; source Solo retains all returns. set_track/add_track also accept chokeGroup 0-16 (0 off; buses require 0). Accepted notes stop older group voices with a 5 ms fade; effects tails remain. set_track supports factory preset plus partial custom synth/params/effects. add_automation accepts stable target IDs 0-125 or keys discovered in get_project.automationParameters, with normalized values. Optional trim:true adds a continuous offset layer: value 0.5 is zero, 0/1 subtracts/adds the full normalized range, then clamps. Discrete targets reject Trim. Later absolute clips override earlier layers; later Trim clips offset their underlying result. IDs 0-5 keep their legacy meaning. Inspect descriptor channel/master capabilities: targets 118-120 require targetTrack:-1 (Master processing), 121 requires a channel/bus (mute), targets 122-125 / aux.send1.level through aux.send4.level are channel/bus auxiliary levels (normalized 0-1 maps to gain 0-1.5; the bus destination and tap remain static), and synth targets need a source channel. Numeric point values remain normalized, even when native Value editing accepts actual units. Read updated revision after each call.",
         obj({"expected_revision": REVISION, "operations": array(OPERATIONS, 128, 1), "preview": BOOL}, ("expected_revision", "operations"))),
    tool("transport", "Control actual app playback: play, pause, stop/rewind or seek in beats. Requires current revision and available audio output.",
         obj({"expected_revision": REVISION, "action": {"enum": ["play", "pause", "stop", "seek"]}, "beat": num(0, 4096)}, ("expected_revision", "action"))),
    tool("save_project", "Save the live song with collected assets under workspace/Projects/<name>.pulse. Existing destinations require explicit overwrite=true.",
         obj({"expected_revision": REVISION, "name": NAME, "overwrite": BOOL}, ("expected_revision", "name"))),
    tool("export_audio", "Start background stereo 48 kHz/24-bit WAV Song export with default -1 dBFS sample-peak normalization (normalize=false preserves mix levels), TPDF dither and three-second tail, under workspace/Exports. Poll pulse_status until exporting=false and inspect status; completion is asynchronous.",
         obj({"expected_revision": REVISION, "name": NAME, "overwrite": BOOL, "normalize": BOOL}, ("expected_revision", "name"))),
    tool("undo", "Move through retained project history. Defaults to one Undo; optional direction=redo and steps=1-100 restore multiple edits in one revision. Read pulse_status(include_history=true) first. New-song creation and each atomic batch are undoable; a new edit clears Redo.", obj({"expected_revision": REVISION, "direction": {"enum": ["undo", "redo"]}, "steps": num(1, 100, True)}, ("expected_revision",))),
    tool("view", "Show/maximize Playlist, Piano roll, Mixer, Synth or Channel rack in the live app. Optional track/pattern selects its contents; selecting a different pattern updates revision.",
         obj({"expected_revision": REVISION, "panel": {"enum": ["playlist", "piano_roll", "mixer", "synth", "channel_rack"]}, "track": TRACK, "pattern": PATTERN}, ("expected_revision", "panel"))),
    tool("mix_analysis", "Analyze the current Song/PAT offline at 48 kHz with a two-second tail (maximum ten minutes). start captures a revision; status is read-only and returns mixAnalysis/report/progress; cancel stops the worker; apply_level re-renders and verifies an eligible -1 dBFS Master-gain reduction before one Undo. Live inputs/click/Browser preview excluded; sample peak/RMS and unweighted FFT, not LUFS/true peak. No arbitrary paths. Mutations require current expected_revision; inspect completion/staleness before applying.", {"type": "object", "properties": {"action": {"enum": ["start", "status", "cancel", "apply_level"]}, "expected_revision": REVISION}, "required": ["action"], "oneOf": [obj({"action": {"enum": ["status"]}, "expected_revision": REVISION}, ("action",)), obj({"action": {"enum": ["start", "cancel", "apply_level"]}, "expected_revision": REVISION}, ("action", "expected_revision"))]}),
    tool("snapshot", "Capture a PNG of the live app's workspace. Returns an MCP image. No song changes.", obj({}), True),
]

def validate(value, schema: dict, path="arguments") -> None:
    if "oneOf" in schema:
        matches = 0
        for choice in schema["oneOf"]:
            try:
                validate(value, choice, path)
                matches += 1
            except ValueError:
                pass
        if matches != 1:
            raise ValueError(f"{path}: does not match one supported operation schema")
        return
    if "enum" in schema and value not in schema["enum"]:
        raise ValueError(f"{path}: unsupported value")
    kind = schema.get("type")
    valid_type = {"object": isinstance(value, dict), "array": isinstance(value, list), "string": isinstance(value, str),
                  "boolean": isinstance(value, bool), "integer": type(value) is int,
                  "number": type(value) in (int, float)}
    if kind and not valid_type[kind]:
        raise ValueError(f"{path}: expected {kind}")
    if kind in ("integer", "number"):
        if value < schema.get("minimum", -math.inf) or value > schema.get("maximum", math.inf) or (type(value) is float and not math.isfinite(value)):
            raise ValueError(f"{path}: outside allowed numeric range")
    elif kind == "string":
        if len(value) < schema.get("minLength", 0) or len(value) > schema.get("maxLength", sys.maxsize) or ("pattern" in schema and not re.fullmatch(schema["pattern"], value)):
            raise ValueError(f"{path}: invalid text or name")
    elif kind == "array":
        if len(value) < schema.get("minItems", 0) or len(value) > schema.get("maxItems", sys.maxsize):
            raise ValueError(f"{path}: invalid item count")
        for index, item in enumerate(value):
            validate(item, schema["items"], f"{path}[{index}]")
        if schema.get("uniqueItems") and any(value[i] == value[j] for i in range(len(value)) for j in range(i)):
            raise ValueError(f"{path}: duplicate items are not allowed")
    elif kind == "object":
        if any(key not in value for key in schema.get("required", [])):
            raise ValueError(f"{path}: missing required properties")
        for key, item in value.items():
            if key not in schema["properties"]:
                raise ValueError(f"{path}: unknown property {key}")
            validate(item, schema["properties"][key], f"{path}.{key}")

class Bridge:
    def __init__(self, workspace: Path, timeout=15.0):
        self.root = workspace.resolve() / "data/control"
        self.timeout = timeout

    def call(self, command: str, arguments: dict) -> dict:
        deadline = time.monotonic() + self.timeout
        metadata_file = self.root / "session.json"
        while True:
            try:
                if metadata_file.stat().st_size > 4096:
                    raise ValueError("Invalid control session metadata")
                metadata = json.loads(metadata_file.read_text(encoding="utf-8-sig"))
                break
            except FileNotFoundError as error:
                raise ValueError("Open Pulse Studio 0.31+ and enable OPTIONS > Local MCP control in this workspace.") from error
            except PermissionError as error:
                if time.monotonic() >= deadline:
                    raise ValueError("Local control session metadata stayed busy; no request was submitted.") from error
                time.sleep(.025)
        session, token = metadata.get("session"), metadata.get("token")
        if metadata.get("schema") != 1 or not isinstance(session, str) or not re.fullmatch(r"[0-9a-f]{32}", session) or not isinstance(token, str) or not token:
            raise ValueError("Invalid control session metadata")
        if command == "edit":
            effects = [op.get(key, {}) for op in arguments.get("operations", []) for key in ("effects", "masterEffects")]
            if any(isinstance(fx, dict) and ("delay" in fx or "distortion" in fx or len(fx.get("order", [])) == 8) for fx in effects):
                version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
                if version is None or tuple(map(int, version.groups())) < (0, 80, 0):
                    raise ValueError("Delay, Distortion and eight-slot ordering require the OPEN app to be Pulse Studio 0.81.0+. No request was submitted.")
        if command == "edit" and any(op.get("op") in ("set_track","add_track") and "auxSends" in op for op in arguments.get("operations", [])):
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 93, 0):
                raise ValueError("Auxiliary sends require the OPEN app to be Pulse Studio 0.93.0+. No request was submitted.")
        if command == "edit" and any(op.get("op")=="add_automation" and ((type(op.get("target")) is int and 122 <= op["target"] <= 125) or (isinstance(op.get("target"), str) and re.fullmatch(r"aux\.send[1-4]\.level", op["target"]))) for op in arguments.get("operations", [])):
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 94, 0):
                raise ValueError("Auxiliary level automation requires the OPEN app to be Pulse Studio 0.94.0+. No request was submitted.")
        if command == "edit" and any(op.get("op")=="ripple_time" for op in arguments.get("operations", [])):
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 92, 0):
                raise ValueError("Ripple editing requires the OPEN app to be Pulse Studio 0.92.0+. No request was submitted.")
        if command == "edit" and any(op.get("op") in ("set_track", "add_track") and "chokeGroup" in op for op in arguments.get("operations", [])):
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 91, 0):
                raise ValueError("Choke groups require the OPEN app to be Pulse Studio 0.91.0+. No request was submitted.")
        if command == "edit" and any(op.get("op")=="add_automation" and "trim" in op for op in arguments.get("operations", [])):
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 90, 0):
                raise ValueError("Trim automation requires the OPEN app to be Pulse Studio 0.90.0+. No request was submitted.")
        if command == "edit" and any(op.get("op")=="add_automation" and (isinstance(op.get("target"),str) or (isinstance(op.get("target"), (int,float)) and op["target"]>5)) for op in arguments.get("operations",[])):
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 81, 0):
                raise ValueError("Extended automation requires the OPEN app to be Pulse Studio 0.81.0+. No request was submitted.")
        if command == "edit":
            selector_keys = {"eq.enabled", "compressor.enabled", "reverb.enabled", "chorus.enabled", "flanger.enabled", "phaser.enabled", "delay.enabled", "distortion.enabled", "stereo.enabled", "stereo.mono", "stereo.swap", "stereo.invertLeft", "stereo.invertRight", "dynamics.mode", "distortion.mode", "synth.wave1", "synth.wave2"}
            if any(op.get("op") == "add_automation" and (op.get("target") in selector_keys if isinstance(op.get("target"), str) else type(op.get("target")) in (int, float) and op["target"] >= 101) for op in arguments.get("operations", [])):
                version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
                if version is None or tuple(map(int, version.groups())) < (0, 86, 0):
                    raise ValueError("Selector automation requires the OPEN app to be Pulse Studio 0.86.0+. No request was submitted.")
        if command == "edit":
            new_keys = {"master.delayReturn", "master.delayFeedback", "master.softClip", "channel.mute"}
            if any(op.get("op") == "add_automation" and (op.get("target") in new_keys if isinstance(op.get("target"), str) else type(op.get("target")) in (int, float) and op["target"] >= 118) for op in arguments.get("operations", [])):
                version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
                if version is None or tuple(map(int, version.groups())) < (0, 87, 0):
                    raise ValueError("Master processing and mute automation require the OPEN app to be Pulse Studio 0.87.0+. No request was submitted.")
        if command == "mix_analysis":
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 68, 0):
                raise ValueError("Mix analysis requires the OPEN app to be Pulse Studio 0.68.0+. No request was submitted.")
        if command == "edit" and arguments.get("preview") is True:
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 67, 0):
                raise ValueError("Edit previews require the OPEN app to be Pulse Studio 0.67.0+. No request was submitted; older apps ignore preview and could commit the edit.")
        uses_history = (command == "undo" and ("direction" in arguments or "steps" in arguments)) or (command == "status" and arguments.get("include_history") is True)
        if uses_history:
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 59, 0):
                raise ValueError("Session history fields require the OPEN app to be Pulse Studio 0.59.0+. No request was submitted; older apps would ignore direction/steps. Save and close the older app, then open the new release.")
        if command == "export_audio" and "normalize" in arguments:
            version = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", str(metadata.get("version", "")))
            if version is None or tuple(map(int, version.groups())) < (0, 64, 0):
                raise ValueError("Export normalization choices require the OPEN app to be Pulse Studio 0.64.0+. No request was submitted; older apps ignore this field. Save and close the older app, then open the new release.")
        folder = self.root / session
        request_id = uuid.uuid4().hex
        request = folder / "requests" / f"{request_id}.json"
        response = folder / "responses" / f"{request_id}.json"
        now = int(time.time() * 1000)
        envelope = {"schema": 1, "id": request_id, "session": session, "token": token,
                    "createdAt": now, "expiresAt": now + 18000, "command": command, "arguments": arguments}
        encoded = json.dumps(envelope, ensure_ascii=False, allow_nan=False).encode("utf-8")
        if len(encoded) > MAX_MESSAGE:
            raise ValueError("Control request exceeds 2 MiB; split edits into smaller batches.")
        temporary = request.with_suffix(".tmp")
        try:
            temporary.write_bytes(encoded)
            os.replace(temporary, request)
            while time.monotonic() < deadline:
                try:
                    if response.stat().st_size > 8 * MAX_MESSAGE:
                        raise ValueError("Oversized control response")
                    reply = json.loads(response.read_text(encoding="utf-8-sig"))
                except (PermissionError, FileNotFoundError):
                    # Observe this one request; never resubmit a possibly applied mutation.
                    time.sleep(.025)
                    continue
                try:
                    response.unlink(missing_ok=True)
                except OSError:
                    pass  # Cleanup cannot turn a known result into an ambiguous failure.
                if not isinstance(reply, dict) or reply.get("id") != request_id or reply.get("schema") != 1:
                    raise ValueError("Invalid control response")
                if not reply.get("ok"):
                    raise ValueError(reply.get("error", "Control operation failed"))
                return reply["result"]
            raise ValueError("App did not respond in time. Inspect pulse_status before retrying: a mutation may already have applied.")
        finally:
            for leftover in (temporary, request):
                try:
                    leftover.unlink(missing_ok=True)
                except OSError:
                    pass

def tool_result(bridge: Bridge, name: str, arguments) -> dict:
    entry = next((tool for tool in TOOLS if tool["name"] == name), None)
    if entry is None:
        return {"content": [{"type": "text", "text": "Unknown Pulse Studio tool"}], "isError": True}
    try:
        validate(arguments, entry["inputSchema"])
        value = bridge.call(name.removeprefix("pulse_"), arguments)
        if name == "pulse_snapshot":
            path = Path(value["path"]).resolve()
            if not path.is_relative_to(bridge.root.resolve()) or path.suffix != ".png" or path.stat().st_size > 8 * MAX_MESSAGE:
                raise ValueError("Invalid workspace snapshot path")
            encoded = base64.b64encode(path.read_bytes()).decode("ascii")
            path.unlink()
            return {"content": [{"type": "image", "mimeType": "image/png", "data": encoded}], "isError": False}
        return {"content": [{"type": "text", "text": json.dumps(value, ensure_ascii=False, allow_nan=False)}],
                "structuredContent": value, "isError": False}
    except (ValueError, OSError, KeyError, TypeError) as error:
        return {"content": [{"type": "text", "text": str(error)}], "isError": True}

def serve(bridge: Bridge, source=None, target=None) -> None:
    source = source or sys.stdin.buffer
    target = target or sys.stdout
    initialized = False
    while True:
        line = source.readline(MAX_MESSAGE + 1)
        if not line:
            return
        response = None
        request_id = None
        try:
            if len(line) > MAX_MESSAGE:
                while line and not line.endswith(b"\n"):
                    line = source.readline(MAX_MESSAGE + 1)
                raise ValueError("MCP message exceeds 2 MiB")
            request = json.loads(line, parse_constant=lambda value: (_ for _ in ()).throw(ValueError("Nonfinite JSON value")))
            if not isinstance(request, dict) or request.get("jsonrpc") != "2.0" or not isinstance(request.get("method"), str):
                raise ValueError("Invalid JSON-RPC request")
            request_id = request.get("id")
            if request_id is not None and (type(request_id) not in (int, str)):
                raise ValueError("Invalid JSON-RPC id")
            method, params = request["method"], request.get("params", {})
            if not isinstance(params, dict):
                raise ValueError("Invalid JSON-RPC params")
            if request_id is None:
                continue
            if method == "initialize":
                version = params.get("protocolVersion")
                if version not in ("2024-11-05", "2025-03-26", "2025-06-18", "2025-11-25"):
                    version = "2025-06-18"
                initialized = True
                value = {"protocolVersion": version, "capabilities": {"tools": {"listChanged": False}},
                         "serverInfo": {"name": "pulse-studio-local", "version": VERSION},
                         "instructions": "Control only the explicitly enabled local Pulse Studio workspace. Read revision before each write. Backups and Undo preserve replaced sessions. No network or arbitrary code execution."}
            elif method == "ping":
                value = {}
            elif not initialized:
                response = {"error": {"code": -32002, "message": "Initialize the MCP session first"}}
            elif method == "tools/list":
                value = {"tools": TOOLS}
            elif method == "tools/call":
                value = tool_result(bridge, params.get("name"), params.get("arguments", {}))
            else:
                response = {"error": {"code": -32601, "message": "Method not found"}}
            if response is None:
                response = {"result": value}
        except (json.JSONDecodeError, UnicodeDecodeError):
            response = {"error": {"code": -32700, "message": "Parse error"}}
        except (ValueError, TypeError, KeyError) as error:
            response = {"error": {"code": -32600, "message": str(error)}}
        target.write(json.dumps({"jsonrpc": "2.0", "id": request_id, **response}, ensure_ascii=False, allow_nan=False) + "\n")
        target.flush()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True, type=Path)
    args = parser.parse_args()
    serve(Bridge(args.workspace))
