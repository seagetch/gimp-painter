#!/usr/bin/env python3
"""Resealed malformed receipts must fail independently of artifact hash checks."""
import pathlib,tempfile,shutil,json,hashlib,subprocess,unittest
E=pathlib.Path(__file__).resolve().parent
R=E.parents[2]
class Proof(unittest.TestCase):
 def reject(self,name,change):
  with tempfile.TemporaryDirectory() as tmp:
   d=pathlib.Path(tmp)/'proof';shutil.copytree(E,d)
   p=d/name;v=json.loads(p.read_text());change(v);p.write_text(json.dumps(v))
   seal=json.loads((d/'artifact-sha256.json').read_text());seal[name]=hashlib.sha256(p.read_bytes()).hexdigest();(d/'artifact-sha256.json').write_text(json.dumps(seal))
   run=subprocess.run(['python3',str(d/'validate.py'),'--root',str(R)],capture_output=True,text=True)
   self.assertNotEqual(run.returncode,0,run.stdout)
 def test_native_failure(self):self.reject('normal-results.json',lambda v:v[0].update(returncode=1))
 def test_missing_native_case(self):self.reject('normal-results.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('ok 16','notok 16')))
 def test_sanitizer_failure(self):self.reject('asan-results.json',lambda v:v.update(returncode=1))
 def test_instrumented_unit_missing(self):self.reject('asan-build.json',lambda v:v['instrumented_sources'].pop())
 def test_build_source_changed(self):self.reject('asan-build.json',lambda v:v.update(changed_during_build=['changed']))
 def test_criterion_weakened(self):self.reject('acceptance.json',lambda v:v.update(criterion='pixel only'))
 def test_physical_task_claimed(self):self.reject('acceptance.json',lambda v:v.update(not_closed=[]))
 def test_duty_missing(self):self.reject('acceptance.json',lambda v:v['hunk_ids'].pop())
 def test_held_baseline_missing(self):self.reject('excluded-provider-seal.json',lambda v:v.pop())
if __name__=='__main__':unittest.main()
