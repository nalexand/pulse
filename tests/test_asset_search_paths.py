# SPDX-License-Identifier: AGPL-3.0-only
"""Real Windows junction boundaries for the bounded audio-content search."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import wave

ROOT=Path(__file__).resolve().parents[1]
PROBE=ROOT/'build-native/PulseTests_artefacts/Release/PulseTests.exe'

@unittest.skipUnless(os.name=='nt' and PROBE.is_file(),'Windows native test executable required')
class AssetSearchPathChecks(unittest.TestCase):
    def run_case(self,root_link):
        with tempfile.TemporaryDirectory(prefix='asset-search-path-',dir=ROOT/'test-output') as folder:
            work=Path(folder).resolve()
            self.assertTrue(work.is_relative_to((ROOT/'test-output').resolve()))
            search=work/'search';external=work/'external'
            search.mkdir();external.mkdir()
            with wave.open(str(external/'same.wav'),'wb') as audio:
                audio.setparams((2,2,48000,0,'NONE','not compressed'))
                audio.writeframes(struct.pack('<hh',1200,-800)*4800)
            raw=(external/'same.wav').read_bytes()
            digest=hashlib.sha256(raw).hexdigest()
            junction=work/'linked-root' if root_link else search/'linked-child'
            self.assertTrue(junction.parent.resolve().is_relative_to(work))
            self.assertTrue(external.resolve().is_relative_to(work))
            subprocess.run(['cmd','/c','mklink','/J',str(junction),str(external)],check=True,capture_output=True,timeout=10)
            try:
                chosen=junction if root_link else search
                if not root_link:shutil.copyfile(external/'same.wav',search/'inside.wav')
                result=subprocess.run([str(PROBE),'--asset-search-probe',str(chosen),digest,str(len(raw))],check=True,capture_output=True,text=True,timeout=15)
                report=json.loads(result.stdout)
                if root_link:
                    self.assertFalse(report['complete']);self.assertEqual(report['candidates'],[]);self.assertIn('ordinary',report['message'])
                else:
                    self.assertTrue(report['complete']);self.assertEqual(report['candidates'],[str(search/'inside.wav')])
                self.assertEqual((external/'same.wav').read_bytes(),raw)
            finally:
                # Remove only this verified, owned junction itself; never its target.
                os.rmdir(junction)

    def test_nested_junction_is_not_searched(self):self.run_case(False)
    def test_junction_cannot_be_the_search_root(self):self.run_case(True)

if __name__=='__main__':unittest.main(verbosity=2)
