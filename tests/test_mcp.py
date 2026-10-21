# SPDX-License-Identifier: AGPL-3.0-only
import io
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import pulse_mcp as mcp

class ProtocolChecks(unittest.TestCase):
    def test_mix_analysis_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_mix_analysis')['inputSchema']
        mcp.validate({'action':'status'},schema)
        for action in ('start','cancel','apply_level'):
            mcp.validate({'action':action,'expected_revision':4},schema)
            with self.assertRaises(ValueError):mcp.validate({'action':action},schema)
        for args in ({'action':'bad'},{'action':'status','path':'x'},{'action':'start','expected_revision':True}):
            with self.assertRaises(ValueError):mcp.validate(args,schema)

    def test_mix_analysis_rejects_old_app(self):
        with tempfile.TemporaryDirectory(prefix='mcp-mix-',dir=ROOT/'test-output') as folder:
            work=Path(folder);control=work/'data/control';requests=control/('a'*32)/'requests';requests.mkdir(parents=True)
            (control/'session.json').write_text(json.dumps({'schema':1,'session':'a'*32,'token':'fixture','version':'0.67.0'}))
            with self.assertRaisesRegex(ValueError,'No request was submitted'):mcp.Bridge(work).call('mix_analysis',{'action':'start','expected_revision':1})
            self.assertFalse(list(requests.iterdir()))

    def test_project_commands_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        base={'expected_revision':1,'operations':[{'op':'project_commands','text':'set tempo to 120 bpm\nmute track 2'}]}
        for preview in (True,False):mcp.validate({**base,'preview':preview},schema)
        for preview in (1,'true',None):
            with self.assertRaises(ValueError):mcp.validate({**base,'preview':preview},schema)
        for text in ('x'*4097,True,None,42):
            with self.assertRaises(ValueError):mcp.validate({**base,'operations':[{'op':'project_commands','text':text}]},schema)
        with self.assertRaises(ValueError):mcp.validate({**base,'operations':[{'op':'project_commands','text':'mute track 2','shell':True}]},schema)

    def test_preview_rejects_older_app_without_submitting(self):
        with tempfile.TemporaryDirectory(prefix='mcp-preview-',dir=ROOT/'test-output') as folder:
            work=Path(folder);control=work/'data/control';requests=control/('a'*32)/'requests';requests.mkdir(parents=True)
            for version in ('0.66.0','0.31.0','unknown'):
                (control/'session.json').write_text(json.dumps({'schema':1,'session':'a'*32,'token':'fixture','version':version}))
                with self.assertRaisesRegex(ValueError,'No request was submitted'):mcp.Bridge(work).call('edit',{'expected_revision':1,'operations':[{'op':'set_song','bpm':140}],'preview':True})
                self.assertFalse(list(requests.iterdir()))

    def test_mute_solo_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'mute':True,'solo':False}]},schema)
        for value in (1,'false',None):
            with self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'mute':value}]},schema)

    def test_physical_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        model={'mode':0,'decay':4,'damping':.3,'brightness':.7,'position':.25,'body':.4,'stiffness':.1}
        def validate(value):mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'instrument':'physical','synth':{'physical':value}}]},schema)
        validate(model)
        for invalid in ({'mode':True},{'mode':2},{'mode':.5},{'decay':0},{'decay':21},{'brightness':0},{'position':1},{'stiffness':-1},{'body':float('nan')},{'extra':0}):
            with self.subTest(invalid=invalid),self.assertRaises(ValueError):validate({**model,**invalid})
        for key in model:
            incomplete=dict(model);del incomplete[key]
            with self.assertRaises(ValueError):validate(incomplete)

    def test_microtuning_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        tuning={'enabled':True,'name':'Custom quartertones','referenceHz':432,'cents':[0]*128}
        mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'tuning':tuning}]},schema)
        for invalid in ({'cents':[0]*127},{'cents':[0]*129},{'cents':[True]+[0]*127},{'cents':[float('nan')]+[0]*127},{'cents':[24001]+[0]*127},{'referenceHz':1000},{'enabled':1},{'extra':0}):
            with self.subTest(invalid=invalid),self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'tuning':{**tuning,**invalid}}]},schema)
        incomplete=dict(tuning);del incomplete['cents']
        with self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'tuning':incomplete}]},schema)

    def test_export_normalization(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_export_audio')['inputSchema']
        for value in (True,False):mcp.validate({'expected_revision':1,'name':'Mix','normalize':value},schema)
        for value in (1,'false',None):
            with self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'name':'Mix','normalize':value},schema)
        with tempfile.TemporaryDirectory(prefix='mcp-normalize-',dir=ROOT/'test-output') as folder:
            work=Path(folder);control=work/'data/control';requests=control/('a'*32)/'requests';requests.mkdir(parents=True)
            (control/'session.json').write_text(json.dumps({'schema':1,'session':'a'*32,'token':'fixture','version':'0.63.0'}))
            with self.assertRaisesRegex(ValueError,'No request was submitted'):mcp.Bridge(work).call('export_audio',{'expected_revision':1,'name':'Mix','normalize':False})
            self.assertFalse(list(requests.iterdir()))

    def test_extended_dynamics_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        for mode in ('compressor','limiter','gate','expander'):
            mcp.validate({'expected_revision':1,'operations':[{'op':'set_song','masterEffects':{'compressor':{'mode':mode,'range':72,'hold':125,'hysteresis':6,'knee':12}}}]},schema)
        for invalid in ({'mode':'invalid'},{'mode':1},{'range':97},{'hold':1001},{'hysteresis':-1},{'knee':25}):
            with self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'operations':[{'op':'set_track','track':7,'effects':{'compressor':invalid}}]},schema)

    def test_note_condition_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        base={'beat':0,'length':.25,'pitch':60,'velocity':.8}
        for fields in ({},{'conditionEvery':4,'conditionStep':4},{'conditionEvery':16,'conditionStep':16}):
            mcp.validate({'expected_revision':1,'operations':[{'op':'set_notes','pattern':0,'track':7,'notes':[{**base,**fields}]}]},schema)
        for field,value in [('conditionEvery',0),('conditionEvery',17),('conditionEvery',True),('conditionStep',1.5),('conditionStep','2')]:
            with self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'operations':[{'op':'set_notes','pattern':0,'track':7,'notes':[{**base,field:value}]}]},schema)

    def test_master_insert_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        effects={'order':[4,0,1,3,5,2],'compressor':{'threshold':-24,'ratio':3,'attack':5,'release':160,'makeup':1},'flanger':{'centre':3,'feedback':-.85},'phaser':{'centre':4000,'rate':.02},'stereo':{'mono':True}}
        mcp.validate({'expected_revision':1,'operations':[{'op':'set_song','masterEffects':effects},{'op':'set_track','track':5,'effects':effects}]},schema)
        for invalid in ({'output':0},{'compressor':{'sidechain':0}},{'order':[0,0,2,3,4,5]},{'order':[0,1]},{'flanger':{'rate':6}},{'phaser':{'centre':4001}},{'compressor':{'threshold':-61}},{'reverb':{'enabled':1}}):
            with self.assertRaises(ValueError):mcp.validate({'expected_revision':1,'operations':[{'op':'set_song','masterEffects':invalid}]},schema)

    def test_master_processing_schema(self):
        schema=next(t for t in mcp.TOOLS if t['name']=='pulse_edit')['inputSchema']
        mcp.validate({'expected_revision':1,'operations':[{'op':'set_song','masterSoftClip':False,'delayReturn':0,'delayFeedback':.8}]},schema)
        for field,value in [('masterSoftClip',1),('masterSoftClip','false'),('delayReturn',-.1),('delayReturn',1.01),('delayReturn',True)]:
            with self.assertRaises(ValueError):
                mcp.validate({'expected_revision':1,'operations':[{'op':'set_song',field:value}]},schema)

    def test_history_schema(self):
        status=next(t for t in mcp.TOOLS if t['name']=='pulse_status')['inputSchema']
        undo=next(t for t in mcp.TOOLS if t['name']=='pulse_undo')['inputSchema']
        for args in ({},{'include_history':True},{'include_history':False}):
            mcp.validate(args,status)
        for args in ({'expected_revision':1},{'expected_revision':2,'direction':'redo','steps':100}):
            mcp.validate(args,undo)
        for args in ({'include_history':1},{'include_history':'true'}):
            with self.assertRaises(ValueError): mcp.validate(args,status)
        for extra in ({'steps':True},{'steps':0},{'steps':101},{'steps':1.5},{'direction':'rewind'}):
            with self.assertRaises(ValueError): mcp.validate({'expected_revision':1,**extra},undo)

    def test_history_version_guard_submits_no_request(self):
        with tempfile.TemporaryDirectory(prefix='mcp-history-version-',dir=ROOT/'test-output') as folder:
            work=Path(folder)
            control=work/'data/control'
            requests=control/('a'*32)/'requests'
            requests.mkdir(parents=True)
            metadata={'schema':1,'session':'a'*32,'token':'fixture'}
            for version in ('0.58.0','0.31.0','invalid',None):
                metadata['version']=version
                (control/'session.json').write_text(json.dumps(metadata))
                for command,args in (('undo',{'expected_revision':1,'direction':'redo'}),('undo',{'expected_revision':1,'steps':2}),('status',{'include_history':True})):
                    with self.assertRaisesRegex(ValueError,'No request was submitted'):
                        mcp.Bridge(work).call(command,args)
                    self.assertEqual(list(requests.iterdir()),[])

    def test_history_version_guard_keeps_old_undo_and_new_navigation(self):
        for version,args in (('0.58.0',{'expected_revision':1}),('0.59.0',{'expected_revision':1,'direction':'redo','steps':2}),('0.60.0',{'expected_revision':1,'steps':3})):
            with self.subTest(version=version,args=args),tempfile.TemporaryDirectory(prefix='mcp-history-call-',dir=ROOT/'test-output') as folder:
                work=Path(folder);control=work/'data/control';session=control/('b'*32)
                requests=session/'requests';responses=session/'responses'
                requests.mkdir(parents=True);responses.mkdir()
                (control/'session.json').write_text(json.dumps({'schema':1,'session':'b'*32,'token':'fixture','version':version}))
                captured=[]
                def app():
                    deadline=time.monotonic()+2
                    while time.monotonic()<deadline:
                        files=list(requests.glob('*.json'))
                        if files:
                            captured.append(json.loads(files[0].read_text()))
                            target=responses/files[0].name
                            staging=target.with_suffix('.tmp')
                            staging.write_text(json.dumps({'schema':1,'id':files[0].stem,'ok':True,'result':{'revision':2}}))
                            staging.replace(target)
                            return
                        time.sleep(.005)
                thread=threading.Thread(target=app);thread.start()
                result=mcp.Bridge(work,timeout=2).call('undo',args)
                thread.join(timeout=3)
                self.assertFalse(thread.is_alive());self.assertEqual(result,{'revision':2});self.assertEqual(len(captured),1);self.assertEqual(captured[0]['arguments'],args)

    def test_automation_shapes_schema(self):
        for curve in range(4):
            mcp.validate({"op":"set_automation_points","clip":1,"points":[
                {"beat":0,"value":.2,"curve":curve,"tension":-.4},{"beat":4,"value":.8}]}, mcp.OPERATIONS)
        for field, value in (("curve", True), ("curve", 4), ("curve", .5), ("tension", 1.01), ("tension", "0")):
            point={"beat":0,"value":.2,field:value}
            with self.subTest(field=field,value=value), self.assertRaises(ValueError):
                mcp.validate({"op":"set_automation_points","clip":1,"points":[point,{"beat":4,"value":.8}]}, mcp.OPERATIONS)

    def test_automation_generator_schema(self):
        for wave in ("sine","triangle","square","steps","random_hold","random_smooth"):
            mcp.validate({"op":"generate_automation","clip":0,"wave":wave,"period":4,"seed":4294967295,
                          "steps":[0,.5,1],"resolution":16,"phase":.25,"low":.1,"high":.9}, mcp.OPERATIONS)
        for field,value in (("clip",True),("wave","noise"),("period",0),("seed",4294967296),("seed",True),("phase",1.1),("resolution",65),("steps",[0,True])):
            operation={"op":"generate_automation","clip":0,"wave":"sine","period":4,field:value}
            with self.subTest(field=field,value=value), self.assertRaises(ValueError):
                mcp.validate(operation,mcp.OPERATIONS)

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
        self.assertEqual(len(names), 12)
        self.assertEqual(len(set(names)), 12)

    def test_sampler_tuning_schema(self):
        schema = mcp.PARAMS
        mcp.validate({"sampleRoot": 57, "sampleTune": -23.5}, schema)
        for params in ({"sampleRoot": 128}, {"sampleRoot": 60.5}, {"sampleRoot": True}, {"sampleTune": 101}, {"sampleTune": "0"}):
            with self.assertRaises(ValueError):
                mcp.validate(params, schema)

    def test_audio_fade_operation_schema(self):
        schema = mcp.OPERATIONS
        for curve in range(3):
            mcp.validate({"op": "set_audio_fades", "clip": 3, "fadeIn": .5, "fadeOut": 1,
                          "fadeInCurve": curve, "fadeOutCurve": curve}, schema)
        for operation in (
            {"op": "set_audio_fades", "clip": -1, "fadeIn": 1},
            {"op": "set_audio_fades", "clip": True, "fadeOut": 1},
            {"op": "set_audio_fades", "clip": 0, "fadeIn": -1},
            {"op": "set_audio_fades", "clip": 0, "fadeOut": 601},
            {"op": "set_audio_fades", "clip": 0, "fadeInCurve": True},
            {"op": "set_audio_fades", "clip": 0, "fadeOutCurve": 3},
            {"op": "set_audio_fades", "clip": 0, "fadeOutCurve": 1.5},
            {"op": "set_audio_fades", "clip": 0, "audioPath": "arbitrary.wav"},
        ):
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                mcp.validate(operation, schema)

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

    def test_additive_harmonic_schema(self):
        schema = next(t for t in mcp.TOOLS if t["name"] == "pulse_edit")["inputSchema"]
        def payload(levels):
            return {"expected_revision": 1, "operations": [{"op": "set_track", "track": 7, "instrument": "additive", "synth": {"harmonics": levels}}]}
        mcp.validate(payload([1] + [0] * 15), schema)
        for levels in ([], [0] * 15, [0] * 17, [True] + [0] * 15, ["0.5"] + [0] * 15, [-.01] + [0] * 15, [1.01] + [0] * 15, [float("nan")] + [0] * 15):
            with self.assertRaises(ValueError):
                mcp.validate(payload(levels), schema)

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

    def test_response_sharing_locks_never_resubmit_mutations(self):
        with tempfile.TemporaryDirectory(prefix="mcp-sharing-", dir=ROOT / "test-output") as temporary:
            workspace = Path(temporary)
            root = workspace / "data/control"
            session = "b" * 32
            requests = root / session / "requests"
            responses = root / session / "responses"
            requests.mkdir(parents=True)
            responses.mkdir()
            metadata = root / "session.json"
            metadata.write_text(json.dumps({"schema": 1, "session": session, "token": "test-only"}))
            captured = []
            errors = []
            def app():
                try:
                    deadline = time.monotonic() + 2
                    while time.monotonic() < deadline:
                        for file in requests.glob("*.json"):
                            try:
                                request = json.loads(file.read_text())
                                file.unlink()
                            except PermissionError:
                                continue
                            captured.append(request)
                            target = responses / file.name
                            staging = target.with_suffix(".tmp")
                            staging.write_text(json.dumps({"schema": 1, "id": request["id"], "ok": True, "result": {"revision": 9}}))
                            staging.replace(target)
                            return
                        time.sleep(.005)
                except Exception as error:
                    errors.append(error)
            original_read, original_unlink = Path.read_text, Path.unlink
            locked = {"metadata": 0, "response": 0, "cleanup": 0}
            def read(file, *args, **kwargs):
                kind = "metadata" if file == metadata else "response" if file.parent == responses else None
                if kind and locked[kind] < 2:
                    locked[kind] += 1
                    raise PermissionError("simulated Windows sharing lock")
                return original_read(file, *args, **kwargs)
            def unlink(file, *args, **kwargs):
                if file.parent == responses:
                    locked["cleanup"] += 1
                    raise PermissionError("simulated cleanup sharing lock")
                return original_unlink(file, *args, **kwargs)
            thread = threading.Thread(target=app)
            with patch.object(Path, "read_text", read), patch.object(Path, "unlink", unlink):
                thread.start()
                result = mcp.Bridge(workspace, timeout=2).call("edit", {"expected_revision": 8, "operations": [{"op": "set_song", "title": "One mutation"}]})
                thread.join(timeout=3)
            self.assertFalse(thread.is_alive())
            self.assertEqual(errors, [])
            self.assertEqual(result, {"revision": 9})
            self.assertEqual(len(captured), 1)
            self.assertEqual(captured[0]["command"], "edit")
            self.assertEqual(locked, {"metadata": 2, "response": 2, "cleanup": 1})
            self.assertEqual(list(requests.glob("*.json")), [])
            self.assertEqual([file.stem for file in responses.glob("*.json")], [captured[0]["id"]])

    def test_busy_metadata_submits_no_request(self):
        with tempfile.TemporaryDirectory(prefix="mcp-busy-metadata-", dir=ROOT / "test-output") as temporary:
            workspace = Path(temporary)
            metadata = workspace / "data/control/session.json"
            metadata.parent.mkdir(parents=True)
            metadata.write_text("{}")
            with patch.object(Path, "read_text", side_effect=PermissionError("locked")):
                with self.assertRaisesRegex(ValueError, "no request was submitted"):
                    mcp.Bridge(workspace, timeout=.03).call("edit", {})
            self.assertEqual(list(workspace.rglob("*.tmp")), [])
            self.assertEqual(list(workspace.rglob("requests/*.json")), [])

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
                        try:
                            request = json.loads(file.read_text())
                            file.unlink()
                        except PermissionError:
                            # Windows may briefly hold a newly renamed request.
                            time.sleep(.005)
                            continue
                        captured.append(request)
                        target = root / session / "responses" / file.name
                        staging = target.with_suffix(".tmp")
                        staging.write_text(json.dumps({"schema": 1, "id": request["id"], "ok": True, "result": {"revision": 9}}))
                        staging.replace(target)
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
