#!/usr/bin/env python3
import copy,json,pathlib,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import check_small_boundaries as c
class SmallBoundaries(unittest.TestCase):
 def setUp(self):self.m=json.loads(c.MANIFEST.read_text());self.args=c.load()
 def reject(self,f):
  f(self.m)
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_complete(self):self.assertEqual(c.validate(self.m,self.args),69)
 def test_missing(self):self.reject(lambda m:m['duties'].pop())
 def test_duplicate(self):self.reject(lambda m:m['duties'].__setitem__(0,copy.deepcopy(m['duties'][1])))
 def test_identity(self):self.reject(lambda m:m['duties'][0].__setitem__('source_range','subset'))
 def test_source(self):self.reject(lambda m:m['duties'][0].__setitem__('source_blob_sha256','bad'))
 def test_criterion(self):self.reject(lambda m:m.__setitem__('original_acceptance','only native rectangles'))
 def test_original_criterion(self):self.reject(lambda m:m['duties'][0].__setitem__('original_acceptance','smaller'))
 def test_ancestry(self):self.reject(lambda m:m['ancestor_contracts'].pop())
 def test_inherited(self):self.reject(lambda m:m['duties'][0]['related_obligations'].clear())
 def test_successor(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].pop('31.017'))
 def test_source_specific(self):self.reject(lambda m:m['duties'][0].__setitem__('source_specific_boundary_duty','generic boundaries only'))
 def test_parent(self):self.reject(lambda m:m['duties'][0].__setitem__('whole_hunk_checkpoint_status','DONE'))
 def test_reverse(self):
  self.args[3]['31.017']['commit_or_artifact']='tasks.md'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_inventory(self):
  self.args[1][self.m['duties'][0]['work_id']]['status']='DONE'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_evidence(self):self.reject(lambda m:m.__setitem__('evidence',[]))
if __name__=='__main__':unittest.main()
