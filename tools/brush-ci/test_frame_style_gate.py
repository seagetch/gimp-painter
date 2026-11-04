#!/usr/bin/env python3
"""Synthetic exact-seven gate contracts; not native GTK evidence."""
import contextlib,io,json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import check_brush_ci as gate
HERE=Path(__file__).resolve().parent
IDS=json.loads((HERE/'frame-style-cases.json').read_text())['case_ids']
class FrameGate(unittest.TestCase):
 def run_gate(self,actual=None,ids=None,result='OK',exitcode=0,stderr='',skip=False):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);actual=IDS if actual is None else actual
   tap='TAP version 14\n1..%d\n'%len(actual)+''.join('ok %d %s%s\n'%(i,x,' # SKIP missing' if skip else '') for i,x in enumerate(actual,1))
   (root/'log').write_text(json.dumps(dict(name='gimp:libgimpwidgets / gimp-frame-style-idempotence',result=result,returncode=exitcode,duration=1,stdout=tap,stderr=stderr))+'\n')
   (root/'cases').write_text(json.dumps(dict(schema_version=1,test='gimp-frame-style-idempotence',case_ids=IDS if ids is None else ids)))
   with patch.object(sys,'argv',['gate','--log',str(root/'log'),'--output',str(root/'out'),'--common-checker',str(HERE.parent.parent/'tools/check_painter_ui_ci.py'),'--case-manifest',str(root/'cases')]),contextlib.redirect_stdout(io.StringIO()):
    return gate.main()
 def test_exact_seven_pass(self):self.assertEqual(self.run_gate(),0)
 def test_zero_rejected(self):self.assertNotEqual(self.run_gate(actual=[]),0)
 def test_six_rejected(self):self.assertNotEqual(self.run_gate(actual=IDS[:-1]),0)
 def test_wrong_leaf_rejected(self):self.assertNotEqual(self.run_gate(actual=IDS[:-1]+['/gimp-frame-style/wrong']),0)
 def test_manifest_cannot_waive_missing_case(self):self.assertNotEqual(self.run_gate(actual=IDS[:-1],ids=IDS[:-1]),0)
 def test_skip_rejected(self):self.assertNotEqual(self.run_gate(skip=True),0)
 def test_exit77_rejected(self):self.assertNotEqual(self.run_gate(result='SKIP',exitcode=77),0)
 def test_leak_rejected(self):self.assertNotEqual(self.run_gate(stderr='ERROR: LeakSanitizer: detected memory leaks'),0)
 def test_child_error_rejected(self):self.assertNotEqual(self.run_gate(stderr='GLib-GObject-CRITICAL: broken fixture'),0)
 def test_warning_rejected(self):self.assertNotEqual(self.run_gate(stderr='(frame:123): GLib-GObject-WARNING **: invalid type registration'),0)
 def test_informational_output_allowed(self):self.assertEqual(self.run_gate(stderr='dbus-daemon: Successfully activated service org.a11y.Bus'),0)
 def test_nonzero_rejected(self):self.assertNotEqual(self.run_gate(result='FAIL',exitcode=1),0)
 def test_timeout_rejected(self):self.assertNotEqual(self.run_gate(result='TIMEOUT',exitcode=-15),0)
 def test_test_registration_matches_seven_source_leaves(self):
  import re
  source=(HERE.parents[1]/'libgimpwidgets/test-frame-style-idempotence.c').read_text()
  self.assertEqual(re.findall(r'g_test_add_(?:func|data_func) \("([^"]+)"',source),IDS)
if __name__=='__main__':unittest.main()
