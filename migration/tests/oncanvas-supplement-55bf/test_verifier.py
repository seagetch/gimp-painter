import copy,importlib.util,json,pathlib,tarfile,unittest
ROOT=pathlib.Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('verifier',ROOT/'verify.py');v=importlib.util.module_from_spec(s);s.loader.exec_module(v)
class Admission(unittest.TestCase):
 def test_archive(self):v.verify(ROOT)
 def test_semantic_controls(self):
  with tarfile.open(ROOT/'evidence.tar.gz')as t:
   read=lambda n:json.load(t.extractfile('oncanvas-supplement/'+n))
   baseline=[read('results.json'),read('summary.json'),read('final-seal.json')]
  mutations=[lambda x:x[0][0].update(returncode=1),lambda x:x[0][1].update(ok_tap_cases=12),lambda x:x[0][2].update(stdout=''),lambda x:x[1].update(production_commit='bad'),lambda x:x[1].update(production_changes=1),lambda x:x[1].update(whole_44_oncanvas_claim=True),lambda x:x[1].update(actual_app_config_and_associated_shell_visible_asserted=False),lambda x:x[2].update(unchanged=False),lambda x:x[1].update(new_sanitizer_claim=True)]
  for i,mutate in enumerate(mutations):
   with self.subTest(control=i):
    x=copy.deepcopy(baseline);mutate(x)
    with self.assertRaises(AssertionError):v.validate(*x)
if __name__=='__main__':unittest.main()
