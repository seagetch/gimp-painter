import hashlib,json,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class GraphProof(unittest.TestCase):
 def check(self,change=None):
  with tempfile.TemporaryDirectory()as tmp:
   e=pathlib.Path(tmp)/'proof';shutil.copytree(ROOT/'migration/tests/gegl-graph-invalidation',e)
   if change:change(e)
   pins=json.loads((e/'artifact-sha256.json').read_text())
   for n in pins:pins[n]=hashlib.sha256((e/n).read_bytes()).hexdigest()
   (e/'artifact-sha256.json').write_text(json.dumps(pins))
   r=subprocess.run(['python3',str(e/'verify.py')],capture_output=True,text=True);self.assertEqual(r.returncode==0,change is None,r.stdout+r.stderr)
 def edit(self,e,n,fn):
  p=e/n;x=json.loads(p.read_text());fn(x);p.write_text(json.dumps(x))
 def test_valid(self):self.check()
 def test_whole_source_false_claim(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(whole_current_application_claim=True)))
 def test_original_duties_false_closure(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(full_source_duties_done=True)))
 def test_oncanvas_disabled(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(oncanvas_enabled=False)))
 def test_landscape(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x.update(portrait=[1920,1080])))
 def test_missing_model_case(self):self.check(lambda e:self.edit(e,'normal-results.json',lambda x:x[0].update(stdout=x[0]['stdout'].replace('ok 6 ','omitted 6 '))))
 def test_skipped_ui(self):self.check(lambda e:self.edit(e,'ui-results.json',lambda x:x[0].update(result='SKIP')))
 def test_no_visible_change(self):self.check(lambda e:(e/'graph-1-after.png').write_bytes((e/'graph-1-before.png').read_bytes()))
 def test_wrong_raw_pixel(self):self.check(lambda e:self.edit(e,'ui-results.json',lambda x:x[0].update(stdout=x[0]['stdout'].replace('RGB=255,0,0','RGB=0,0,255'))))
 def test_mutant_pass(self):self.check(lambda e:self.edit(e,'mutations/drop-stack-origin/result.json',lambda x:x.update(result='OK')))
 def test_removed_instrumentation(self):self.check(lambda e:self.edit(e,'sanitizer-scope.json',lambda x:x['relevant_instrumented_units'][0].update(sanitize_flags=[])))
 def test_lost_original_duty(self):self.check(lambda e:self.edit(e,'original-duty-snapshot.json',lambda x:x['rows'].pop()))
 def test_closed_original_hunk(self):self.check(lambda e:self.edit(e,'original-duty-snapshot.json',lambda x:x['rows'][0].update(implementation_state='IMPLEMENTED')))
 def test_closed_full_drawable(self):self.check(lambda e:self.edit(e,'inherited-drawable-duty.json',lambda x:x[0].update(whole_hunk_checkpoint_status='DONE')))
 def test_held_candidate_import(self):self.check(lambda e:self.edit(e,'held-source-seal.json',lambda x:x['rows'][0].update(sha256='0'*64)))
 def test_source_mismatch(self):self.check(lambda e:self.edit(e,'report.json',lambda x:x['source_sha256'].update({'app/core/gimpprojection.c':'0'*64})))
if __name__=='__main__':unittest.main()
