#!/usr/bin/env python3
import copy,json,pathlib,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import check_work_buffer as c
class WorkBufferCriterion(unittest.TestCase):
 def setUp(self):self.m=json.loads(c.MANIFEST.read_text());self.args=c.load()
 def reject(self,edit):
  edit(self.m)
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_complete(self):self.assertEqual(c.validate(self.m,self.args),14)
 def test_missing(self):self.reject(lambda m:m['duties'].clear())
 def test_duplicate(self):self.reject(lambda m:m['duties'].append(copy.deepcopy(m['duties'][0])))
 def test_source(self):self.reject(lambda m:m['duties'][0].__setitem__('source_range','selection subset'))
 def test_digest(self):self.reject(lambda m:m['duties'][0].__setitem__('source_blob_sha256','bad'))
 def test_criterion(self):self.reject(lambda m:m.__setitem__('original_acceptance','allow post-start mutation'))
 def test_inherited(self):self.reject(lambda m:m['duties'][-1]['related_obligations'].clear())
 def test_successor(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].pop('31.017'))
 def test_owner(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].__setitem__('99.999','later'))
 def test_self(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].__setitem__('09.009','later'))
 def test_parent_completion(self):self.reject(lambda m:m.__setitem__('whole_hunk_checkpoint_status','DONE'))
 def test_reverse_link(self):
  owner='31.017';self.args[3][owner]['commit_or_artifact']='tasks.md'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_completion(self):
  self.args[1][self.m['duties'][0]['work_id']]['status']='DONE'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
if __name__=='__main__':unittest.main()
