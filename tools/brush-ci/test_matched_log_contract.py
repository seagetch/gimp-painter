#!/usr/bin/env python3
"""Regression contracts for exact Meson wrapper-log handling; no native acceptance."""
import importlib.util,json,os,subprocess,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
def load(name):
 spec=importlib.util.spec_from_file_location(name,HERE/(name+'.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
D=load('matched_driver_state');P=load('matched_provenance')
class Contract(unittest.TestCase):
 def test_shell_eof_status_identity(self):
  for phase,initial in [('measure',0),('one',14)]:
   s='order=%d; status_order=$order; while IFS=" " read -r cell rep order kind; do :; done <<< "current-normal 1 14 fidelity"; printf "%%s:%%s" "$order" "$status_order"'%initial
   self.assertEqual(subprocess.check_output(['bash','-c',s],text=True),':'+str(initial))
 def test_exact_driver_path_and_no_native_override(self):
  s=(HERE/'run_matched_preclick.sh').read_text()
  self.assertEqual(s.count('--log "$build/meson-logs/$label-python3.json"'),3)
  self.assertNotIn('--log "$build/meson-logs/$label.json"',s)
  self.assertIn('--wrapper "python3 $here/matched_preclick_exec.py"',s)
  self.assertIn('native == 0 && collection == 0 && archived == 0 && postflight == 0 && log_metadata == 0',s)
  self.assertIn('--code "$result" --order "$status_order"',s)
 def test_bounded_receipt(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);logs=root/'cells/baseline-normal/build/meson-logs';logs.mkdir(parents=True);out=root/'out';out.mkdir();p=logs/'matched-01-python3.json'
   for verdict,code in [('OK',0),('FAIL',7),('FAIL',-6),('EXPECTEDFAIL',-5)]:
    p.write_text(json.dumps({'result':verdict,'returncode':code,'env':{'SECRET':'must-not-export'},'stdout':'private-output'})+'\n')
    self.assertEqual(D.log_evidence(root,'baseline-normal',1,out),0)
    receipt=json.loads((out/'log-evidence-01.json').read_text())
    self.assertEqual((receipt['meson_result'],receipt['native_returncode']),(verdict,code))
    self.assertNotIn('must-not-export',json.dumps(receipt));self.assertNotIn('private-output',json.dumps(receipt))
   p.unlink();(logs/'matched-01.json').write_text('{}')
   self.assertEqual(D.log_evidence(root,'baseline-normal',1,out),1)
   self.assertEqual(json.loads((out/'log-evidence-01.json').read_text())['failure_category'],'expected-log-missing')
   p.symlink_to(logs/'matched-01.json');self.assertEqual(D.log_evidence(root,'baseline-normal',1,out),1);p.unlink()
   for data in ['{}','{"result":"OK","returncode":0,"returncode":1}','{}\n{}','{"result":"OK","returncode":true}']:
    p.write_text(data);self.assertEqual(D.log_evidence(root,'baseline-normal',1,out),1)
 def test_fifo_rejected_without_blocking(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);logs=root/'cells/baseline-normal/build/meson-logs';logs.mkdir(parents=True);out=root/'out';out.mkdir()
   os.mkfifo(logs/'matched-01-python3.json')
   code="import sys;sys.path.insert(0,sys.argv[1]);from pathlib import Path;from matched_driver_state import log_evidence;raise SystemExit(log_evidence(Path(sys.argv[2]),'baseline-normal',1,Path(sys.argv[3])))"
   r=subprocess.run(['python3','-B','-c',code,str(HERE),str(root),str(out)],timeout=2)
   self.assertEqual(r.returncode,1)
   self.assertEqual(json.loads((out/'log-evidence-01.json').read_text())['failure_category'],'invalid-or-unreadable-log')
 def test_exact_meson_default_options(self):
  defaults={'ASAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1','UBSAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1:print_stacktrace=1','MSAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1:print_stacktrace=1'}
  for cell in P.CELLS:
   good=dict(defaults)
   if cell.endswith('sanitizer'):good.update(ASAN_OPTIONS='halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
   P.verify_sanitizer_runtime_options(good,cell)
   for key in defaults:
    for value in (None,'','detect_leaks=0',good[key]+':suppressions=x'):
     bad=dict(good)
     if value is None:del bad[key]
     else:bad[key]=value
     with self.assertRaises(P.EvidenceError):P.verify_sanitizer_runtime_options(bad,cell)
   with self.assertRaises(P.EvidenceError):P.verify_sanitizer_runtime_options(dict(good,LSAN_OPTIONS=''),cell)
if __name__=='__main__':unittest.main()
