import copy,importlib.util,json,pathlib,tarfile,unittest
ROOT=pathlib.Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('verifier',ROOT/'verify.py');v=importlib.util.module_from_spec(s);s.loader.exec_module(v)
class Admission(unittest.TestCase):
 def test_archive(self):v.verify(ROOT)
 def test_runtime_controls(self):
  with tarfile.open(ROOT/'evidence.tar.gz')as t:
   read=lambda n:json.load(t.extractfile('oncanvas-ci/'+n))
   baseline=[read(n)for n in ['results.json','landscape-results.json','portrait-flag-negative.json','summary.json','final-seal.json']]
  mutations=[lambda x:x[0][0].update(returncode=1),lambda x:x[0][1].update(ok_tap_cases=12),lambda x:x[1][0].update(portrait_flag=True),lambda x:x[1][1].update(returncode=1),lambda x:x[2].update(returncode=0),lambda x:x[2].update(stdout='',stderr=''),lambda x:x[3].update(production_changes=1),lambda x:x[3].update(default_ci_fixed_resolution=True),lambda x:x[4].update(unchanged=False)]
  for i,mutate in enumerate(mutations):
   with self.subTest(control=i):
    x=copy.deepcopy(baseline);mutate(x)
    with self.assertRaises(AssertionError):v.validate(*x)
 def test_source_controls(self):
  n='app/tests/test-painter-smudge-ui.cpp'
  with tarfile.open(ROOT/'evidence.tar.gz')as t:
   old=t.extractfile('oncanvas-ci/base/'+n).read().decode();new=t.extractfile('oncanvas-ci/'+n).read().decode()
  for bad in [new.replace('"painter-canvas-ui", TRUE','"painter-canvas-ui", FALSE'),new.replace('if (g_strcmp0 (g_getenv ("GIMP_TEST_REQUIRE_PORTRAIT"), "1") == 0) {','if (true) {'),new.replace('g_assert_true (gimp_painter_canvas_ui_get_visible (oncanvas_shell));','/* no visible check */')]:
   with self.assertRaises(AssertionError):v.validate_source(old,bad,n)
if __name__=='__main__':unittest.main()
