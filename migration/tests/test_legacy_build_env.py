"""Unit tests for local-only autotools relocation, using synthetic tool files."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / 'tools/prepare_legacy_build_env.py'


class LegacyBuildEnvironmentTests(unittest.TestCase):
    def seed(self, base):
        deps = base / 'deps'
        tools = deps / 'root/usr/bin'
        tools.mkdir(parents=True)
        (deps / 'env.sh').write_text('export GIMP_DEPS_ROOT=' + str(deps/'root')+'\n')
        for name in ('autoconf','autoheader','autom4te','automake-1.17','aclocal-1.17',
                     'libtoolize','intltoolize','gtkdocize','autopoint','m4'):
            (tools / name).write_text('#!/bin/sh\nprefix=/usr\ndatadir="/usr/share"\n')
        config = deps/'root/usr/share/autoconf/autom4te.cfg'
        config.parent.mkdir(parents=True)
        config.write_text("args: --prepend-include '/usr/share/autoconf'\n")
        return deps

    def run_helper(self, deps, output):
        return subprocess.run([sys.executable, str(HELPER), '--deps', str(deps),
                               '--output', str(output)], capture_output=True, text=True)

    def test_metadata_and_relocation_are_reproducible(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            deps = self.seed(base)
            out = base/'out'
            original = (deps/'root/usr/bin/gtkdocize').read_bytes()
            result = self.run_helper(deps,out)
            self.assertEqual(result.returncode,0,result.stderr)
            metadata = json.loads((out/'environment.json').read_text())
            for name,digest in metadata['generated_sha256'].items():
                self.assertEqual(hashlib.sha256((out/name).read_bytes()).hexdigest(),digest)
            self.assertNotIn('prefix=/usr\n',(out/'bin/gtkdocize').read_text())
            self.assertEqual((deps/'root/usr/bin/gtkdocize').read_bytes(),original)
            before = (out/'environment.json').read_bytes()
            self.assertEqual(self.run_helper(deps,out).returncode,0)
            self.assertEqual((out/'environment.json').read_bytes(),before)
            self.assertIn('trailer_m4=',(out/'env.sh').read_text())

    def test_missing_tool_is_a_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp)
            deps=self.seed(base)
            (deps/'root/usr/bin/autopoint').unlink()
            self.assertNotEqual(self.run_helper(deps,base/'out').returncode,0)
            self.assertFalse((base/'out').exists())

    def test_shell_metacharacters_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp)
            deps=self.seed(base)
            self.assertNotEqual(self.run_helper(deps,base/'bad;path').returncode,0)
            self.assertFalse((base/'bad;path').exists())


if __name__ == '__main__':
    unittest.main()
