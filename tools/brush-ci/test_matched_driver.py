#!/usr/bin/env python3
"""Synthetic filesystem/shell contracts only; no native display or build."""
import hashlib,json,os,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import matched_driver_state as m
HERE=Path(__file__).resolve().parent
SOURCE=HERE.parents[1]
class OwnedState(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.base=Path(self.tmp.name)
  self.run=self.base/'painter-matched-preclick.Abc12345';self.run.mkdir(mode=0o700)
  for name in ('cells','profiles','retained-output'):(self.run/name).mkdir()
  self.cell='baseline-normal';self.build=self.run/'cells'/self.cell/'build';(self.build/'app/tests').mkdir(parents=True)
  self.env=patch.dict(os.environ,{'RUNNER_TEMP':str(self.base)});self.env.start();self.addCleanup(self.env.stop)
 def test_owned_run(self):self.assertEqual(m.owned_run(self.run),self.run)
 def test_public_or_wrong_root_rejected(self):
  self.run.chmod(0o755)
  with self.assertRaises(ValueError):m.owned_run(self.run)
 def test_symlink_run_rejected(self):
  link=self.base/'painter-matched-preclick.Bbc12345';link.symlink_to(self.run,target_is_directory=True)
  with self.assertRaises(ValueError):m.owned_run(link)
 def test_fresh_profile_then_exact_archive(self):
  p=m.prepare_profile(self.run,self.cell,1);cache=self.build/'app/tests/gimpdir-output';self.assertFalse((cache/'pluginrc').exists())
  (cache/'pluginrc').write_text('fixture-cache');r=m.archive_output(self.run,self.cell,1)
  self.assertFalse(cache.exists());self.assertEqual((p/'native-output/pluginrc').read_text(),'fixture-cache')
  self.assertEqual(r['files']['pluginrc'],hashlib.sha256(b'fixture-cache').hexdigest())
  p2=m.prepare_profile(self.run,self.cell,6);self.assertNotEqual(p,p2);self.assertFalse((cache/'pluginrc').exists())
  self.assertEqual((p/'native-output/pluginrc').read_text(),'fixture-cache')
 def test_prior_owned_output_preserved_not_deleted(self):
  c=self.build/'app/tests/gimpdir-output';c.mkdir();(c/'old').write_text('prior')
  m.prepare_profile(self.run,self.cell,1)
  self.assertEqual((self.run/'retained-output/01-baseline-normal-before/old').read_text(),'prior')
  self.assertEqual(list(c.iterdir()),[])
 def test_reused_profile_rejected(self):
  m.prepare_profile(self.run,self.cell,1)
  with self.assertRaises(FileExistsError):m.prepare_profile(self.run,self.cell,1)
 def test_symlink_cache_rejected_without_touching_external(self):
  outside=self.base/'outside';outside.mkdir();(outside/'keep').write_text('safe');(self.build/'app/tests/gimpdir-output').symlink_to(outside,target_is_directory=True)
  with self.assertRaises(ValueError):m.prepare_profile(self.run,self.cell,1)
  self.assertEqual((outside/'keep').read_text(),'safe')
 def test_symlink_archive_entry_rejected(self):
  m.prepare_profile(self.run,self.cell,1);(self.build/'app/tests/gimpdir-output/link').symlink_to(self.base)
  with self.assertRaises(ValueError):m.archive_output(self.run,self.cell,1)
 def test_exact_selection_rejects_wrong_cell_repetition_kind(self):
  m.selection('baseline-normal',1,1,'primary')
  for args in [('current-normal',1,1,'primary'),('baseline-normal',2,1,'primary'),('baseline-normal',1,1,'extended')]:
   with self.assertRaises(ValueError):m.selection(*args)
class DriverContracts(unittest.TestCase):
 def setUp(self):self.raw=(HERE/'run_matched_preclick.sh').read_text();self.p=m.policy()
 def test_four_fresh_cells_and_exact_versions(self):
  self.assertEqual(len(self.p['cells']),4);self.assertEqual(len(set(self.p['cells'])),4)
  self.assertEqual(self.p['baseline_commit'],'e25a94e21efca6e6604ddac6f15570c049f421e7')
  self.assertEqual(self.p['current_commit'],'b671825309326bdb55571652e43ab78cc83fd92d')
  self.assertIn('worktree add --detach "$src" "$commit"',self.raw)
  self.assertNotIn('prepare_brush_baseline.py',self.raw)
 def test_exact_twelve_plus_two_order(self):
  rows=self.p['order'];self.assertEqual([x['order'] for x in rows],list(range(1,15)))
  for c in self.p['cells']:self.assertEqual([x['repetition'] for x in rows if x['cell']==c and x['kind']=='primary'],[1,2,3])
  self.assertEqual([x['cell'] for x in rows[4:8]],['current-sanitizer','baseline-sanitizer','current-normal','baseline-normal'])
  self.assertEqual([(x['cell'],x['kind']) for x in rows[12:]],[('current-normal','fidelity'),('current-sanitizer','fidelity')])
 def test_fixed_targets_catalogs_and_only_current_fidelity(self):
  self.assertEqual(len(self.p['common_targets']),13);self.assertEqual(sum('plug-ins/common/' in x for x in self.p['common_targets']),4)
  self.assertEqual(sum(x.endswith(':alias') for x in self.p['common_targets']),6)
  self.assertEqual(self.p['current_fidelity_target'],'painter-compact-options')
 def test_exact_sanitizer_and_deadline_policy(self):
  self.assertEqual(self.p['ASAN_OPTIONS'],'halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1')
  self.assertEqual(self.p['UBSAN_OPTIONS'],'halt_on_error=1:print_stacktrace=1')
  self.assertIn('unset ASAN_OPTIONS UBSAN_OPTIONS LSAN_OPTIONS',self.raw)
  self.assertIn('sanitizer+=(-Db_sanitize=address,undefined)',self.raw)
  self.assertNotIn('--timeout-multiplier',self.raw);self.assertNotIn('detect_leaks=0',self.raw)
  self.assertIn('leaf=/matched-preclick/primary',self.raw);self.assertNotIn('leaf=/matched-preclick/extended',self.raw)
 def test_no_native_failure_waiver(self):
  for native,collection,archived,postflight in [(0,0,0,0),(1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1),(77,0,0,0)]:
   result=subprocess.run(['bash','-c',f'native={native};collection={collection};archived={archived};postflight={postflight}; (( native == 0 && collection == 0 && archived == 0 && postflight == 0 ))'])
   self.assertEqual(result.returncode,0 if (native,collection,archived,postflight)==(0,0,0,0) else 1)
  self.assertIn('|| native=$?',self.raw)
 def test_all_fourteen_attempted_after_native_failure(self):
  begin=self.raw.index("while IFS=$'\\t' read -r cell rep order kind; do")
  end=self.raw.index('# Resolution/inventory tools',begin)
  block=self.raw[begin:end]
  script='set -euo pipefail\nhere="$1";run=/private;out=/artifacts;failed=0\nbash() { printf "%s\\n" "$7"; return 1; }\n'+block+'exit "$failed"\n'
  r=subprocess.run(['bash','-c',script,'test',str(HERE)],capture_output=True,text=True)
  self.assertEqual(r.returncode,1);self.assertEqual(r.stdout.splitlines(),[str(x) for x in range(1,15)])
 def test_postflight_before_archive_and_all_font_checks_after_matrix(self):
  self.assertLess(self.raw.index('runtime-preflight'),self.raw.index('stage=native-measurement'))
  self.assertLess(self.raw.index('runtime-postflight'),self.raw.index('matched_driver_state.py" archive'))
  self.assertLess(self.raw.index('stage=fourteen-native-runs'),self.raw.index('stage=runtime-comparability'))
  self.assertIn('--cache-directory "$profile/native-output" --postflight',self.raw)
 def test_same_portrait_wrapper_and_fresh_profiles(self):
  self.assertEqual(self.raw.count("-screen 0 1080x1920x24 -nolisten tcp"),1)
  self.assertIn('--wrapper "python3 $here/matched_preclick_exec.py"',self.raw)
  self.assertIn('GDK_SCALE=1 GDK_DPI_SCALE=1 GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas',self.raw)
  self.assertIn('LANG=C.UTF-8 LC_ALL=C.UTF-8',self.raw)
 def test_disk_failclear_and_no_system_cleanup(self):
  self.assertIn('available >= 40 * 1024 * 1024',self.raw)
  self.assertNotIn('rm -',self.raw);self.assertNotIn('drop_caches',self.raw);self.assertNotIn('sudo',self.raw)
 def test_reviewed_observer_unchanged(self):
  inputs=HERE/'matched-preclick'
  for name,key in [('test-matched-preclick.c','observer_source_sha256'),('matched-preclick-phases.inc','phase_source_sha256'),('meson.append','meson_append_sha256')]:self.assertEqual(hashlib.sha256((inputs/name).read_bytes()).hexdigest(),self.p[key])
 def test_workflow_separate_bounded_and_private_log_not_uploaded(self):
  text=(SOURCE/'.github/workflows/painter-native-ui.yml').read_text();before,job=text.split('\n\n  matched-preclick-diagnostic:',1)
  self.assertEqual(hashlib.sha256((before+'\n').encode()).hexdigest(),'65ef4a7f90e9280e03b71bba4576cda3cafac9750531d7dcef6244706c2c55e5')
  for x in ('timeout-minutes: 180','timeout-minutes: 120','timeout-minutes: 50','path: matched-results/') :self.assertIn(x,job)
  self.assertNotIn('continue-on-error',job);self.assertNotIn('path: $',job);self.assertNotIn('gdb',job)
if __name__=='__main__':unittest.main()
