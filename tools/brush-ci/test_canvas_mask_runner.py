from pathlib import Path
import re
import subprocess
import unittest
import canvas_mask_provider as m

class Runner(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.path=Path(__file__).with_name('run_canvas_mask_control.sh');cls.text=cls.path.read_text()
 def test_shell_syntax_only(self):
  self.assertEqual(subprocess.run(['bash','-n',str(self.path)],capture_output=True).returncode,0)
 def test_exact_sanitizer_options(self):
  self.assertIn('export ASAN_OPTIONS='+m.ASAN,self.text);self.assertIn('export UBSAN_OPTIONS='+m.UBSAN,self.text)
  self.assertIn('unset ASAN_OPTIONS UBSAN_OPTIONS',self.text);self.assertIn('NO_AT_BRIDGE LSAN_OPTIONS',self.text)
 def test_fresh_profile_per_invocation(self):
  self.assertIn('profile="$run/profile-canvas-mask-$mode-$label"',self.text)
  self.assertIn('mkdir "$profile"',self.text);self.assertNotIn('mkdir -p "$profile"',self.text)
 def test_exact_leaf_or_unfiltered_full(self):
  self.assertIn("[[ $label == fixed-full107 ]] || args=(--test-args '-p "+m.CASE+"')",self.text)
 def test_normal_only_controls(self):
  self.assertIn('[[ $mode != normal ]] || labels+=(old-focused fixed-full107)',self.text)
 def test_original_failure_preserved(self):
  self.assertIn('[[ $status == 0 ]] || failed=1',self.text);self.assertIn('exit "$failed"',self.text)
 def test_per_run_finite_observation_after_status(self):
  self.assertIn('--shell-exit "$status"',self.text)
  self.assertIn('--output "$evidence/$label-status.json" || failed=1',self.text)
  self.assertLess(self.text.index('[[ $status == 0 ]] || failed=1'),self.text.index('--observe-log'))
 def test_private_logs_only(self):
  self.assertIn('> "$run/canvas-mask-$mode-$label-console.log" 2>&1',self.text)
  self.assertNotIn('cp ',self.text);self.assertNotIn('cat "$run/',self.text)
 def test_fixture_overrides_unset(self):
  words=set(' '.join(line.strip()[6:] for line in self.text.splitlines() if line.strip().startswith('unset ')).split())
  self.assertTrue(set(m.FORBIDDEN_RUNTIME)<=words)
 def test_portrait_xvfb(self):
  self.assertIn('-screen 0 1080x1920x24 -nolisten tcp',self.text)
 def test_provider_precedes_execution(self):
  self.assertLess(self.text.index('--provider --source'),self.text.index('xvfb-run'))
  self.assertGreater(self.text.index('args=(--aggregate'),self.text.index('xvfb-run'))

if __name__=='__main__':unittest.main()
