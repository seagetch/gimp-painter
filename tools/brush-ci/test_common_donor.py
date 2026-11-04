#!/usr/bin/env python3
"""Pure source/negative tests: no Git refs, GUI, CI, or baseline execution."""
import copy,json,shutil,tempfile,unittest,subprocess
from pathlib import Path
import verify_common_donor as gate
HERE=Path(__file__).resolve().parent;DRAFT=HERE.parent
class DonorProof(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();root=Path(self.temp.name);self.donor=root/'donor';self.candidate=root/'candidate'
  self.pins=json.loads((HERE/'pins.json').read_text())
  self.before=(HERE/'common306-header-routes.reference.json').read_bytes()
  if (DRAFT/'ci-donor').is_dir():
   shutil.copytree(DRAFT/'ci-donor',self.donor);shutil.copytree(self.donor,self.candidate)
   current=DRAFT/'probe/export'
  else:
   current=HERE.parents[1]
   names=list(gate.PROTECTED)+[gate.ROUTES,'app/tools/gimppaintertogglepopup.h','app/widgets/gimpblobeditor.h']
   for name in names:
    p=self.donor/name;p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes(subprocess.check_output(['git','-C',str(current),'show',self.pins['accepted_task307_baseline_commit']+':'+name]))
   shutil.copytree(self.donor,self.candidate)
  (self.candidate/gate.ROUTES).write_bytes((current/gate.ROUTES).read_bytes())
  name='app/tools/gimppaintertogglepopup.h';(self.candidate/name).write_bytes((current/name).read_bytes())
 def tearDown(self):self.temp.cleanup()
 def check(self):return gate.payload(self.candidate,self.donor,self.pins,self.before)
 def test_current_probe_payload_equality_not_acceptance(self):
  r=self.check();self.assertTrue(r['non_brush_prerequisite_entries_identical']);self.assertTrue(r['donor_has_only_two_306_prerequisite_additions']);self.assertEqual(r['common_case_count'],58);self.assertEqual(r['required_common_png_count'],16)
 def test_gate6_and_both_scales_required(self):
  r=self.check();self.assertEqual(len(r['overlay_paint_gate_case_ids']),6);self.assertEqual(r['overlay_paint_gate_scales'],[1,2])
  self.pins['common_overlay_gate_case_count']=5
  with self.assertRaisesRegex(ValueError,'gate6/both-scales'):self.check()
  self.pins['common_overlay_gate_case_count']=6;self.pins['common_overlay_gate_scales']=[1]
  with self.assertRaisesRegex(ValueError,'gate6/both-scales'):self.check()
 def test_historical_identity_cannot_be_retargeted_to_candidate(self):
  self.pins['historical_dockbook_diagnostic']['source_commit']=self.pins['observed_task307_ci_source_commit']
  with self.assertRaisesRegex(ValueError,'historical diagnostic identity drift'):self.check()
 def test_null_and_draft_pins_block_runtime_routing(self):
  with self.assertRaisesRegex(ValueError,'BLOCKED'):gate.commits(self.pins)
  self.pins.update(accepted_task307_baseline_commit='a'*40,candidate_final_commit='b'*40,draft_only=True)
  with self.assertRaisesRegex(ValueError,'draft_only'):gate.commits(self.pins)
 def test_every_protected_candidate_file_mismatch_rejected(self):
  for name in gate.PROTECTED:
   p=self.candidate/name;before=p.read_bytes();p.write_bytes(before+b'\nchanged\n')
   with self.assertRaisesRegex(ValueError,'drift'):self.check()
   p.write_bytes(before)
 def test_matching_but_unreviewed_harness_changes_rejected(self):
  name='app/tests/test-common-layer-popup.cpp'
  for root in (self.candidate,self.donor):
   p=root/name;p.write_bytes(p.read_bytes()+b'\nchanged\n')
  with self.assertRaisesRegex(ValueError,'renewed control design'):self.check()
 def test_inherited_route_changes_extra_missing_duplicate_rejected(self):
  p=self.candidate/gate.ROUTES;original=json.loads(p.read_text());variants=[]
  a=copy.deepcopy(original);a['routes'][0]['review']='changed';variants.append(a)
  a=copy.deepcopy(original);a['routes'].append({'path':'extra.h'});variants.append(a)
  a=copy.deepcopy(original);a['routes'].pop();variants.append(a)
  a=copy.deepcopy(original);a['routes'].append(a['routes'][-1]);variants.append(a)
  for a in variants:
   p.write_text(json.dumps(a))
   with self.assertRaisesRegex(ValueError,'prerequisites differ'):self.check()
 def test_donor_cannot_import_brush_prerequisite(self):
  (self.donor/gate.ROUTES).write_bytes((self.candidate/gate.ROUTES).read_bytes())
  with self.assertRaisesRegex(ValueError,'inventory changed'):self.check()
 def test_both_common_prerequisite_headers_reject_other_changes(self):
  for name in ('app/tools/gimppaintertogglepopup.h','app/widgets/gimpblobeditor.h'):
   p=self.candidate/name;old=p.read_bytes();p.write_bytes(old+b'\nchanged\n')
   with self.assertRaisesRegex(ValueError,'prerequisite header differs'):self.check()
   p.write_bytes(old)
 def test_shared_header_requires_exact_reviewed_brush_insertion(self):
  name='app/tools/gimppaintertogglepopup.h'
  (self.candidate/name).write_bytes((self.donor/name).read_bytes())
  with self.assertRaisesRegex(ValueError,'reviewed Brush-only insertion'):self.check()
 def test_protected_set_cannot_be_reduced(self):
  self.pins['common_protected_file_sha256'].pop(gate.PROTECTED[0])
  with self.assertRaisesRegex(ValueError,'file set changed'):self.check()
 def test_symlink_source_rejected(self):
  p=self.candidate/gate.PROTECTED[0];p.unlink();p.symlink_to(self.donor/gate.PROTECTED[0])
  with self.assertRaisesRegex(ValueError,'symlink'):self.check()
if __name__=='__main__':unittest.main(verbosity=2)
