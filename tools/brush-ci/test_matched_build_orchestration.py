#!/usr/bin/env python3
"""Pure shell contract tests; no Meson, compiler, native test or display."""
import pathlib, subprocess, tempfile, unittest
HERE=pathlib.Path(__file__).resolve().parent
class BuildFailureBoundary(unittest.TestCase):
 def setUp(self):
  self.raw=(HERE/'run_matched_preclick.sh').read_text()
  start=self.raw.index('      cell_stage=environment\n')
  end=self.raw.index('      src="$run/cells/$cell/source";',start)
  self.traps=self.raw[start:end]
 def execute(self,code,collector_code=0):
  with tempfile.TemporaryDirectory() as temp:
   log=pathlib.Path(temp)/'calls'
   script='''set -euo pipefail
here=/known; run=/private; out=/artifact; cell=baseline-sanitizer
python3() { printf '%s\\n' "$*" >> "$LOG"; if [[ "$*" == *'--state finished'* ]]; then return "$COLLECTOR_CODE"; fi; }
(
'''+self.traps+'''cell_stage=compile
build_checkpoint
bash -c 'exit "$1"' fixture "$TOOL_CODE"
cell_stage=complete
build_checkpoint
)
'''
   import os
   env=dict(os.environ,LOG=str(log),COLLECTOR_CODE=str(collector_code),TOOL_CODE=str(code))
   result=subprocess.run(['bash','-c',script],env=env,capture_output=True,text=True)
   return result.returncode,log.read_text().splitlines()
 def test_original_failure_codes_preserved(self):
  for code in (1,2,77,124,137):
   with self.subTest(code=code):
    status,lines=self.execute(code)
    self.assertEqual(status,code)
    self.assertIn('--stage compile --state finished --exit-code '+str(code),lines[-1])
    self.assertFalse(any('--stage complete' in line for line in lines))
 def test_collector_failure_never_waives_native_failure(self):
  status,lines=self.execute(77,5);self.assertEqual(status,77)
 def test_collector_failure_turns_success_into_failure(self):
  status,lines=self.execute(0,5);self.assertEqual(status,1)
 def test_success_record_is_complete(self):
  status,lines=self.execute(0);self.assertEqual(status,0)
  self.assertIn('--stage complete --state finished --exit-code 0',lines[-1])
 def test_all_build_stages_checkpointed_in_order(self):
  stages=['environment','configure','target-selection','compile','catalog-stage','build-identity','overlay-check','complete']
  positions=[self.raw.index('cell_stage='+s) for s in stages]
  self.assertEqual(positions,sorted(positions))
  for stage in stages[1:]:self.assertIn('cell_stage='+stage+'; build_checkpoint',self.raw)
 def test_no_raw_artifact_copy_or_command_dump(self):
  for forbidden in ('set -x','BASH_COMMAND','cp "$run/logs','\n      env >','printenv'):
   self.assertNotIn(forbidden,self.raw)
if __name__=='__main__':unittest.main()
