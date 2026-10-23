#!/usr/bin/env python3
import copy,json,pathlib,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import check_precision_refusal as c
class PrecisionRefusal(unittest.TestCase):
 def setUp(self):self.m=json.loads(c.MANIFEST.read_text());self.args=c.load()
 def reject(self,f):
  f(self.m)
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_complete(self):self.assertEqual(c.validate(self.m,self.args),2)
 def test_criterion(self):self.reject(lambda m:m.__setitem__('original_acceptance','silently convert'))
 def test_dependency(self):self.reject(lambda m:m.__setitem__('dependencies',[]))
 def test_direct(self):self.reject(lambda m:m['direct_duties'].append({'work_id':'invented'}))
 def test_feature(self):self.reject(lambda m:m.__setitem__('whole_legacy_feature_complete',True))
 def test_missing(self):self.reject(lambda m:m['inherited_duties'].pop())
 def test_original(self):self.reject(lambda m:m['inherited_duties'][0].__setitem__('source_range','subset'))
 def test_obligation(self):self.reject(lambda m:m['inherited_duties'][0].__setitem__('inherited_09_015_obligation','narrower'))
 def test_successor(self):self.reject(lambda m:m['inherited_duties'][0]['successor_obligations'].pop('31.017'))
 def test_parent(self):self.reject(lambda m:m['inherited_duties'][0].__setitem__('whole_hunk_checkpoint_status','DONE'))
 def test_policy(self):self.reject(lambda m:m['compatibility_policies'].clear())
 def test_reverse(self):
  self.args[3]['31.017']['commit_or_artifact']='tasks.md'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_inventory(self):
  self.args[1][self.m['inherited_duties'][0]['work_id']]['status']='DONE'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_evidence(self):self.reject(lambda m:m.__setitem__('evidence',[]))
if __name__=='__main__':unittest.main()
