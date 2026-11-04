import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import prepare_matched_schemas as m

class SchemaPreparation(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/'deps';self.root.mkdir()
        self.compiler=self.root/m.COMPILER;self.compiler.parent.mkdir(parents=True)
        self.compiler.write_text('#!/bin/sh\nexit 0\n');self.compiler.chmod(0o700)
        self.schemas=self.root/m.SCHEMAS;self.schemas.mkdir(parents=True)
        self.log=Path(self.tmp.name)/'private.log';self.receipt=Path(self.tmp.name)/'receipt.json'
    def invoke(self):
        with patch.dict(os.environ,{'LD_PRELOAD':'','LD_AUDIT':''}):
            code=m.prepare(self.root,self.log,self.receipt)
        return code,json.loads(self.receipt.read_text())
    def test_missing_compiler_is_finite(self):
        self.compiler.unlink();code,r=self.invoke();self.assertEqual(code,1)
        self.assertEqual(r['stage'],'missing-or-unsafe-compiler');self.assertIsNone(r['compiler_exit'])
    def test_missing_schema_directory(self):
        self.schemas.rmdir();code,r=self.invoke();self.assertEqual(r['stage'],'missing-or-unsafe-schema-directory')
    def test_symlinked_compiler_rejected(self):
        self.compiler.unlink();self.compiler.symlink_to('/usr/bin/true');code,r=self.invoke()
        self.assertEqual(r['stage'],'missing-or-unsafe-compiler')
    def test_successful_compiler_without_cache_rejected(self):
        code,r=self.invoke();self.assertEqual(code,1);self.assertEqual(r['stage'],'missing-output')
        self.assertEqual(r['compiler_exit'],0)
    def test_exact_dependency_compiler_and_loader(self):
        def fake(args,**kw):
            self.assertEqual(args,[str(self.compiler),'--strict',str(self.schemas)])
            self.assertEqual(kw['env']['LD_LIBRARY_PATH'],str(self.root/'usr/lib/x86_64-linux-gnu'))
            self.assertEqual(kw['timeout'],90)
            (self.schemas/'gschemas.compiled').write_bytes(b'cache')
            return subprocess.CompletedProcess(args,0)
        with patch.object(m.subprocess,'run',side_effect=fake):code,r=self.invoke()
        self.assertEqual(code,0);self.assertEqual(r['stage'],'complete');self.assertEqual(len(r['cache_sha256']),64)
    def test_loader_failure_is_bounded(self):
        self.compiler.write_text('#!/bin/sh\necho "error while loading shared libraries: /private/SECRET" >&2\nexit 127\n')
        code,r=self.invoke();self.assertEqual(code,127);self.assertTrue(r['loader_error_observed'])
        self.assertNotIn('SECRET',self.receipt.read_text());self.assertNotIn(str(self.root),self.receipt.read_text())
    def test_executable_not_loadable(self):
        self.compiler.write_text('not executable format');code,r=self.invoke()
        self.assertEqual(code,127);self.assertEqual(r['stage'],'compiler-unavailable')
    def test_timeout_is_finite(self):
        with patch.object(m.subprocess,'run',side_effect=subprocess.TimeoutExpired(['private'],90)):
            code,r=self.invoke()
        self.assertEqual(code,124);self.assertEqual(r['stage'],'compiler-timeout')
    def test_unsafe_output_rejected(self):
        (self.schemas/'gschemas.compiled').symlink_to('/private/nope');code,r=self.invoke()
        self.assertEqual(r['stage'],'unsafe-output')
    def test_loader_injection_rejected(self):
        with patch.dict(os.environ,{'LD_PRELOAD':'private'}):
            code=m.prepare(self.root,self.log,self.receipt)
        self.assertEqual(code,1);self.assertEqual(json.loads(self.receipt.read_text())['stage'],'loader-injection-rejected')
    def test_large_diagnostic_truncates_without_export(self):
        def fake(args,**kw):
            kw['stdout'].write(b'SECRET'*(m.LIMIT//6+5))
            return subprocess.CompletedProcess(args,2)
        with patch.object(m.subprocess,'run',side_effect=fake):code,r=self.invoke()
        self.assertEqual(code,2);self.assertTrue(r['diagnostic_truncated'])
        self.assertNotIn('SECRET',self.receipt.read_text())
    def test_signal_status_preserved(self):
        with patch.object(m.subprocess,'run',return_value=subprocess.CompletedProcess([], -11)):
            code,r=self.invoke()
        self.assertEqual(code,139);self.assertEqual(r['compiler_exit'],-11)
    def test_nonexecutable_compiler_rejected(self):
        self.compiler.chmod(0o600);code,r=self.invoke()
        self.assertEqual(code,1);self.assertEqual(r['stage'],'missing-or-unsafe-compiler')
    def test_shell_failure_cannot_reach_builds(self):
        text=Path(__file__).with_name('run_matched_preclick.sh').read_text()
        block=text[text.index('  stage=dependency-schemas'):text.index('  stage=four-builds')]
        script='set -euo pipefail\nhere=/known;run=/private;out=/artifact\npython3() { return 77; }\n'+block+'echo BUILDS-STARTED\n'
        p=subprocess.run(['bash','-c',script],capture_output=True,text=True)
        self.assertEqual(p.returncode,77);self.assertNotIn('BUILDS-STARTED',p.stdout)
    def test_receipt_write_failure_is_closed(self):
        self.receipt=Path(self.tmp.name)/'missing-parent'/'receipt.json'
        self.assertEqual(m.prepare(self.root,self.log,self.receipt),1)
    def test_runtime_preparation_before_builds(self):
        text=Path(__file__).with_name('run_matched_preclick.sh').read_text()
        a=text.index('  stage=dependency-schemas');b=text.index('  stage=four-builds')
        block=text[a:b];self.assertIn('"$here/prepare_matched_schemas.py" --root "$run/deps/root"',block)
        self.assertIn('--receipt "$out/dependency-schemas.json"',block)
        self.assertNotIn('/usr/bin/glib-compile-schemas',block)
        self.assertLess(text.index('prepare-linux-build-deps.py'),a)

if __name__=='__main__':unittest.main()
