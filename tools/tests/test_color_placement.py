#!/usr/bin/env python3
import copy,json,pathlib,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import check_color_placement as c
class ColorPlacement(unittest.TestCase):
 def setUp(self):self.m=json.loads(c.MANIFEST.read_text());self.args=c.load()
 def reject(self,f):
  f(self.m)
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_complete(self):self.assertEqual(c.validate(self.m,self.args),0)
 def test_criterion(self):self.reject(lambda m:m.__setitem__('original_acceptance','approximate'))
 def test_dependency(self):self.reject(lambda m:m.__setitem__('dependencies',[]))
 def test_duties(self):self.reject(lambda m:m['duties'].append({'work_id':'invented'}))
 def test_feature(self):self.reject(lambda m:m.__setitem__('whole_legacy_feature_complete',True))
 def test_policy_missing(self):self.reject(lambda m:m['compatibility_policies'].clear())
 def test_policy_changed(self):self.reject(lambda m:m['compatibility_policies'].__setitem__('migration/contracts/mypaint-gray-surface.md','bad'))
 def test_successor(self):self.reject(lambda m:m['deferred_criteria'].pop('09.015'))
 def test_successor_criterion(self):self.reject(lambda m:m['deferred_criteria'].__setitem__('09.016','smaller scope'))
 def test_reverse(self):
  self.args[3]['09.017']['commit_or_artifact']='tasks.md'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_evidence(self):self.reject(lambda m:m.__setitem__('evidence',[]))
if __name__=='__main__':unittest.main()
