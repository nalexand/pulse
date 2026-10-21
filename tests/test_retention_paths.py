# SPDX-License-Identifier: AGPL-3.0-only
"""Windows junction and bounded-scan checks against the native cleanup planner."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build-native/PulseTests_artefacts/Release/PulseTests.exe"


@unittest.skipUnless(os.name == "nt" and EXE.is_file(), "Windows native retention probe required")
class RetentionPathChecks(unittest.TestCase):
    def probe(self, work):
        result = subprocess.run([str(EXE), "--retention-probe", str(work)], cwd=ROOT,
                                capture_output=True, text=True, check=True, timeout=20)
        return json.loads(result.stdout)

    def junction(self, link, target):
        result = subprocess.run(["cmd.exe", "/c", "mklink", "/J", str(link), str(target)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_junction_history_is_never_followed(self):
        with tempfile.TemporaryDirectory(prefix="retention-junction-", dir=ROOT / "test-output") as folder:
            work = Path(folder)
            self.assertTrue(work.resolve().is_relative_to((ROOT / "test-output").resolve()))
            target = work / "owned-target"
            target.mkdir()
            payload = target / "keep.txt"
            payload.write_bytes(b"KEEP EXACT TARGET")
            session = work / "session"
            (session / "data").mkdir(parents=True)
            link = session / "data/History"
            self.junction(link, target)
            try:
                state = self.probe(session)
                self.assertFalse(state["ok"])
                self.assertEqual(state["removed"], 0)
                self.assertEqual(payload.read_bytes(), b"KEEP EXACT TARGET")
            finally:
                # Remove the junction itself, without recursively touching target.
                link.rmdir()

    def test_nested_junction_and_unknown_files_are_excluded(self):
        with tempfile.TemporaryDirectory(prefix="retention-nested-", dir=ROOT / "test-output") as folder:
            work = Path(folder)
            self.assertTrue(work.resolve().is_relative_to((ROOT / "test-output").resolve()))
            subprocess.run([str(EXE), "--persistence-probe", str(work), "seed"], check=True, timeout=10)
            original = (work / "song.pulse").read_bytes()
            history = work / "data/History"
            history.mkdir(parents=True)
            for index in range(4):
                value = json.loads(original)
                value["title"] = f"Owned archive {index}"
                raw = json.dumps(value).encode()
                name = "Recovery_" + hashlib.sha256(raw).hexdigest() + ".pulse"
                (history / name).write_bytes(raw)
            target = work / "owned-target"
            target.mkdir()
            payload = target / "keep.pulse"
            payload.write_bytes(original)
            link = history / "Nested"
            self.junction(link, target)
            try:
                state = self.probe(work)
                self.assertTrue(state["ok"])
                self.assertEqual(state["removed"], 2)
                self.assertEqual(payload.read_bytes(), original)
                self.assertEqual(len(list(history.glob("Recovery_*.pulse"))), 2)
            finally:
                link.rmdir()

    def test_locked_copy_reports_partial_cleanup_and_retains_remaining_files(self):
        import ctypes
        from ctypes import wintypes
        import time
        with tempfile.TemporaryDirectory(prefix="retention-lock-", dir=ROOT / "test-output") as folder:
            work = Path(folder)
            self.assertTrue(work.resolve().is_relative_to((ROOT / "test-output").resolve()))
            subprocess.run([str(EXE), "--persistence-probe", str(work), "seed"], check=True, timeout=10)
            seed = json.loads((work / "song.pulse").read_bytes())
            history = work / "data/History"
            history.mkdir(parents=True)
            files = []
            now = time.time()
            for index in range(4):
                seed["title"] = f"Locked archive {index}"
                raw = json.dumps(seed).encode()
                file = history / ("Recovery_" + hashlib.sha256(raw).hexdigest() + ".pulse")
                file.write_bytes(raw)
                os.utime(file, (now - index * 86400, now - index * 86400))
                files.append(file)
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                          wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
            kernel.CreateFileW.restype = wintypes.HANDLE
            kernel.CloseHandle.argtypes = [wintypes.HANDLE]
            handle = kernel.CreateFileW(str(files[2]), 0x80000000, 1 | 2, None, 3, 0x80, None)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
            try:
                state = self.probe(work)
                self.assertFalse(state["ok"])
                self.assertEqual(state["removed"], 1)
                self.assertTrue(all(file.is_file() for file in files[:3]))
                self.assertFalse(files[3].exists())
            finally:
                kernel.CloseHandle(handle)

    def test_scan_limit_never_publishes_a_partial_delete_list(self):
        with tempfile.TemporaryDirectory(prefix="retention-bound-", dir=ROOT / "test-output") as folder:
            work = Path(folder)
            self.assertTrue(work.resolve().is_relative_to((ROOT / "test-output").resolve()))
            history = work / "data/History"
            history.mkdir(parents=True)
            for index in range(5001):
                (history / f"unmanaged-{index}.txt").touch()
            state = self.probe(work)
            self.assertFalse(state["ok"])
            self.assertEqual(state["removed"], 0)
            self.assertEqual(len(list(history.iterdir())), 5001)


if __name__ == "__main__":
    unittest.main()
