import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
import preframe_identity as m

class Identity(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.run = self.base/'painter-brush-ci.Abc12345'; self.run.mkdir(mode=0o700)
        self.src = self.run/'source'; self.build = self.run/'build'
        self.src.mkdir(); self.build.mkdir()
        self.binary = self.build/'app/tests/painter-compact-options'
        paths = [self.binary, self.build/'compile_commands.json', self.build/'build.ninja', self.build/'meson-info/intro-buildoptions.json', self.run/'deps/env.sh', self.src/'tools/linux-debian13-env.sh']
        for p in paths: p.parent.mkdir(parents=True, exist_ok=True); p.write_text('fixture')
        self.binary.chmod(0o700)
        (self.build/'meson-info/intro-buildoptions.json').write_text(json.dumps([{'name':'b_sanitize','value':['address','undefined']}]))
        (self.build/'meson-info/intro-targets.json').write_text(json.dumps([{'name':'painter-compact-options','type':'executable','filename':[str(self.binary)]}]))
        d = self.src/'tools/brush-ci'; d.mkdir()
        shutil.copyfile(m.__file__, d/'preframe_identity.py')
        frozen = Path(__file__).with_name('sample_case14.py')
        shutil.copyfile(frozen, d/'sample_case14.py')
        self.opt = self.base/'options.json'
        self.opt.write_text(json.dumps({'ASAN_OPTIONS':m.ASAN,'UBSAN_OPTIONS':m.UBSAN,'LSAN_OPTIONS_unset':True}))
        self.receipt = self.base/'receipt.json'; self.output = self.base/'output.json'
        self.env = patch.dict(os.environ, {'GITHUB_ACTIONS':'true','GITHUB_SHA':'a'*40,'RUNNER_TEMP':str(self.base)}, clear=True)
        self.env.start(); self.addCleanup(self.env.stop)
        self.commands = patch.object(m, 'command', side_effect=self.command)
        self.commands.start(); self.addCleanup(self.commands.stop)
        self.args = argparse.Namespace(run=str(self.run), source=str(self.src), build=str(self.build), options_json=str(self.opt), original_exit=1)
    def command(self, argv):
        if argv[0]=='git': return 'a'*40+'\n' if 'rev-parse' in argv else ''
        raise AssertionError('Unexpected command')
    def seal(self):
        j=m.capture(self.args); self.receipt.write_text(json.dumps(j)); return j
    def test_roundtrip(self):
        self.seal()
        self.assertEqual(m.verify(str(self.receipt))['original_exit'], 1)
    def test_binary_mutation(self):
        self.seal(); self.binary.write_text('mutated')
        with self.assertRaises(m.Invalid): m.verify(str(self.receipt))
    def test_deps_env_mutation(self):
        self.seal(); (self.run/'deps/env.sh').write_text('mutated')
        with self.assertRaises(m.Invalid): m.verify(str(self.receipt))
    def test_build_mutation(self):
        self.seal(); (self.build/'build.ninja').write_text('mutated')
        with self.assertRaises(m.Invalid): m.verify(str(self.receipt))
    def test_helper_mutation(self):
        self.seal(); (self.src/'tools/brush-ci/preframe_identity.py').write_text('mutated')
        with self.assertRaises(m.Invalid): m.verify(str(self.receipt))
    def test_controller_mutation(self):
        (self.src/'tools/brush-ci/sample_case14.py').write_text('mutated')
        with self.assertRaises(m.Invalid): self.seal()
    def test_symlink_rejected(self):
        p=self.run/'deps/env.sh'; p.unlink(); p.symlink_to(self.opt)
        with self.assertRaises(m.Invalid): self.seal()
    def test_public_run_rejected(self):
        self.run.chmod(0o755)
        with self.assertRaises(m.Invalid): self.seal()
    def test_wrong_runner_temp(self):
        os.environ['RUNNER_TEMP']=str(self.run)
        with self.assertRaises(m.Invalid): self.seal()
    def test_wrong_head(self):
        os.environ['GITHUB_SHA']='b'*40
        with self.assertRaises(m.Invalid): self.seal()
    def test_not_hosted(self):
        os.environ['GITHUB_ACTIONS']='false'
        with self.assertRaises(m.Invalid): self.seal()
    def test_dirty_source(self):
        original=self.command
        with patch.object(m,'command',side_effect=lambda a: '?? local\n' if 'status' in a else original(a)):
            with self.assertRaises(m.Invalid): self.seal()
    def test_no_sanitizer(self):
        (self.build/'meson-info/intro-buildoptions.json').write_text('[{"name":"b_sanitize","value":"none"}]')
        with self.assertRaises(m.Invalid): self.seal()
    def test_malformed_options_finite(self):
        self.seal()
        for bad in ([1], {}, [{'name':'b_sanitize','value':12}]):
            with self.subTest(bad=bad):
                (self.build/'meson-info/intro-buildoptions.json').write_text(json.dumps(bad))
                self.assertEqual(m.main(['verify','--receipt',str(self.receipt),'--output',str(self.output)]),2)
    def test_boolean_original_exit_rejected(self):
        j=self.seal(); j['original_exit']=True; self.receipt.write_text(json.dumps(j))
        with self.assertRaises(m.Invalid): m.verify(str(self.receipt))
    def test_malformed_targets_finite(self):
        self.seal()
        (self.build/'meson-info/intro-targets.json').write_text('["private malformed content"]')
        self.assertEqual(m.main(['verify','--receipt',str(self.receipt),'--output',str(self.output)]),2)
        self.assertNotIn('private',self.output.read_text())
    def test_malformed_receipt_finite(self):
        self.receipt.write_text('[]')
        self.assertEqual(m.main(['verify','--receipt',str(self.receipt),'--output',str(self.output)]),2)
    def test_changed_options(self):
        j=json.loads(self.opt.read_text()); j['ASAN_OPTIONS']='detect_leaks=0'; self.opt.write_text(json.dumps(j))
        with self.assertRaises(m.Invalid): self.seal()
    def wrapper(self, extra=None):
        return ['wrapper','--receipt',str(self.receipt),'--output',str(self.output),'--controller',str(self.src/'tools/brush-ci/sample_case14.py'),'--',str(self.binary),'-p',m.CASE]+(extra or [])
    def test_wrapper_exec_exact_no_env_changes(self):
        self.seal(); os.environ.update(ASAN_OPTIONS=m.ASAN, UBSAN_OPTIONS=m.UBSAN)
        before=dict(os.environ)
        class Executed(Exception): pass
        with patch.object(m.os,'execv',side_effect=Executed) as ex:
            with self.assertRaises(Executed): m.main(self.wrapper())
        self.assertEqual(before,dict(os.environ))
        args=ex.call_args.args[1]
        self.assertIn('/usr/bin/gdb',args); self.assertEqual(args[-5:], ['--output',str(self.output),'--','-p',m.CASE])
    def test_wrapper_rejects_extra_selector(self):
        self.seal(); os.environ.update(ASAN_OPTIONS=m.ASAN, UBSAN_OPTIONS=m.UBSAN)
        with patch.object(m.os,'execv') as ex:
            self.assertEqual(m.main(self.wrapper(['-x','/another'])),2); ex.assert_not_called()
    def test_wrapper_rejects_lsan_even_empty(self):
        self.seal(); os.environ.update(ASAN_OPTIONS=m.ASAN, UBSAN_OPTIONS=m.UBSAN,LSAN_OPTIONS='')
        with patch.object(m.os,'execv') as ex:
            self.assertEqual(m.main(self.wrapper()),2); ex.assert_not_called()
    def test_fixed_failure_report(self):
        self.receipt.write_text('{"SECRET":"not printed"}')
        self.assertEqual(m.main(['verify','--receipt',str(self.receipt),'--output',str(self.output)]),2)
        self.assertNotIn('SECRET',self.output.read_text()); self.assertFalse(json.loads(self.output.read_text())['acceptance_eligible'])
    def test_verify_cli(self):
        self.seal()
        self.assertEqual(m.main(['verify','--receipt',str(self.receipt),'--output',str(self.output)]),0)
        self.assertFalse(json.loads(self.output.read_text())['acceptance_eligible'])

if __name__=='__main__': unittest.main()
