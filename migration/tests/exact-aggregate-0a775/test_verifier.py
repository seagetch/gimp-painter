import copy,importlib.util,json,pathlib,tarfile,unittest
ROOT=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('verify',ROOT/'verify.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
class Admission(unittest.TestCase):
 def test_archive(self):v.verify(ROOT)
 def test_runtime_controls(self):
  with tarfile.open(ROOT/'evidence.tar.gz')as t:
   read=lambda n:json.load(t.extractfile('exact-'+v.COMMIT+'/'+n))
   base=[read('results.json'),read('aggregate-summary.json'),read('extra-ui-results.json'),read('graph-ui-result.json')]
  mutants=[lambda x:x[0][0].update(result='FAIL'),lambda x:x[0].pop(),lambda x:x[1].update(completed_ok_tap_cases=521),lambda x:x[1].update(selected_extra_total=89),lambda x:x[2][0].update(returncode=1),lambda x:x[3][0].update(result='FAIL'),lambda x:x[1].update(source_commit='bad'),lambda x:x[1].update(non_tap_targets=[]),lambda x:next(r for r in x[0]if r['name'].endswith(' / event-compression-order')).update(stdout='')]
  for i,mutate in enumerate(mutants):
   with self.subTest(control=i):
    x=copy.deepcopy(base);mutate(x)
    with self.assertRaises(AssertionError):v.validate(*x)
 def test_erratum_controls(self):
  with tarfile.open(ROOT/'evidence.tar.gz')as t:r=json.load(t.extractfile('exact-'+v.COMMIT+'/historical-tap-count-correction.json'))['receipts'][1]
  with tarfile.open(ROOT.parent/'exact-aggregate-55bf/evidence.tar.gz')as t:rows=json.load(t.extractfile('exact-'+r['source_commit']+'/results.json'))
  for mutate in [lambda r:r.update(corrected_executed_non_skip_tap=478),lambda r:r['independent_coverage'][0].update(independent_completed_runs=[]),lambda r:r['independent_coverage'][0]['independent_completed_runs'][0].update(result='FAIL')]:
   x=copy.deepcopy(r);mutate(x)
   with self.assertRaises(AssertionError):v.validate_erratum(x,rows)
if __name__=='__main__':unittest.main()
