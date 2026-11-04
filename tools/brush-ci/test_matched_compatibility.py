#!/usr/bin/env python3
"""Exact source/branch contracts only; no native Meson/compiler execution."""
import hashlib,json,pathlib,unittest,tempfile,os
from unittest.mock import patch
import apply_matched_compatibility as a
HERE=pathlib.Path(__file__).resolve().parent
class Compatibility(unittest.TestCase):
 def setUp(self):self.p=a.policy(HERE/'matched-preclick/baseline-meson-compatibility.json')
 def test_exact_policy_pin(self):self.assertEqual(a.digest((HERE/'matched-preclick/baseline-meson-compatibility.json').read_bytes()),a.POLICY_SHA)
 def test_exact_two_hunks(self):
  self.assertEqual(len(self.p['edits']),2)
  self.assertIn('if meson.can_run_host_binaries() and have_gobject_introspection',self.p['edits'][0]['before'])
  self.assertIn('if meson.can_run_host_binaries()\n',self.p['edits'][0]['after'])
  self.assertNotIn("subdir('app/tests/color-ownership')",str(self.p['edits']))
 def test_source_guard_truth_table(self):
  # Evaluate only the exact source-reviewed guard expressions, not Meson runtime.
  before=self.p['edits'][0]['before'].splitlines()[0][3:]
  after=next(x for x in self.p['edits'][0]['after'].splitlines() if x.startswith('if '))[3:]
  for runnable in (False,True):
   for gir in (False,True):
    env={'RUNNABLE':runnable,'have_gobject_introspection':gir}
    self.assertEqual(eval(before.replace('meson.can_run_host_binaries()','RUNNABLE'),{'__builtins__':{}},env),runnable and gir)
    self.assertEqual(eval(after.replace('meson.can_run_host_binaries()','RUNNABLE'),{'__builtins__':{}},env),runnable)
 def test_gir_dependencies_guarded(self):
  for edit in self.p['edits']:
   text=edit['after'];self.assertIn('if have_gobject_introspection\n',text);self.assertIn('endif\n',text)
 def test_unknown_source_rejected_both_modes(self):
  for cell in a.CELLS:
   with self.assertRaises(ValueError):a.transform(b'unknown',cell,self.p)
 def test_unknown_cell_rejected(self):
  with self.assertRaises(ValueError):a.transform(b'', 'other-normal',self.p)
class ApplyBoundary(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.base=pathlib.Path(self.tmp.name)
  self.run=self.base/'painter-matched-preclick.Abc12345';self.run.mkdir(mode=0o700)
  self.cell='baseline-normal';self.src=self.run/'cells'/self.cell/'source';self.src.mkdir(parents=True)
  self.file=self.src/'meson.build';self.file.write_bytes(b'before\n')
  self.p={'baseline_commit':'baseline','current_commit':'current','baseline_before_sha256':a.digest(b'before\n'),'baseline_after_sha256':a.digest(b'after\n'),'current_unchanged_sha256':a.digest(b'current\n'),'edits':[{'before':'before\n','after':'after\n'}]}
  self.env=patch.dict(os.environ,{'GITHUB_ACTIONS':'true','RUNNER_TEMP':str(self.base)});self.env.start();self.addCleanup(self.env.stop)
  self.pol=patch.object(a,'policy',return_value=self.p);self.pol.start();self.addCleanup(self.pol.stop)
  self.git=patch.object(a.subprocess,'check_output',return_value=b'baseline\n');self.mockgit=self.git.start();self.addCleanup(self.git.stop)
 def test_baseline_atomic_exact_edit_then_second_refused(self):
  inode=self.file.stat().st_ino;self.assertTrue(a.apply(self.src,self.cell,HERE));self.assertEqual(self.file.read_bytes(),b'after\n');self.assertNotEqual(inode,self.file.stat().st_ino)
  with self.assertRaises(ValueError):a.apply(self.src,self.cell,HERE)
 def test_current_verified_without_write(self):
  self.cell='current-normal';self.src=self.run/'cells'/self.cell/'source';self.src.mkdir(parents=True);self.file=self.src/'meson.build';self.file.write_bytes(b'current\n');self.mockgit.return_value=b'current\n'
  before=self.file.stat();self.assertFalse(a.apply(self.src,self.cell,HERE));after=self.file.stat();self.assertEqual((before.st_ino,before.st_mtime_ns),(after.st_ino,after.st_mtime_ns))
 def test_shallow_source_rejected(self):
  for source in ('/','/source'):
   with self.assertRaises(ValueError):a.apply(source,self.cell,HERE)
  self.mockgit.assert_not_called()
 def test_wrong_cell_owned_path_rejected(self):
  with self.assertRaises(ValueError):a.apply(self.src,'baseline-sanitizer',HERE)
  self.mockgit.assert_not_called()
 def test_source_symlink_rejected(self):
  self.src.rename(self.src.with_name('other'));self.src.symlink_to(self.src.with_name('other'),target_is_directory=True)
  with self.assertRaises(ValueError):a.apply(self.src,self.cell,HERE)
 def test_meson_symlink_rejected(self):
  self.file.rename(self.file.with_name('original'));self.file.symlink_to('original')
  with self.assertRaises(ValueError):a.apply(self.src,self.cell,HERE)
  self.assertEqual(self.file.with_name('original').read_bytes(),b'before\n')
 def test_wrong_commit_rejected(self):
  self.mockgit.return_value=b'wrong\n'
  with self.assertRaises(ValueError):a.apply(self.src,self.cell,HERE)
  self.assertEqual(self.file.read_bytes(),b'before\n')
 def test_nonhosted_rejected(self):
  with patch.dict(os.environ,{'GITHUB_ACTIONS':'false'}):
   with self.assertRaises(ValueError):a.apply(self.src,self.cell,HERE)
 def test_public_owned_run_rejected(self):
  self.run.chmod(0o755)
  with self.assertRaises(ValueError):a.apply(self.src,self.cell,HERE)
if __name__=='__main__':unittest.main()
