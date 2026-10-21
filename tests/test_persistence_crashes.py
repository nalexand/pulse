# SPDX-License-Identifier: AGPL-3.0-only
"""Actual process termination and Windows sharing failures against the save path."""
import ctypes
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build-native/PulseTests_artefacts/Release/PulseTests.exe"


@unittest.skipUnless(EXE.is_file(), "build PulseTests before native crash checks")
class PersistenceCrashChecks(unittest.TestCase):
    def test_terminated_saves_keep_a_complete_committed_generation(self):
        for stage in ("written", "flushed", "verified", "backup", "beforeCommit", "committed"):
            with self.subTest(stage=stage), tempfile.TemporaryDirectory(prefix="save-crash-", dir=ROOT / "test-output") as folder:
                work = Path(folder)
                subprocess.run([str(EXE), "--persistence-probe", folder, "seed"], check=True, timeout=10)
                primary = work / "song.pulse"
                original = primary.read_bytes()
                proc = subprocess.Popen([str(EXE), "--persistence-probe", folder, stage])
                try:
                    deadline = time.monotonic() + 15
                    while not (work / "probe-reached.txt").exists() and time.monotonic() < deadline:
                        self.assertIsNone(proc.poll(), "save probe exited before its checkpoint")
                        time.sleep(.01)
                    self.assertTrue((work / "probe-reached.txt").is_file(), "checkpoint deadline exceeded")
                    proc.kill()  # TerminateProcess on Windows; no destructors or cleanup.
                    proc.wait(timeout=5)
                    parsed = json.loads(primary.read_text(encoding="utf-8-sig"))
                    self.assertEqual(parsed["title"], "Candidate" if stage == "committed" else "Last committed")
                    self.assertEqual(parsed["patterns"][0]["lanes"][7][0]["pitch"], 67 if stage == "committed" else 60)
                    if stage != "committed":
                        self.assertEqual(primary.read_bytes(), original)
                    if stage in ("backup", "beforeCommit", "committed"):
                        self.assertEqual((work / "song.pulse.bak").read_bytes(), original)
                    # Restarting ignores abandoned staging and can save normally.
                    subprocess.run([str(EXE), "--persistence-probe", folder, "normal"], check=True, timeout=10)
                    self.assertEqual(json.loads(primary.read_text())["title"], "Candidate")
                    json.loads((work / "song.pulse.bak").read_text())
                finally:
                    if proc.poll() is None:
                        proc.kill()
                        proc.wait(timeout=5)

    @unittest.skipUnless(__import__("os").name == "nt", "Windows sharing semantics")
    def test_locked_destination_rejects_commit_and_retains_song(self):
        with tempfile.TemporaryDirectory(prefix="save-lock-", dir=ROOT / "test-output") as folder:
            subprocess.run([str(EXE), "--persistence-probe", folder, "seed"], check=True, timeout=10)
            primary = Path(folder) / "song.pulse"
            original = primary.read_bytes()
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_void_p]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel.CreateFileW(str(primary), 0x80000000, 1, None, 3, 0, None)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
            try:
                result = subprocess.run([str(EXE), "--persistence-probe", folder, "normal"], timeout=10)
                self.assertEqual(result.returncode, 3)
                self.assertEqual(primary.read_bytes(), original)
            finally:
                kernel.CloseHandle(handle)


if __name__ == "__main__":
    unittest.main()
