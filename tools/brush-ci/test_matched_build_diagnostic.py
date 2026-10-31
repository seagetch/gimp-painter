import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

PATH = Path(__file__).with_name('collect_matched_build_diagnostic.py')
spec = importlib.util.spec_from_file_location('diagnostic', PATH)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class Diagnostics(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root/'logs').mkdir()
        self.log = self.root/'logs/baseline-normal-compile.log'
    def tearDown(self):
        self.tmp.cleanup()
    def collect(self, data=b'error: bad\n'):
        self.log.write_bytes(data)
        return m.collect(self.root, 'baseline-normal', 'compile', 'finished', 17)
    def test_fixed_labels_no_content(self):
        data = b'/secret/home/a.c:1:2: error: token=SuperSecret\ncc -DSECRET=TopSecret\n  return credential;\na.c:1: warning: "quoted value"\n'
        result = self.collect(data)
        text = json.dumps(result)
        for secret in ('SuperSecret', 'TopSecret', '/secret', 'credential', 'quoted value', 'cc -D'):
            self.assertNotIn(secret, text)
        self.assertEqual(result['exit_code'], 17)
        self.assertEqual(result['log']['category_counts']['compiler-error'], 1)
        self.assertEqual(result['log']['category_counts']['uncategorized'], 2)
    def test_running_never_reads(self):
        with patch.object(m, 'read_tail', side_effect=AssertionError):
            self.assertEqual(m.collect('/nonexistent', m.CELLS[0], 'compile', 'running')['log']['status'], 'not-read-running')
    def test_complete_no_log(self):
        with patch.object(m, 'read_tail', side_effect=AssertionError):
            self.assertEqual(m.collect('/nonexistent', m.CELLS[0], 'complete', 'finished', 0)['log']['status'], 'not-applicable')
    def test_missing_retains_exit(self):
        r = m.collect(self.root,m.CELLS[0],'compile','finished',99)
        self.assertEqual(r['exit_code'],99)
        self.assertEqual(r['log']['status'],'missing-log')
    def test_symlink_file(self):
        self.log.symlink_to('/etc/passwd')
        self.assertEqual(m.read_tail(self.root,m.CELLS[0],'compile')['status'],'unsafe-or-unreadable-log')
    def test_symlink_parent(self):
        alias=self.root/'alias'; alias.symlink_to(self.root,target_is_directory=True)
        self.assertEqual(m.read_tail(alias,m.CELLS[0],'compile')['status'],'unsafe-or-unreadable-log')
    def test_fifo_no_block(self):
        os.mkfifo(self.log)
        self.assertEqual(m.read_tail(self.root,m.CELLS[0],'compile')['status'],'unsafe-log')
    def test_directory_not_read(self):
        self.log.mkdir()
        self.assertEqual(m.read_tail(self.root,m.CELLS[0],'compile')['status'],'unsafe-log')
    def test_bounded(self):
        r=self.collect(b'gcc: error: old\n'+b'x'*100000+b'\ngcc: warning: end\n')['log']
        self.assertEqual(r['bytes_read'],65536)
        self.assertTrue(r['tail_truncated'])
        self.assertEqual(r['category_counts']['compiler-error'],0)
        self.assertEqual(r['category_counts']['compiler-warning'],1)
    def test_tail_count(self):
        r=self.collect(b'gcc: error: x\n'*100)['log']
        self.assertEqual(len(r['tail']),32)
        self.assertEqual(r['tail_lines_omitted'],68)
    def test_empty(self):
        self.assertEqual(self.collect(b'')['log']['tail'],[])
    def test_meson_not_compiler_or_linker(self):
        labels=m.classify(b'../private/meson.build:2075:4: ERROR: '+m.NATIVE_GIMP_MESSAGE)
        self.assertIn('native-gimp-required',labels)
        self.assertNotIn('compiler-error',labels)
        self.assertNotIn('linker-error',labels)
    def test_exact_program_template(self):
        self.assertIn('native-gimp-lookup-missing',m.classify(m.PROGRAM_MISSING))
        self.assertNotIn('native-gimp-lookup-missing',m.classify(b'echo '+m.PROGRAM_MISSING))
        self.assertNotIn('native-gimp-lookup-missing',m.classify(m.PROGRAM_MISSING+b' SECRET'))
    def test_project_template_no_remainder(self):
        line=b'meson.build:1:1: ERROR: '+m.NATIVE_GIMP_MESSAGE
        for bad in (b'echo '+line,line+b' secret',line.replace(b'gimp-3.0',b'secret')):
            self.assertNotIn('native-gimp-required',m.classify(bad))
    def test_anchored_tool_headers(self):
        for line in (b'/usr/bin/ld: cannot find -lasan',b'/usr/bin/x86_64-linux-gnu-ld.bfd: error',b'collect2: error: ld returned 1 exit status'):
            self.assertIn('linker-error',m.classify(line))
        for line in (b'echo ld: error',b'source text ld: error',b'meson.build:1:1: ERROR: unknown'):
            self.assertNotIn('linker-error',m.classify(line))
        self.assertIn('compiler-error',m.classify(b'/private/foo.c:4:7: error: private'))
        self.assertNotIn('compiler-error',m.classify(b'echo gcc: error: private'))
    def test_no_project_prose_export(self):
        r=self.collect(b'../private/meson.build:2075:4: ERROR: '+m.NATIVE_GIMP_MESSAGE+b'\n')
        text=json.dumps(r)
        self.assertNotIn('private',text)
        self.assertNotIn('When cross-compiling',text)
        self.assertIn('native-gimp-required',text)

    def test_exit_scope(self):
        for stage in m.STAGES:
            r=m.collect(self.root,m.CELLS[0],stage,'finished',17)
            self.assertEqual(r['exit_code'],17)
            self.assertEqual(r['exit_scope'],'build-cell-shell-status-at-checkpointed-stage-not-necessarily-tool-exit')

    def test_invalid_selectors(self):
        for cell,stage,state,code in [('bad','compile','finished',1),(m.CELLS[0],'bad','finished',1),(m.CELLS[0],'compile','running',0),(m.CELLS[0],'compile','finished',None),(m.CELLS[0],'compile','finished',256)]:
            with self.assertRaises(ValueError): m.collect(self.root,cell,stage,state,code)
    def test_output_atomic_replace(self):
        r=self.collect(); m.persist(self.root,m.CELLS[0],r)
        path=self.root/'build-diagnostics/baseline-normal.json'
        self.assertEqual(json.loads(path.read_text()),r)
        r['exit_code']=3; m.persist(self.root,m.CELLS[0],r)
        self.assertEqual(json.loads(path.read_text())['exit_code'],3)
        self.assertEqual(path.stat().st_mode & 0o777,0o600)
    def test_output_directory_symlink_rejected(self):
        (self.root/'build-diagnostics').symlink_to(self.root,target_is_directory=True)
        with self.assertRaises(OSError): m.persist(self.root,m.CELLS[0],{})
    def test_change_during_read(self):
        self.log.write_bytes(b'error: first\n')
        original = os.read
        def changing(fd, count):
            data = original(fd, count)
            with self.log.open('ab') as stream:
                stream.write(b'changed\n')
            return data
        with patch.object(m.os, 'read', side_effect=changing):
            self.assertEqual(m.read_tail(self.root,m.CELLS[0],'compile')['status'], 'changed-during-read')
    def test_useful_categories(self):
        data = (b'fatal error: foo.h: No such file or directory\n'
                b'ld: undefined reference to hidden_name\n'
                b'meson.build:3:2: ERROR: Dependency hidden not found\n'
                b'libasan.so: cannot open shared object file\n'
                b'No space left on device\n'
                b'Killed signal terminated program cc1\n'
                b'Traceback (most recent call last)\n')
        counts = self.collect(data)['log']['category_counts']
        for name in ('missing-header','undefined-reference','configure-error','dependency-not-found','sanitizer-runtime-not-found','no-space','python-exception','compiler-killed'):
            self.assertGreater(counts[name],0,name)

    def test_cli(self):
        run = self.root/'painter-matched-preclick.ABC12345'; run.mkdir(mode=0o700)
        (run/'logs').mkdir()
        out = self.root/'matched-results'; out.mkdir()
        (run/'logs/baseline-normal-compile.log').write_text('ninja: build stopped: subcommand failed.\n')
        p=subprocess.run([sys.executable,str(PATH),'--run',str(run),'--out',str(out),'--cell',m.CELLS[0],'--stage','compile','--state','finished','--exit-code','1'],capture_output=True,env={**os.environ,'RUNNER_TEMP':str(self.root),'GITHUB_WORKSPACE':str(self.root)})
        self.assertEqual(p.returncode,0)
        self.assertEqual(p.stdout+p.stderr,b'')
    def test_root_boundary(self):
        run=self.root/'painter-matched-preclick.ABC12345'; run.mkdir(mode=0o700)
        out=self.root/'matched-results'; out.mkdir()
        with patch.dict(os.environ, {'RUNNER_TEMP':str(self.root),'GITHUB_WORKSPACE':str(self.root)}):
            m.validate_roots(run,out)
            with self.assertRaises(ValueError): m.validate_roots(self.root,out)
            with self.assertRaises(ValueError): m.validate_roots(run,self.root)
            run.chmod(0o755)
            with self.assertRaises(ValueError): m.validate_roots(run,out)
    def test_cli_arbitrary_root_rejected(self):
        p=subprocess.run([sys.executable,str(PATH),'--run',str(self.root),'--out',str(self.root),'--cell',m.CELLS[0],'--stage','compile','--state','running'],capture_output=True,env={**os.environ,'RUNNER_TEMP':str(self.root),'GITHUB_WORKSPACE':str(self.root)})
        self.assertEqual(p.returncode,2)
        self.assertNotIn(str(self.root).encode(),p.stdout+p.stderr)

    def test_cli_bad_value_not_echoed(self):
        p=subprocess.run([sys.executable,str(PATH),'--cell','SecretValue'],capture_output=True)
        self.assertEqual(p.returncode,2)
        self.assertNotIn(b'SecretValue',p.stderr)

if __name__ == '__main__': unittest.main()
