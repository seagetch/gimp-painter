#!/usr/bin/env python3
import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('resolution',ROOT/'tools/check_drawable_buffer_read_resolution.py');c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
class Resolution(unittest.TestCase):
 def setUp(self):self.m=json.loads(c.MANIFEST.read_text());self.args=c.load()
 def reject(self,edit):
  edit(self.m)
  with self.assertRaises(ValueError):c.validate(self.m,*self.args)
 def test_complete(self):self.assertEqual(c.validate(self.m,*self.args),48)
 def test_missing(self):self.reject(lambda m:m['duties'].pop())
 def test_duplicate(self):self.reject(lambda m:m['duties'].__setitem__(0,copy.deepcopy(m['duties'][1])))
 def test_source(self):self.reject(lambda m:m['duties'][0].__setitem__('source_sha256','changed'))
 def test_successor(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].pop('31.017'))
 def test_undo(self):self.reject(lambda m:m['duties'][-1]['successor_obligations'].pop('09.011'))
 def test_original(self):self.reject(lambda m:m.__setitem__('original_acceptance','read some pixels'))
 def test_hunk_completion(self):self.reject(lambda m:m['duties'][0].__setitem__('whole_hunk_status','DONE'))
 def test_self_dependency(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].__setitem__('09.003','later'))
 def test_unknown_owner(self):self.reject(lambda m:m['duties'][0]['successor_obligations'].__setitem__('99.999','later'))
 def future(self):
  args=copy.deepcopy(self.args);row=self.m['duties'][0];args[1][row['work_id']]['status']='DONE'
  for owner in row['successor_obligations']:
   args[2][owner][0]='[x]';args[3][owner].update(status='DONE',test='Native regression passed',result='PASS')
   args[3][owner]['commit_or_artifact']+='; migration/tests/drawable-buffer-read-parent/report.json'
  return args
 def test_premature_current_hunk_done(self):
  args=copy.deepcopy(self.args);args[1][self.m['duties'][0]['work_id']]['status']='DONE'
  with self.assertRaises(ValueError):c.validate(self.m,*args)
 def test_future_missing_test_evidence(self):
  args=self.future();args[3]['31.017']['test']='-'
  with self.assertRaises(ValueError):c.validate(self.m,*args)
 def test_future_missing_result_evidence(self):
  args=self.future();args[3]['31.017']['result']='pending'
  with self.assertRaises(ValueError):c.validate(self.m,*args)
 def test_future_crosslink_only_evidence(self):
  args=self.future();args[3]['31.017']['commit_or_artifact']='migration/contracts/drawable-buffer-read-resolution.json#31.017'
  with self.assertRaises(ValueError):c.validate(self.m,*args)
 def test_future_evidenced_completion(self):self.assertEqual(c.validate(self.m,*self.future()),48)
 def test_tempbuf_consumer_owner(self):self.reject(lambda m:next(d for d in m['duties'] if 'temp-buf' in d['path'])['successor_obligations'].pop('31.017'))
 def test_read_write_conversion_not_whole_consumer_completion(self):
  row=next(d for d in self.m['duties'] if 'temp-buf' in d['path']);args=copy.deepcopy(self.args)
  args[1][row['work_id']]['status']='DONE'
  for owner in ('09.004','09.005'):
   args[2][owner][0]='[x]';args[3][owner].update(status='DONE',test='Original criterion verified',result='PASS')
   args[3][owner]['commit_or_artifact']+='; migration/tests/tempbuf-conversion/standalone.json'
  with self.assertRaises(ValueError):c.validate(self.m,*args)
if __name__=='__main__':unittest.main()
