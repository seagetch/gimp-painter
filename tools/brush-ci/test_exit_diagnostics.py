#!/usr/bin/env python3
"""Synthetic/source-only diagnostic policy tests; never execute native code."""
import contextlib,io,json,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
import check_brush_ci as gate
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
class ExitDiagnostics(unittest.TestCase):
 def dispatch(self,mode,fail=False):
  raw=(HERE/'run_brush_runtime.sh').read_text();block=raw.split('# BEGIN fresh-process causality diagnostics',1)[1].split('# END fresh-process causality diagnostics.',1)[0];block=block[block.index('\n'):]
  script='set -euo pipefail\nmode=$1;draft=/bundle;failed=0\nrun_group() { printf "%s|%s|%s|%s\\n" "$1" "$2" "$3" "$4"; '+('return 1;' if fail else 'return 0;')+' }\n'+block+'\nexit "$failed"\n'
  p=subprocess.run(['bash','-c',script,'stub',mode],text=True,capture_output=True);return p,p.stdout.splitlines()
 def test_normal_three_fresh_labels(self):
  p,lines=self.dispatch('positive');self.assertEqual(p.returncode,0);self.assertEqual(len(lines),3);self.assertEqual(len({x.split('|')[0] for x in lines}),3);self.assertIn('private-target-normal|1|/brush-thumbnail-native-popup/private-target-release',lines[0])
 def test_sanitizer_two_baselines_only(self):
  p,lines=self.dispatch('sanitizer');self.assertEqual(p.returncode,0);self.assertEqual(len(lines),2);self.assertTrue(all('|1|/brush-exit-diagnostic/' in x for x in lines))
 def test_failures_are_retained_and_other_baselines_still_run(self):
  for mode,count in [('positive',3),('sanitizer',2)]:
   p,lines=self.dispatch(mode,True);self.assertEqual(p.returncode,1);self.assertEqual(len(lines),count)
 def test_baseline_red_untouched(self):
  p,lines=self.dispatch('baseline-red');self.assertEqual(p.returncode,0);self.assertFalse(lines)
 def check(self,manifest,report=None):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'log').write_text('{}\n');(p/'manifest').write_text(json.dumps(manifest));module=mock.Mock();module.read_results.return_value=report or {'passed':True,'errors':[],'tests':[]}
   argv=['gate','--log',str(p/'log'),'--case-manifest',str(p/'manifest'),'--common-checker','unused','--output',str(p/'result')]
   with mock.patch.object(sys,'argv',argv),mock.patch.object(gate,'load_common',return_value=module),contextlib.redirect_stdout(io.StringIO()):code=gate.main()
   return code,json.loads((p/'result').read_text())
 def test_exact_three_manifests_are_nonacceptance(self):
  for p in HERE.glob('diagnostic-*.json'):
   code,result=self.check(json.loads(p.read_text()));self.assertEqual(code,0);self.assertTrue(result['diagnostic_only']);self.assertIs(result['acceptance_eligible'],False)
 def test_unlabelled_exit_diagnostic_rejected(self):
  j=json.loads((HERE/'diagnostic-harness-only.json').read_text());j.pop('diagnostic_only');self.assertEqual(self.check(j)[0],1)
 def test_wrong_or_multi_case_diagnostic_rejected(self):
  j=json.loads((HERE/'diagnostic-harness-only.json').read_text())
  for ids in [['/wrong'],['/brush-exit-diagnostic/harness-only','/brush-exit-diagnostic/fixture-no-popup']]:j['case_ids']=ids;self.assertEqual(self.check(j)[0],1)
 def test_diagnostic_leak_still_fails(self):
  j=json.loads((HERE/'diagnostic-harness-only.json').read_text());report={'passed':True,'errors':[],'tests':[{'stdout':'','stderr':'ERROR: LeakSanitizer: detected memory leaks'}]};self.assertEqual(self.check(j,report)[0],1)
 def test_original62_and_brush21_namespace_exclusion(self):
  groups=json.loads((HERE/'positive-groups.json').read_text())['groups'];group=next(g for g in groups if g['label']=='painter-compact-options');self.assertEqual(group['selector'],'/painter-compact-options');self.assertEqual(len(group['case_ids']),62)
  brush=json.loads((HERE/'brush-cases.json').read_text())['case_ids'];self.assertEqual(len(brush),21);self.assertFalse(any('/brush-exit-diagnostic/' in x for x in group['case_ids']+brush))
 def test_ownership_cases_exact_and_missing_case_rejected(self):
  for name in ['color-config-cases.json','display-renderer-cases.json','canvas-cases.json']:
   j=json.loads((HERE/name).read_text());self.assertEqual(self.check(j)[0],0)
   j['case_ids']=j['case_ids'][:1];self.assertEqual(self.check(j)[0],1)
 def test_fixture_baseline_never_opens_popup(self):
  s=(ROOT/'app/tests/test-painter-brush-thumbnail-popup.inc').read_text();body=s.split('brush_exit_diagnostic_fixture_no_popup (void)',1)[1].split('\n}',1)[0];self.assertIn('brush_popup_fixture ()',body);self.assertIn('brush_popup_finish (&f)',body);self.assertNotIn('brush_popup_open',body)
if __name__=='__main__':unittest.main(verbosity=2)
