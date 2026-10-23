#!/usr/bin/env python3
"""Semantic controls reseal local artifact hashes, not acceptance criteria."""
import hashlib,json,pathlib,shutil,subprocess,tempfile,unittest
R=pathlib.Path(__file__).resolve().parents[2];SOURCE=R/'migration/tests/container-reentrant-membership'
class Controls(unittest.TestCase):
 def check(self,change=None):
  with tempfile.TemporaryDirectory() as d:
   e=pathlib.Path(d)/'migration/tests/container-reentrant-membership';shutil.copytree(SOURCE,e)
   if change:change(e)
   pins=json.loads((e/'artifact-sha256.json').read_text())
   for n in pins:pins[n]=hashlib.sha256((e/n).read_bytes()).hexdigest()
   (e/'artifact-sha256.json').write_text(json.dumps(pins))
   p=subprocess.run(['python3',str(e/'verify.py')],capture_output=True,text=True)
   self.assertEqual(p.returncode==0,change is None,p.stdout+p.stderr)
 def edit(self,e,n,fn):
  p=e/n;x=json.loads(p.read_text());fn(x);p.write_text(json.dumps(x))
 def test_valid(self):self.check()
 def test_whole_hunk_false_closure(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(whole_hunk_closure=True)))
 def test_rendered_claim(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(rendered_ui_evidence=True)))
 def test_sanitizer_scope(self):self.check(lambda e:self.edit(e,'asan-build.json',lambda x:x['instrumented_sources'].pop()))
 def test_mutant_pass(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x['mutations'][0].update(exit_code=0)))
 def test_missing_native_case(self):self.check(lambda e:(e/'normal-portrait.log').write_text((e/'normal-portrait.log').read_text().replace('ok 13 ','omitted 13 ')))
 def test_portrait_required(self):self.check(lambda e:(e/'asan-portrait.log').write_text((e/'asan-portrait.log').read_text().replace('1080x1920','1920x1080')))
 def test_source_archive_mismatch(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x['source_sha256'].update({'app/core/gimpcontainer.c':'0'*64})))
if __name__=='__main__':unittest.main()
