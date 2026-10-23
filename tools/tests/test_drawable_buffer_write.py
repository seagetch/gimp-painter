#!/usr/bin/env python3
import copy,importlib.util,json,pathlib,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import check_drawable_buffer_write as c
class WriteCriterion(unittest.TestCase):
 def setUp(self):self.m=json.loads(c.MANIFEST.read_text());self.args=c.load()
 def reject(self,edit):
  edit(self.m)
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_complete(self):self.assertEqual(c.validate(self.m,self.args),14)
 def test_missing(self):self.reject(lambda m:m['duties'].pop())
 def test_duplicate(self):self.reject(lambda m:m['duties'].__setitem__(0,copy.deepcopy(m['duties'][1])))
 def test_identity(self):self.reject(lambda m:m['duties'][0].__setitem__('source_sha256','changed'))
 def test_original(self):self.reject(lambda m:m.__setitem__('original_acceptance','writes'))
 def test_dispatch(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].pop('31.017'))
 def test_undo(self):self.reject(lambda m:m['duties'][-1]['successor_obligations'].pop('09.011'))
 def test_inherited(self):self.reject(lambda m:m['duties'][0].__setitem__('inherited_09_004_obligation','changed'))
 def test_checkpoint(self):self.reject(lambda m:m['duties'][0].__setitem__('whole_hunk_checkpoint_status','DONE'))
 def test_premature_current_completion(self):
  self.args[1][self.m['duties'][0]['work_id']]['status']='DONE'
  with self.assertRaises(ValueError):c.validate(self.m,self.args)
 def test_unknown_owner(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].__setitem__('99.999','later'))
 def test_self_dependency(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].__setitem__('09.004','later'))
 def test_future_evidenced_completion(self):
  self.args[1][self.m['duties'][0]['work_id']]['status']='DONE';self.args[2]['31.017'][0]='[x]'
  self.args[3]['31.017'].update(status='DONE',test='Native dispatcher replacement tests',result='PASS')
  self.args[3]['31.017']['commit_or_artifact']+='; migration/tests/future-dispatch.json'
  self.assertEqual(c.validate(self.m,self.args),14)
if __name__=='__main__':unittest.main()
