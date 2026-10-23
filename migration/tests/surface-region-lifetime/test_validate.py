#!/usr/bin/env python3
import copy,json,pathlib,unittest
from validate import validate,validate_links
HERE=pathlib.Path(__file__).resolve().parent
class NegativeControls(unittest.TestCase):
 def setUp(self):
  self.d=json.loads((HERE/'results.json').read_text());self.n=json.loads((HERE/'native.json').read_text());self.m=json.loads((HERE/'ownership-map.json').read_text())
 def reject(self):
  with self.assertRaises(ValueError):validate(self.d,self.n,self.m)
 def test_missing_legacy_site(self):self.m['legacy_sites'].pop();self.reject()
 def test_factory_gap_erased(self):self.m['legacy_sites'][-1]['remaining_tasks']=[];self.reject()
 def test_whole_hunk_claim(self):self.m['whole_hunk_status']='DONE';self.reject()
 def test_sanitizer_failure(self):self.d['results'][1]['returncode']=1;self.reject()
 def test_test_skipped(self):self.d['results'][0]['stdout']+='\n# SKIP untested\n';self.reject()
 def test_extended_budget(self):self.d['results'][1]['timeout']=600;self.reject()
 def test_dropped_ancestor(self):self.m['ancestor_duties'].pop(next(iter(self.m['ancestor_duties'])));self.reject()
 def test_missing_reverse_link(self):
  with self.assertRaises(ValueError):validate_links({'commit_or_artifact':'ownership-map.json'})
 def test_missing_native(self):self.n.pop();self.reject()
if __name__=='__main__':unittest.main()
