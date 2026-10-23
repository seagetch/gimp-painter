#!/usr/bin/env python3
"""Historical invariants remain strict; live owners can legitimately complete."""
import copy,importlib.util,json,pathlib,sys,unittest,tempfile,shutil
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[4];sys.path.insert(0,str(ROOT/'tools'))
import check_cpp_registration as checker
from assign_legacy_hunks import read_tsv
class Recovery(unittest.TestCase):
 def setUp(self):
  self.inv=ROOT/'migration/inventory';self.review=json.loads((self.inv/'cpp-registration-review.json').read_text());self.work={r['work_id']:r for r in read_tsv(self.inv/'legacy-port-work-items.tsv')};self.assignments={r['hunk_id']:r for r in read_tsv(self.inv/'hunk-wbs.tsv')}
 def check(self):checker.check_current_lineage(self.review,self.work,self.assignments,self.inv)
 def reject(self):
  with self.assertRaises((AssertionError,KeyError)):self.check()
 def registered(self):return next(r['original_work_id'] for r in self.review['reviews'] if r['registration_status']=='VERIFIED_AFTER_CURRENT_BUILD')
 def test_current_pass(self):self.check()
 def test_missing_registration(self):del self.work[self.registered()];self.reject()
 def test_registration_not_done(self):self.work[self.registered()]['status']='TODO';self.reject()
 def test_missing_source_ownership(self):
  source=self.review['reviews'][-1]['source_id'];self.work={k:v for k,v in self.work.items() if v['source_id']!=source};self.reject()
 def test_unreviewed_retirement(self):
  row=next(r for r in self.review['reviews'] if r['registration_status']=='REASSIGNED_TODO');wid=next(w for w in row['corrected_source_work_ids'] if w in self.work);del self.work[wid];self.reject()
 def test_changed_payload(self):self.work[self.registered()]['source_sha256']='0'*64;self.reject()
 def test_done_without_evidence(self):self.work[self.registered()]['artifact']='-';self.reject()
 def test_added_owner_without_assignment(self):
  row=copy.deepcopy(self.work[self.registered()]);row['work_id']='invented';row['wbs_task']='99.999';self.work['invented']=row;self.reject()
 def test_completed_successor_not_forced_todo(self):
  sources={r['source_id'] for r in self.review['reviews'] if r['registration_status']=='REASSIGNED_TODO'}
  row=next(w for w in self.work.values() if w['source_id'] in sources and w['status']=='TODO');row.update(status='DONE',owner='test-control',artifact='fixture proof',test='fixture check',result='fixture pass');self.check()
 def test_exact_original_checker(self):checker.check_historical()
 def test_damaged_historical_archive(self):
  with tempfile.TemporaryDirectory()as directory:
   target=pathlib.Path(directory)
   for n in ('historical-pin.json','historical-checkpoint.tar.gz'):shutil.copy2(checker.RECOVERY/n,target/n)
   p=target/'historical-checkpoint.tar.gz';p.write_bytes(p.read_bytes()+b'damaged')
   with patch.object(checker,'RECOVERY',target),self.assertRaises(AssertionError):checker.check_historical()
 def test_wrong_historical_commit(self):
  with tempfile.TemporaryDirectory()as directory:
   target=pathlib.Path(directory);p=json.loads((checker.RECOVERY/'historical-pin.json').read_text());p['original_commit']='0'*40;(target/'historical-pin.json').write_text(json.dumps(p))
   with patch.object(checker,'RECOVERY',target),self.assertRaises(AssertionError):checker.check_historical()
if __name__=='__main__':unittest.main()
