#!/usr/bin/env python3
import copy,json,pathlib,tarfile,unittest
import verify
class Controls(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  with tarfile.open(pathlib.Path(__file__).with_name('evidence.tar.gz')) as t:cls.data=[json.loads(t.extractfile('exact-'+verify.COMMIT+'/'+n).read())for n in ['results.json','aggregate-summary.json','extra-ui-results.json','oncanvas-scope-audit.json']]
 def test_positive(self):verify.validate(*copy.deepcopy(self.data))
 def test_semantic_controls(self):
  mutations=[lambda r,s,e,a:r[0].__setitem__('result','FAIL'),lambda r,s,e,a:r.pop(),lambda r,s,e,a:s.__setitem__('completed_ok_tap_cases',479),lambda r,s,e,a:s.__setitem__('selected_extra_total',55),lambda r,s,e,a:e[0].__setitem__('returncode',1),lambda r,s,e,a:a.__setitem__('default_enabled',True),lambda r,s,e,a:a.__setitem__('missing_explicit_enable',[]),lambda r,s,e,a:a.__setitem__('not_blanket_oncanvas_44_targets',False),lambda r,s,e,a:a.__setitem__('unused_environment_not_used_as_proof',False)]
  for i,mutate in enumerate(mutations):
   with self.subTest(control=i):
    data=copy.deepcopy(self.data);mutate(*data)
    with self.assertRaises(AssertionError):verify.validate(*data)
if __name__=='__main__':unittest.main()
