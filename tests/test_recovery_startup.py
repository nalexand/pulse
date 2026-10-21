# SPDX-License-Identifier: AGPL-3.0-only
"""Isolated desktop startup and update handoff against real recovery files."""
from contextlib import contextmanager
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r"project\(PulseStudio VERSION ([\d.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
APP = ROOT / "build-native/PulseStudio_artefacts/Release" / f"Pulse Studio {VERSION}.exe"
PROBE = ROOT / "build-native/PulseTests_artefacts/Release/PulseTests.exe"
sys.path.insert(0, str(ROOT / "tools"))
from mcp_client import PulseClient


@unittest.skipUnless(APP.is_file() and PROBE.is_file(), "build native applications first")
class RecoveryStartupChecks(unittest.TestCase):
    @contextmanager
    def session(self, primary=None, backup=None, last_saved=False, retention=None, audio=False):
        with tempfile.TemporaryDirectory(prefix="recovery-startup-", dir=ROOT / "test-output") as folder:
            work = Path(folder)
            self.assertTrue(work.resolve().is_relative_to((ROOT / "test-output").resolve()))
            subprocess.run([str(PROBE), "--persistence-probe", folder, "seed"], check=True, timeout=10)
            seed = (work / "song.pulse").read_bytes()
            data = work / "data"
            data.mkdir()
            (data / "mcp-settings.json").write_text(json.dumps({"schema": 1, "enabled": True}), encoding="utf-8")
            if primary is not None:
                (data / "recovery-v0.29.pulse").write_bytes(seed if primary == "valid" else primary)
            if backup is not None:
                (data / "recovery-v0.29.pulse.bak").write_bytes(seed if backup == "valid" else backup)
            if last_saved:
                saved = json.loads(seed)
                saved["title"] = "Last saved project"
                (work / "saved.pulse").write_text(json.dumps(saved), encoding="utf-8")
                (data / "last-project.json").write_text(json.dumps({"schema": 1, "path": str(work / "saved.pulse")}), encoding="utf-8")
            if audio:
                import struct
                import wave
                source = work / "original-input.wav"
                with wave.open(str(source), "wb") as writer:
                    writer.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
                    writer.writeframes(struct.pack("<h", 4000) * 2 * 96000)
                saved = json.loads((work / "saved.pulse").read_text())
                saved.update(songMode=True, loop=False, bpm=120)
                saved["clips"] = [{"track": 0, "pattern": 0, "beat": 0, "length": 2,
                                   "audioPath": str(source), "sourceOffset": 0, "gain": .5,
                                   "fadeIn": .25, "fadeOut": .25, "fadeInCurve": 0, "fadeOutCurve": 0,
                                   "automationTarget": -1, "automationTrack": -1, "points": []}]
                (work / "saved.pulse").write_text(json.dumps(saved), encoding="utf-8")
            if retention is not None:
                (data / "recovery-retention.json").write_text(json.dumps(retention), encoding="utf-8")
            command = [str(APP), "--workspace", folder]
            proc = subprocess.Popen(command + ["--resume", "--control-session"])
            try:
                deadline = time.monotonic() + 15
                while not (data / "control/session.json").is_file() and time.monotonic() < deadline:
                    self.assertIsNone(proc.poll(), "native app exited before startup")
                    time.sleep(.02)
                self.assertTrue((data / "control/session.json").is_file(), "native startup deadline")
                with PulseClient(work) as client:
                    yield work, proc, client, command
            finally:
                if proc.poll() is None:
                    subprocess.run(command + ["--request-update"], timeout=10, check=True)
                    try:
                        proc.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        # Only this owned, disposable fixture may be terminated.
                        proc.kill()
                        proc.wait(timeout=5)

    def test_damaged_latest_uses_previous_snapshot(self):
        with self.session(primary=b"{damaged", backup="valid") as (work, proc, client, command):
            state = client.call("pulse_get_project")
            self.assertEqual(state["project"]["title"], "Last committed")
            self.assertEqual(state["project"]["patterns"][0]["lanes"][7][0]["pitch"], 60)
            self.assertTrue(state["dirty"])
            self.assertEqual(state["currentFile"], "")
            self.assertTrue((work / "data/recovery-v0.29.pulse.bak").is_file())
            self.assertEqual(json.loads((work / "data/recovery-v0.29.pulse").read_text())["title"], "Last committed")
            self.assertTrue(any(p.read_bytes() == b"{damaged" for p in (work / "data").glob("recovery-v0.29.pulse.damaged-*")))

    def test_two_bad_generations_retain_evidence_and_open_last_saved(self):
        with self.session(primary=b"{bad", backup=b"{also bad", last_saved=True) as (work, proc, client, command):
            state = client.call("pulse_get_project")
            self.assertEqual(state["project"]["title"], "Last saved project")
            self.assertFalse(state["dirty"])
            self.assertEqual(Path(state["currentFile"]), work / "saved.pulse")
            self.assertEqual((work / "data/recovery-v0.29.pulse").read_bytes(), b"{bad")
            self.assertEqual((work / "data/recovery-v0.29.pulse.bak").read_bytes(), b"{also bad")

    def test_missing_primary_does_not_resurrect_retired_backup(self):
        with self.session(backup="valid", last_saved=True) as (work, proc, client, command):
            state = client.call("pulse_get_project")
            self.assertEqual(state["project"]["title"], "Last saved project")
            self.assertFalse(state["dirty"])

    def test_background_checkpoint_survives_actual_process_termination(self):
        with self.session(last_saved=True) as (work, proc, client, command):
            state = client.call("pulse_status")
            state = client.call("pulse_new_song", expected_revision=state["revision"], title="Background checkpoint", bpm=120)
            state = client.call("pulse_edit", expected_revision=state["revision"], operations=[{
                "op": "set_notes", "pattern": 0, "track": 7,
                "notes": [{"beat": 0, "length": 1, "pitch": 60, "velocity": .8}]}])
            expected = state["revision"]
            deadline = time.monotonic() + 25
            while time.monotonic() < deadline:
                state = client.call("pulse_status")
                if state["autosavedRevision"] == expected and not state["autosaveBusy"]:
                    break
                time.sleep(.1)
            self.assertEqual(state["autosavedRevision"], expected)
            self.assertTrue(state["dirty"])
            self.assertEqual(state["autosaveError"], "")
            recovery = work / "data/recovery-v0.29.pulse"
            checkpoint = recovery.read_bytes()
            # A later live edit has no completed checkpoint yet.
            state = client.call("pulse_edit", expected_revision=expected, operations=[{
                "op": "set_notes", "pattern": 0, "track": 7,
                "notes": [{"beat": 0, "length": 1, "pitch": 72, "velocity": .8}]}])
            session_file = work / "data/control/session.json"
            previous_session = json.loads(session_file.read_text())["session"]
            proc.kill()
            proc.wait(timeout=5)
            self.assertEqual(recovery.read_bytes(), checkpoint)
            restarted = subprocess.Popen(command + ["--resume", "--control-session"])
            try:
                deadline = time.monotonic() + 15
                current_session = previous_session
                while time.monotonic() < deadline:
                    self.assertIsNone(restarted.poll())
                    try:
                        current_session = json.loads(session_file.read_text())["session"]
                    except (PermissionError, FileNotFoundError):
                        time.sleep(.02)
                        continue
                    if current_session != previous_session:
                        break
                    time.sleep(.02)
                self.assertNotEqual(current_session, previous_session)
                with PulseClient(work) as resumed:
                    recovered = resumed.call("pulse_get_project")
                    self.assertEqual(recovered["title"], "Background checkpoint")
                    self.assertEqual(recovered["project"]["patterns"][0]["lanes"][7][0]["pitch"], 60)
                    self.assertTrue(recovered["dirty"])
                    self.assertEqual(recovered["currentFile"], "")
            finally:
                if restarted.poll() is None:
                    subprocess.run(command + ["--request-update"], check=True, timeout=10)
                    try:
                        restarted.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        restarted.kill()
                        restarted.wait(timeout=5)

    def test_opted_in_retention_runs_after_mcp_permanent_save(self):
        import hashlib
        policy = {"schema": 1, "keepNewest": 2, "maxAgeDays": 0, "maxMiB": 256, "automatic": True}
        with self.session(last_saved=True, retention=policy) as (work, proc, client, command):
            history = work / "data/History"
            history.mkdir(exist_ok=True)
            seed = json.loads((work / "song.pulse").read_bytes())
            for index in range(4):
                seed["title"] = f"Managed old copy {index}"
                raw = json.dumps(seed).encode()
                (history / ("Recovery_" + hashlib.sha256(raw).hexdigest() + ".pulse")).write_bytes(raw)
            safety = history / "Before_open_protected.pulse"
            safety.write_bytes(b"SAFETY COPY RETAINED")
            state = client.call("pulse_status")
            state = client.call("pulse_new_song", expected_revision=state["revision"], title="Retention saved song", bpm=120)
            saved = client.call("pulse_save_project", expected_revision=state["revision"], name="Retention saved song")
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                state = client.call("pulse_status")
                if not state["recoveryCleanupBusy"] and len(list(history.glob("Recovery_*.pulse"))) == 2:
                    break
                time.sleep(.05)
            self.assertEqual(len(list(history.glob("Recovery_*.pulse"))), 2)
            self.assertFalse(state["recoveryCleanupBusy"])
            self.assertFalse(state["dirty"])
            self.assertEqual(state["revision"], saved["revision"])
            self.assertEqual(state["title"], "Retention saved song")
            self.assertTrue(Path(saved["path"]).is_file())
            self.assertEqual(safety.read_bytes(), b"SAFETY COPY RETAINED")

    def test_existing_audio_fade_mcp_edit_save_export_and_undo(self):
        with self.session(last_saved=True, audio=True) as (work, proc, client, command):
            original = client.call("pulse_get_project")
            clip = original["project"]["clips"][0]
            source = (work / "original-input.wav").read_bytes()
            state = client.call("pulse_edit", expected_revision=original["revision"], operations=[{
                "op": "set_audio_fades", "clip": 0, "fadeIn": .5, "fadeInCurve": 1,
                "fadeOut": .75, "fadeOutCurve": 2}])
            changed = client.call("pulse_get_project")
            actual = changed["project"]["clips"][0]
            self.assertEqual((actual["fadeIn"], actual["fadeOut"], actual["fadeInCurve"], actual["fadeOutCurve"]), (.5, .75, 1, 2))
            self.assertEqual({k: v for k, v in actual.items() if k not in ("fadeIn", "fadeOut", "fadeInCurve", "fadeOutCurve")},
                             {k: v for k, v in clip.items() if k not in ("fadeIn", "fadeOut", "fadeInCurve", "fadeOutCurve")})
            bad = client.request("tools/call", {"name": "pulse_edit", "arguments": {
                "expected_revision": changed["revision"], "operations": [
                    {"op": "set_audio_fades", "clip": 0, "fadeOut": 1},
                    {"op": "set_audio_fades", "clip": 1, "fadeIn": 1}]}})
            self.assertTrue(bad["isError"])
            self.assertEqual(client.call("pulse_get_project")["project"], changed["project"])
            saved = client.call("pulse_save_project", expected_revision=changed["revision"], name="Curved audio")
            written = json.loads(Path(saved["path"]).read_text(encoding="utf-8-sig"))
            self.assertEqual(written["schema"], 18)
            self.assertEqual(written["clips"][0]["fadeOutCurve"], 2)
            relative = Path(written["clips"][0]["audioPath"])
            self.assertFalse(relative.is_absolute())
            self.assertEqual(relative.parts[0], "Assets")
            collected = Path(saved["path"]).parent / relative
            self.assertTrue(collected.resolve().is_relative_to((Path(saved["path"]).parent / "Assets").resolve()))
            self.assertEqual(collected.read_bytes(), source)
            state = client.call("pulse_status")
            client.call("pulse_export_audio", expected_revision=state["revision"], name="Curved audio")
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                state = client.call("pulse_status")
                if not state["exporting"]:
                    break
                time.sleep(.05)
            self.assertIn("Export complete", state["status"])
            self.assertGreater((work / "Exports/Curved audio.wav").stat().st_size, 100000)
            client.call("pulse_undo", expected_revision=state["revision"])
            restored = client.call("pulse_get_project")["project"]["clips"][0]
            self.assertEqual((restored["fadeInCurve"], restored["fadeOutCurve"]), (0, 0))
            self.assertEqual((work / "original-input.wav").read_bytes(), source)

    def test_update_handoff_stays_open_when_recovery_cannot_commit(self):
        with self.session(last_saved=True) as (work, proc, client, command):
            obstruction = work / "data/recovery-v0.29.pulse"
            obstruction.mkdir()
            try:
                subprocess.run(command + ["--request-update"], check=True, timeout=10)
                deadline = time.monotonic() + 5
                state = client.call("pulse_status")
                while not state["status"].startswith("Update postponed") and time.monotonic() < deadline:
                    time.sleep(.05)
                    state = client.call("pulse_status")
                self.assertIsNone(proc.poll())
                self.assertTrue(state["status"].startswith("Update postponed"))
                self.assertEqual(state["title"], "Last saved project")
            finally:
                obstruction.rmdir()
            subprocess.run(command + ["--request-update"], check=True, timeout=10)
            self.assertEqual(proc.wait(timeout=8), 0)
            self.assertEqual(json.loads(obstruction.read_text())["title"], "Last saved project")


if __name__ == "__main__":
    unittest.main()
