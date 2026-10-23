#!/usr/bin/env python3
import hashlib,json,pathlib,shutil,subprocess,tempfile,unittest
R=pathlib.Path(__file__).resolve().parents[2];SOURCE=R/'migration/tests/native-stroke-undo-09011'
class Controls(unittest.TestCase):
 def check(self,change=None):
  with tempfile.TemporaryDirectory()as d:
   e=pathlib.Path(d)/'migration/tests/native-stroke-undo-09011';shutil.copytree(SOURCE,e)
   if change:change(e)
   pins=json.loads((e/'artifact-sha256.json').read_text())
   for n in pins:pins[n]=hashlib.sha256((e/n).read_bytes()).hexdigest()
   (e/'artifact-sha256.json').write_text(json.dumps(pins));p=subprocess.run(['python3',str(e/'verify.py')],capture_output=True,text=True);self.assertEqual(p.returncode==0,change is None,p.stdout+p.stderr)
 def edit(self,e,n,fn):
  p=e/n;x=json.loads(p.read_text());fn(x);p.write_text(json.dumps(x))
 def test_valid(self):self.check()
 def test_false_paired_closure(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(real_engine_pairing_complete=True)))
 def test_history_publication(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(publishes_history_in_factory=True)))
 def test_missing_oncanvas(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(oncanvas_enabled=False)))
 def test_false_ui_claim(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(rendered_ui_evidence=True)))
 def test_changed_sanitizer_scope(self):self.check(lambda e:self.edit(e,'asan-build.json',lambda x:x['instrumented_sources'].pop()))
 def test_mutant_pass(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x['mutants'][0].update(exit_code=0)))
 def test_source_mismatch(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x['source_sha256'].update({'app/paint/gimpmypaintcoreundo.cpp':'0'*64})))
 def test_missing_case(self):self.check(lambda e:(e/'asan-portrait.log').write_text((e/'asan-portrait.log').read_text().replace('ok 9 ','omitted 9 ')))
 def test_wrong_portrait(self):self.check(lambda e:(e/'restored-portrait.log').write_text((e/'restored-portrait.log').read_text().replace('1080x1920','1920x1080')))
 def test_held_source_wrong(self):self.check(lambda e:self.edit(e,'held-source-seal.json',lambda x:x[0].update(tested='0'*64)))
 def test_missing_red(self):self.check(lambda e:self.edit(e,'buffer-observer-red.json',lambda x:x.update(result='OK')))
 def test_header_wrong_abi(self):self.check(lambda e:self.edit(e,'header.json',lambda x:x['command'].remove('-fno-rtti')))
if __name__=='__main__':unittest.main()
