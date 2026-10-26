# SPDX-License-Identifier: AGPL-3.0-only
"""Terminate the owned recorder process without destructors, then recover its acknowledged PCM prefix."""
import hashlib,json,struct,subprocess,tempfile,time,unittest,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROBE=ROOT/'build-native/PulseTests_artefacts/Release/PulseTests.exe'
@unittest.skipUnless(PROBE.is_file(),'build PulseTests first')
class TakeCrashChecks(unittest.TestCase):
    def crashed(self,work,mode):
        proc=subprocess.Popen([str(PROBE),'--take-probe',str(work),mode],creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        try:
            deadline=time.monotonic()+15
            while not (work/'probe-ready.txt').exists() and time.monotonic()<deadline:
                self.assertIsNone(proc.poll(),'recorder exited before its flush checkpoint');time.sleep(.01)
            self.assertTrue((work/'probe-ready.txt').is_file(),'recording checkpoint deadline')
            directory=Path((work/'take-directory.txt').read_text(encoding='utf-8-sig'))
            checkpoint=json.loads((directory/'checkpoint.json').read_text(encoding='utf-8-sig'))
            proc.kill();proc.wait(timeout=5) # TerminateProcess: no final flush, destructor or UI finalization.
            return directory,checkpoint
        finally:
            if proc.poll() is None:proc.kill();proc.wait(timeout=5)
    def recover(self,work,ok=True):
        result=subprocess.run([str(PROBE),'--take-probe',str(work),'recover'],capture_output=True,text=True,encoding='utf-8',timeout=20)
        self.assertEqual(result.returncode,0 if ok else 1,result.stderr)
        return Path((work/'recovered-path.txt').read_text(encoding='utf-8-sig')) if ok else None
    def test_actual_termination_recovers_exact_stereo_and_mono_checkpoint_bytes(self):
        for mode,channels in [('stereo',2),('mono',1)]:
            with self.subTest(mode=mode),tempfile.TemporaryDirectory(prefix='take-crash-',dir=ROOT/'test-output') as folder:
                work=Path(folder);directory,c=self.crashed(work,mode);self.assertEqual(c['frames'],16000)
                original=next(directory.glob('Take_*.wav'));data=bytearray(original.read_bytes());offset=c['dataOffset'];pcm=bytes(data[offset:offset+c['frames']*channels*3]);self.assertEqual(len(pcm),c['frames']*channels*3)
                # Simulate a torn later header and unacknowledged partial write. Recovery never edits it.
                struct.pack_into('<I',data,4,0xffffffff);struct.pack_into('<I',data,offset-4,0);data.extend(b'partial-write!')
                original.write_bytes(data);before=hashlib.sha256(original.read_bytes()).hexdigest();project=self.recover(work)
                with wave.open(str(project.with_name('recovered.wav')),'rb') as audio:
                    self.assertEqual((audio.getnchannels(),audio.getsampwidth(),audio.getframerate(),audio.getnframes()),(channels,3,8000,16000));self.assertEqual(audio.readframes(16000),pcm)
                p=json.loads(project.read_text(encoding='utf-8-sig'));self.assertEqual(p['clips'][-1]['beat'],4);self.assertEqual(p['clips'][-1]['length'],4);self.assertEqual(p['patterns'][0]['lanes'][7][0]['pitch'],65)
                self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(),before)
                again=self.recover(work);self.assertEqual(again,project);self.assertEqual(len(list(directory.glob('Recovered-*'))),1)
    def test_unacknowledged_short_take_is_retained_without_inventing_audio(self):
        with tempfile.TemporaryDirectory(prefix='take-unflushed-',dir=ROOT/'test-output') as folder:
            work=Path(folder);directory,c=self.crashed(work,'unflushed');self.assertEqual(c['frames'],0);before=(next(directory.glob('Take_*.wav'))).read_bytes();self.recover(work,False);self.assertEqual((next(directory.glob('Take_*.wav'))).read_bytes(),before);self.assertEqual(list(directory.glob('Recovered-*')),[])
    def test_corrupt_acknowledged_data_or_session_never_publishes_copy(self):
        for kind in ('truncate','session','tail','identity'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory(prefix='take-invalid-',dir=ROOT/'test-output') as folder:
                work=Path(folder);directory,c=self.crashed(work,'stereo');audio=next(directory.glob('Take_*.wav'))
                if kind=='truncate':audio.write_bytes(audio.read_bytes()[:c['dataOffset']+c['frames']*6-1])
                elif kind=='session':(directory/'session.pulse').write_text('invalid',encoding='utf-8')
                elif kind=='tail':
                    b=bytearray(audio.read_bytes());b[c['dataOffset']+c['frames']*6-1]^=0x7f;audio.write_bytes(b)
                else:
                    c['id']='../../outside';(directory/'checkpoint.json').write_text(json.dumps(c),encoding='utf-8')
                before=audio.read_bytes();self.recover(work,False);self.assertEqual(audio.read_bytes(),before);self.assertEqual(list(directory.glob('Recovered-*')),[])
if __name__=='__main__':unittest.main()
