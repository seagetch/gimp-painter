#!/usr/bin/env python3
import pathlib,tempfile,shutil,json,hashlib,subprocess,unittest
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
class Proof(unittest.TestCase):
 def reject(self,name,change):
  with tempfile.TemporaryDirectory()as tmp:
   d=pathlib.Path(tmp)/'proof';shutil.copytree(E,d);p=d/name;v=json.loads(p.read_text());change(v);p.write_text(json.dumps(v));seal=json.loads((d/'artifact-sha256.json').read_text());seal[name]=hashlib.sha256(p.read_bytes()).hexdigest();(d/'artifact-sha256.json').write_text(json.dumps(seal));r=subprocess.run(['python3',str(d/'validate.py'),'--root',str(R)],capture_output=True,text=True);self.assertNotEqual(r.returncode,0,r.stdout)
 def test_no_false_original_closure(self):self.reject('scope.json',lambda v:v.update(original_closed=True))
 def test_save_not_silently_resolved(self):self.reject('scope.json',lambda v:v.update(save_then_continuation_entries=1))
 def test_original_criterion(self):self.reject('scope.json',lambda v:v.update(original_criterion='logical segment'))
 def test_native_failure(self):self.reject('normal-results.json',lambda v:v[0].update(returncode=1))
 def test_missing_native_target(self):self.reject('normal-results.json',lambda v:v.pop())
 def test_sanitizer_failure(self):self.reject('asan-results.json',lambda v:v[0].update(returncode=1))
 def test_sanitizer_scope(self):self.reject('asan-build.json',lambda v:v['instrumented_sources'].pop())
 def test_missing_source(self):self.reject('source-manifest.json',lambda v:v.pop())
 def test_missing_header(self):self.reject('headers.json',lambda v:v.pop())
 def test_missing_mutant(self):self.reject('mutations.json',lambda v:v.pop())
 def test_missing_exclusion(self):self.reject('excluded-provider-seal.json',lambda v:v.pop())
if __name__=='__main__':unittest.main()
