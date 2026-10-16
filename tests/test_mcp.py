# SPDX-License-Identifier: AGPL-3.0-only
import io
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import pulse_mcp as mcp

class ProtocolChecks(unittest.TestCase):
    def rpc(self, *messages):
        output = io.StringIO()
        bridge = Mock()
        bridge.call.return_value = {"revision": 3, "title": "Test"}
        stream = io.BytesIO(b"".join(json.dumps(m).encode() + b"\n" for m in messages))
        mcp.serve(bridge, stream, output)
        return [json.loads(line) for line in output.getvalue().splitlines()], bridge

    @staticmethod
    def initialize():
        return {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-06-18"}}

    def test_initialize_tools_notifications(self):
        rows, _ = self.rpc(self.initialize(), {"jsonrpc": "2.0", "method": "notifications/initialized"},
                           {"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]["result"]["protocolVersion"], "2025-06-18")
        names = [tool["name"] for tool in rows[1]["result"]["tools"]]
        self.assertEqual(len(names), 11)
        self.assertEqual(len(set(names)), 11)

    def test_uninitialized_tool_rejected(self):
        rows, bridge = self.rpc({"jsonrpc": "2.0", "id": 1, "method": "tools/list"})
        self.assertEqual(rows[0]["error"]["code"], -32002)
        bridge.call.assert_not_called()

    def test_unknown_method(self):
        rows, _ = self.rpc(self.initialize(), {"jsonrpc": "2.0", "id": 2, "method": "arbitrary/execute"})
        self.assertEqual(rows[1]["error"]["code"], -32601)

    def test_valid_call_structured_content(self):
        rows, bridge = self.rpc(self.initialize(), {"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "pulse_status", "arguments": {}}})
        self.assertEqual(rows[1]["result"]["structuredContent"]["revision"], 3)
        bridge.call.assert_called_once_with("status", {})

    def test_missing_revision_never_reaches_app(self):
        result = mcp.tool_result(Mock(), "pulse_new_song", {"title": "Test", "bpm": 76})
        self.assertTrue(result["isError"])

    def test_traversal_never_reaches_app(self):
        bridge = Mock()
        for name in ("../elsewhere", "D:\\target", "/tmp/test", "name.pulse", ""):
            result = mcp.tool_result(bridge, "pulse_save_project", {"expected_revision": 0, "name": name})
            self.assertTrue(result["isError"])
        bridge.call.assert_not_called()

    def test_invalid_types(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_new_song")["inputSchema"]
        for tempo in (True, "76", float("nan"), 10**500, -1, 301):
            with self.assertRaises(ValueError):
                mcp.validate({"expected_revision": 0, "title": "Test", "bpm": tempo}, schema)

    def test_unknown_parameters(self):
        with self.assertRaises(ValueError):
            mcp.validate({"arbitrary": "command"}, mcp.obj({}))

    def test_atomic_edit_schema(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        mcp.validate({"expected_revision": 1, "operations": [{"op": "set_track", "track": 5, "instrument": "wavetable", "synth": {"table": 2, "tableEnv": -.2}, "effects": {"reverb": {"mix": .3}}},
                     {"op": "set_notes", "pattern": 0, "track": 5, "notes": [{"beat": 0, "length": 4, "pitch": 38, "velocity": .5}]}]}, schema)
        for operation in ({"op": "run_shell"}, {"op": "set_notes", "notes": []}, {"op": "set_track", "track": 0, "synth": {"arbitrary": 5}}):
            with self.assertRaises(ValueError):
                mcp.validate({"expected_revision": 1, "operations": [operation]}, schema)

    def test_pattern_colour_range_and_types(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        for kind in ("add_pattern", "set_pattern"):
            for colour in (0, 12, -1, 13, 1.5, True, "3"):
                op = {"op": kind, "colour": colour, **({"name": "Colour"} if kind == "add_pattern" else {"pattern": 0})}
                if type(colour) is int and colour in (0, 12):
                    mcp.validate({"expected_revision": 0, "operations": [op]}, schema)
                else:
                    with self.assertRaises(ValueError):
                        mcp.validate({"expected_revision": 0, "operations": [op]}, schema)

    def test_selection_extraction_schema(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        mcp.validate({"expected_revision": 0, "operations": [{"op": "extract_notes", "pattern": 0, "track": 7, "indices": [0, 511], "move": True, "alignStart": False}]}, schema)
        for indices in ([], [0, 0], [True], [-1], [512], [1.5], "0"):
            with self.assertRaises(ValueError):
                mcp.validate({"expected_revision": 0, "operations": [{"op": "extract_notes", "pattern": 0, "track": 7, "indices": indices}]}, schema)

    def test_pattern_merge_schema(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        for policy in ("keep_all", "skip_exact", "prefer_first", "prefer_second"):
            mcp.validate({"expected_revision": 0, "operations": [{"op": "merge_patterns", "first": 0, "second": 1, "policy": policy}]}, schema)
        for bad in ({"mode": "bad"}, {"policy": "bad"}, {"mode": True}, {"unknown": 0}):
            with self.assertRaises(ValueError):
                mcp.validate({"expected_revision": 0, "operations": [{"op": "merge_patterns", "first": 0, "second": 1, **bad}]}, schema)

    def test_section_clip_schema(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        mcp.validate({"expected_revision": 0, "operations": [{"op": "section_clips", "section": "verse", "destination": 16.25, "move": True, "allowOverlap": False}]}, schema)
        for bad in ({"destination": -1}, {"destination": 4097}, {"section": ""}, {"move": 1}, {"allowOverlap": "yes"}, {"replace": True}):
            with self.assertRaises(ValueError):
                mcp.validate({"expected_revision": 0, "operations": [{"op": "section_clips", "section": "verse", "destination": 16, **bad}]}, schema)

    def test_parse_error_and_recovery(self):
        output = io.StringIO()
        mcp.serve(Mock(), io.BytesIO(b"not-json\n" + json.dumps(self.initialize()).encode() + b"\n"), output)
        rows = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual(rows[0]["error"]["code"], -32700)
        self.assertIn("result", rows[1])

    def test_clip_lengths_match_native_project_limit(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        for kind in ("pattern", "automation"):
            for length in (600, 600.001):
                operation = ({"op": "set_arrangement", "clips": [{"track": 0, "pattern": 0, "beat": 0, "length": length}]}
                             if kind == "pattern" else {"op": "add_automation", "track": 0, "target": 5, "targetTrack": -1,
                                                       "beat": 0, "length": length, "points": [{"beat": 0, "value": 1}, {"beat": length, "value": 0}]})
                arguments = {"expected_revision": 0, "operations": [operation]}
                if length == 600:
                    mcp.validate(arguments, schema)
                else:
                    with self.assertRaises(ValueError):
                        mcp.validate(arguments, schema)

    def test_documented_skill_example_uses_supported_operations(self):
        import re
        reference = (ROOT / "skills/pulse-studio-mcp/references/api.md").read_text(encoding="utf-8")
        examples = [json.loads(block) for block in re.findall(r"```json\n(.*?)\n```", reference, re.S)]
        operations = next(value for value in examples if isinstance(value, list))
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        mcp.validate({"expected_revision": 0, "operations": operations}, schema)

    def test_error_is_tool_error(self):
        bridge = Mock()
        bridge.call.side_effect = ValueError("Project revision changed")
        result = mcp.tool_result(bridge, "pulse_status", {})
        self.assertTrue(result["isError"])
        self.assertIn("revision changed", result["content"][0]["text"])

    def test_live_file_transport_and_cleanup(self):
        folder = ROOT / "test-output"
        folder.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="mcp-protocol-", dir=folder) as temporary:
            workspace = Path(temporary)
            self.assertTrue(workspace.resolve().is_relative_to(folder.resolve()))
            root = workspace / "data/control"
            session = "a" * 32
            (root / session / "requests").mkdir(parents=True)
            (root / session / "responses").mkdir()
            (root / "session.json").write_text(json.dumps({"schema": 1, "session": session, "token": "test-only"}))
            captured = []
            def app():
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline:
                    files = list((root / session / "requests").glob("*.json"))
                    if files:
                        file = files[0]
                        request = json.loads(file.read_text())
                        captured.append(request)
                        file.unlink()
                        target = root / session / "responses" / file.name
                        target.write_text(json.dumps({"schema": 1, "id": request["id"], "ok": True, "result": {"revision": 9}}))
                        return
                    time.sleep(.005)
            thread = threading.Thread(target=app)
            thread.start()
            result = mcp.Bridge(workspace, timeout=2).call("status", {})
            thread.join(timeout=3)
            self.assertEqual(result["revision"], 9)
            self.assertEqual(captured[0]["token"], "test-only")
            self.assertEqual(captured[0]["session"], session)
            self.assertEqual(list((root / session / "requests").glob("*")), [])
            self.assertEqual(list((root / session / "responses").glob("*")), [])

if __name__ == "__main__":
    unittest.main()
