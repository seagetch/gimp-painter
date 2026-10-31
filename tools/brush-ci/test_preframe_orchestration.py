#!/usr/bin/env python3
"""Static/synthetic handoff checks. Never install or execute GDB/native UI."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
class Integration(unittest.TestCase):
 def test_separate_sanitizer_step_after_unchanged_native_driver(self):
  workflow=(ROOT/'.github/workflows/painter-native-ui.yml').read_text()
  section=workflow.split('  brush-native-popup:',1)[1].split('  babl-owned-doc-proof:',1)[0]
  original='''      - name: Build and test additive Brush native portrait UI
        id: native_driver
        run: |
          cd "$PAINTER_CI_SOURCE"
          bash tools/brush-ci/run_brush_ci.sh "$GITHUB_WORKSPACE/ci-results" '${{ matrix.target }}'
'''
  self.assertIn(original,section)
  start=section.index('      - name: Isolated post-acceptance')
  self.assertGreater(start,section.index(original))
  self.assertLess(start,section.index('      - name: Preserve focused verification evidence'))
  diagnostic=section[start:section.index('      - name: Preserve bounded APT',start)]
  for guard in ("always()","matrix.target == 'sanitizer'","steps.runtime_seal.outcome == 'success'","steps.native_driver.outcome == 'failure'"):
   self.assertIn(guard,diagnostic)
  self.assertNotIn('continue-on-error',section)
  self.assertIn('timeout-minutes: 10',diagnostic)
 def test_handoff_after_original_runtime_cannot_change_its_exit(self):
  source=(HERE/'run_brush_ci.sh').read_text()
  start=source.index('# Nonacceptance handoff only AFTER')
  self.assertGreater(start,source.index('bash "$here/run_brush_runtime.sh"'))
  self.assertGreater(start,source.index('verify_source || failed=1'))
  body=source[start:source.index("printf 'Brush %s automated checks",start)]
  self.assertIn('--source "$src" --build "$build" --run "$run"',body)
  for original in (0,1):
   for capture in (0,2):
    with tempfile.TemporaryDirectory() as t:
     pre=f'set -euo pipefail\nhere=/bundle;src=/source;build=/build;run={t};out={t};mode=sanitizer;failed={original}\npython3() {{ return {capture}; }}\n'
     p=subprocess.run(['bash','-c',pre+body],capture_output=True,text=True)
     self.assertEqual(p.returncode,original,p.stderr)
     if capture:
      j=json.loads((Path(t)/'preframe-handoff-unavailable.json').read_text());self.assertIs(j['acceptance_eligible'],False)
 def test_exact_same_sanitizer_settings_no_suppression(self):
  original=(HERE/'run_brush_runtime.sh').read_text()
  diagnostic=(HERE/'run_preframe_diagnostic.sh').read_text()
  line=next(x.strip() for x in original.splitlines() if x.strip().startswith('export ASAN_OPTIONS='))
  self.assertIn(line,diagnostic);self.assertIn('unset LSAN_OPTIONS',diagnostic)
  for prohibited in ('detect_leaks=0','halt_on_error=0','suppressions=','exitcode=0','ptrace_scope','setcap','seccomp=','--privileged'):
   self.assertNotIn(prohibited,diagnostic)
 def test_exact_portrait_registration_and_private_raw_logs(self):
  s=(HERE/'run_preframe_diagnostic.sh').read_text()
  self.assertIn("--server-args='-screen 0 1080x1920x24 -nolisten tcp'",s)
  self.assertIn('GDK_SCALE=1 GDK_DPI_SCALE=1 GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas',s)
  self.assertIn("--test-args '-p /brush-thumbnail-native-popup/private-target-release' painter-compact-options",s)
  self.assertIn('> "$private/meson-console.log" 2>&1',s)
  self.assertIn('> "$private/apt-update.log" 2>&1',s)
  self.assertIn('> "$private/apt-install.log" 2>&1',s)
  self.assertNotIn('cp ',s);self.assertNotIn('tee ',s)
  self.assertIn('ulimit -c 0',s)
 def test_verify_before_install_and_restore(self):
  s=(HERE/'run_preframe_diagnostic.sh').read_text()
  first=s.index('verify --receipt');install=s.index('apt-get update');restore=s.index('source "$src/tools/linux-debian13-env.sh"')
  self.assertLess(first,install);self.assertLess(install,restore)
  self.assertIn('apt-get install --no-install-recommends -y gdb',s)
  self.assertLess(s.index('stage=post-install-identity'),restore)
  self.assertEqual(s.count('apt-get install'),1)
  self.assertIn('timeout --kill-after=10s 60s apt-get update',s)
  self.assertIn('timeout --kill-after=10s 180s apt-get install',s)
  self.assertEqual(s.count('timeout=10'),2)
 def test_missing_handoff_is_inconclusive_without_install(self):
  with tempfile.TemporaryDirectory() as t:
   p=subprocess.run(['bash',str(HERE/'run_preframe_diagnostic.sh'),t],env={**os.environ,'GITHUB_ACTIONS':'true'},capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stderr)
   report=json.loads((Path(t)/'preframe-diagnostic/status.json').read_text())
   self.assertEqual(report['stage'],'identity-unavailable');self.assertIs(report['acceptance_eligible'],False)
   self.assertTrue(report['original_driver_exit_unchanged'])
 def test_nonhosted_guard_refuses_before_work(self):
  with tempfile.TemporaryDirectory() as t:
   p=subprocess.run(['bash',str(HERE/'run_preframe_diagnostic.sh'),t],env={**os.environ,'GITHUB_ACTIONS':'false'},capture_output=True,text=True)
   self.assertEqual(p.returncode,2);self.assertFalse(list(Path(t).iterdir()))
 def test_shell_syntax(self):
  for name in ('run_brush_ci.sh','run_preframe_diagnostic.sh'):
   subprocess.run(['bash','-n',str(HERE/name)],check=True)
if __name__=='__main__':unittest.main(verbosity=2)
