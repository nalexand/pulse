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

VERSION = "0.37.0"
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
    **{k: num(0, 3, True) for k in ("wave1", "wave2")}, "coarse": num(-24, 24, True),
    "table": num(0, 7, True), "tablePosition": num(0, 1), "tableEnv": num(-1, 1),
    "tableDecay": num(.01, 8), "tableLfo": num(0, 1), "mix": num(0, 1), "detune": num(-50, 50),
    "pulse": num(.05, .95), "sub": num(0, 1), "noise": num(0, .5), "decay": num(.01, 4),
    "sustain": num(0, 1), "cutoff": num(40, 20000), "resonance": num(0, 1),
    "filterEnv": num(-6, 6), "filterDecay": num(.01, 4), "fmRatio": num(.25, 16),
    "fmIndex": num(0, 12), "fmDecay": num(.01, 4), "lfoRate": num(.1, 20),
    "lfoPitch": num(0, 100), "lfoFilter": num(0, 4),
})
PARAMS = obj({"volume": num(0, 1.5), "pan": num(-1, 1), "cutoff": num(60, 20000),
              "drive": num(0, 1), "send": num(0, .8), "attack": num(.001, 2), "release": num(.02, 3)})
EFFECTS = obj({
    "eq": obj({"enabled": BOOL, **{k: num(-18, 18) for k in ("low", "mid", "high")}}),
    "reverb": obj({"enabled": BOOL, **{k: num(0, 1) for k in ("mix", "room", "damping")}}),
    "chorus": obj({"enabled": BOOL, "mix": num(0, 1), "rate": num(.05, 8), "depth": num(0, 1),
                   "delay": num(5, 35), "feedback": num(-.8, .8), "width": num(0, 1)}),
    "stereo": obj({**{k: BOOL for k in ("enabled", "mono", "swap", "invertLeft", "invertRight")},
                   "width": num(0, 2), "trim": num(-24, 6)}),
})
NOTE = obj({"beat": num(0, 64), "length": num(.01, 64), "pitch": num(0, 127, True),
            "velocity": num(0, 1), "probability": num(0, 1), "ratchets": num(1, 8, True)},
           ("beat", "length", "pitch", "velocity"))
CLIP = obj({"track": TRACK, "pattern": PATTERN, "beat": num(0, 4096), "length": num(.0625, 600)},
           ("track", "pattern", "beat", "length"))
MARKER = obj({"id": string(), "name": string(), "beat": num(0, 4096), "endBeat": num(-1, 4096),
              "colour": num(0, 7, True)}, ("id", "name", "beat", "colour"))
POINT = obj({"beat": num(0, 4096), "value": num(0, 1), "hold": BOOL}, ("beat", "value"))

def operation(kind: str, props: dict, required=()) -> dict:
    return obj({"op": {"enum": [kind]}, **props}, ("op", *required))

track_properties = {"name": string(), "instrument": {"enum": ["kick", "snare", "hat", "clap", "open_hat", "bass", "pad", "lead", "analog", "fm", "wavetable"]},
                    "preset": string(), "synth": SYNTH, "params": PARAMS, "effects": EFFECTS, "output": num(-1, 15, True)}
OPERATIONS = {"oneOf": [
    operation("set_song", {"title": string(200), "bpm": num(30, 300), "swing": num(0, .6), "master": num(0, 1),
                           "delayFeedback": num(0, .8), "songMode": BOOL, "loop": BOOL, "metronome": BOOL,
                           "notes": {"type": "string", "maxLength": 16000}}),
    operation("add_track", {**track_properties, "bus": BOOL}, ("name",)),
    operation("set_track", {"track": TRACK, **track_properties}, ("track",)),
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
    operation("set_markers", {"markers": array(MARKER, 128)}, ("markers",)),
    operation("section_clips", {"section": string(80), "destination": num(0, 4096), "move": BOOL, "allowOverlap": BOOL}, ("section", "destination")),
    operation("add_automation", {"track": TRACK, "target": num(0, 5, True), "targetTrack": num(-1, 15, True),
                                 "beat": num(0, 4096), "length": num(.0625, 600), "points": array(POINT, 256, 2)},
              ("track", "target", "targetTrack", "beat", "length", "points")),
]}

def tool(name: str, description: str, schema: dict, read_only=False) -> dict:
    return {"name": "pulse_" + name, "description": description, "inputSchema": schema,
            "annotations": {"readOnlyHint": read_only, "destructiveHint": not read_only,
                            "idempotentHint": read_only, "openWorldHint": False}}

TOOLS = [
    tool("status", "Read the OPEN app's revision, title, audio, transport and export status. Read before every write; user edits change revision.", obj({}), True),
    tool("get_project", "Read the committed live song, all track sounds, note lanes, clips and sections with revision. No project-file replacement.", obj({}), True),
    tool("list_sounds", "List original factory sounds with complete editable Analog/FM/Wavetable patches. Synth overrides can create new sounds on each track.", obj({}), True),
    tool("new_song", "Start a new Song-mode composition in the open app with eight default channels and one pattern. Back up the previous session to History and retain it in Undo.",
         obj({"expected_revision": REVISION, "title": string(200), "bpm": num(30, 300)}, ("expected_revision", "title", "bpm"))),
    tool("edit", "Apply 1-128 atomic operations as ONE project undo. Indices are zero-based. add_track/add_pattern append; extract_notes copies or moves selected indices into a new pattern; merge_patterns layers with conflict handling or sequences phrases; set_notes replaces one lane; set_arrangement replaces all clips. section_clips copies/moves whole clips inside a named section with destination overlap guards. set_track supports factory preset plus partial custom synth/params/effects. add_automation targets 0 volume, 1 pan, 2 cutoff, 3 drive, 4 send, 5 master, with normalized values. Read updated revision after each call.",
         obj({"expected_revision": REVISION, "operations": array(OPERATIONS, 128, 1)}, ("expected_revision", "operations"))),
    tool("transport", "Control actual app playback: play, pause, stop/rewind or seek in beats. Requires current revision and available audio output.",
         obj({"expected_revision": REVISION, "action": {"enum": ["play", "pause", "stop", "seek"]}, "beat": num(0, 4096)}, ("expected_revision", "action"))),
    tool("save_project", "Save the live song with collected assets under workspace/Projects/<name>.pulse. Existing destinations require explicit overwrite=true.",
         obj({"expected_revision": REVISION, "name": NAME, "overwrite": BOOL}, ("expected_revision", "name"))),
    tool("export_audio", "Start background stereo 48 kHz/24-bit WAV Song export with -1 dBFS sample-peak normalization, TPDF dither and three-second tail, under workspace/Exports. Poll pulse_status until exporting=false and inspect status; completion is asynchronous.",
         obj({"expected_revision": REVISION, "name": NAME, "overwrite": BOOL}, ("expected_revision", "name"))),
    tool("undo", "Undo the last project edit in the open app. New-song creation and each atomic batch are undoable.", obj({"expected_revision": REVISION}, ("expected_revision",))),
    tool("view", "Show/maximize Playlist, Piano roll, Mixer, Synth or Channel rack in the live app. Optional track/pattern selects its contents; selecting a different pattern updates revision.",
         obj({"expected_revision": REVISION, "panel": {"enum": ["playlist", "piano_roll", "mixer", "synth", "channel_rack"]}, "track": TRACK, "pattern": PATTERN}, ("expected_revision", "panel"))),
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
        metadata_file = self.root / "session.json"
        try:
            if metadata_file.stat().st_size > 4096:
                raise ValueError("Invalid control session metadata")
            metadata = json.loads(metadata_file.read_text(encoding="utf-8-sig"))
        except FileNotFoundError as error:
            raise ValueError("Open Pulse Studio 0.31+ and enable OPTIONS > Local MCP control in this workspace.") from error
        session, token = metadata.get("session"), metadata.get("token")
        if metadata.get("schema") != 1 or not isinstance(session, str) or not re.fullmatch(r"[0-9a-f]{32}", session) or not isinstance(token, str) or not token:
            raise ValueError("Invalid control session metadata")
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
            deadline = time.monotonic() + self.timeout
            while time.monotonic() < deadline:
                if response.exists():
                    if response.stat().st_size > 8 * MAX_MESSAGE:
                        raise ValueError("Oversized control response")
                    reply = json.loads(response.read_text(encoding="utf-8-sig"))
                    response.unlink()
                    if reply.get("id") != request_id or reply.get("schema") != 1:
                        raise ValueError("Invalid control response")
                    if not reply.get("ok"):
                        raise ValueError(reply.get("error", "Control operation failed"))
                    return reply["result"]
                time.sleep(.025)
            raise ValueError("App did not respond in time. Inspect pulse_status before retrying: a mutation may already have applied.")
        finally:
            temporary.unlink(missing_ok=True)
            request.unlink(missing_ok=True)

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
