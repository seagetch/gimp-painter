import unittest
from pathlib import Path
import subprocess
import tempfile
from unittest.mock import mock_open, patch
from prove import public_identity, stage_lcms_metadata, probe_pkg_config, MAX_PC_BYTES
from validate import check_run, check_elf, loaded_paths

OUT = 'babl_init completed\nbabl_exit completed\n'
RED = '''\n=================================================================\n==123==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 12 byte(s) in 1 object(s) allocated from:
    #0 0x123 malloc allocator.c:1
    #1 0x456 babl_strdup babl-memory.c:1
    #2 0x789 babl_format_new babl-format.c:377

SUMMARY: AddressSanitizer: 12 byte(s) leaked in 1 allocation(s).
'''

class Validation(unittest.TestCase):
    def test_public_identity_allowlist(self):
        cpu = 'vendor_id : example\nmodel name : Test CPU\nserial : SECRET\n\nvendor_id : second CPU\n'
        with patch('builtins.open', mock_open(read_data=cpu)), patch.dict('os.environ', {'GITHUB_SHA': 'a' * 300, 'SECRET_TOKEN': 'secret', 'HOSTNAME': 'private'}, clear=True):
            identity = public_identity()
        self.assertEqual(identity['ci'], {'GITHUB_SHA': 'a' * 256})
        self.assertEqual(identity['cpu'], {'vendor_id': 'example', 'model name': 'Test CPU'})
        self.assertEqual(set(identity['kernel']), {'system', 'release', 'machine'})
    def test_red(self):
        self.assertEqual(check_run('original', 23, OUT, RED)['leaked_bytes'], 12)
    def test_green(self):
        self.assertEqual(check_run('patched', 0, OUT, '')['allocations'], 0)
    def test_reject_bad_exits(self):
        for code in (0, 1, -11, 124):
            with self.assertRaises(ValueError): check_run('original', code, OUT, RED)
    def test_reject_wrong_leak(self):
        for bad in (RED.replace('babl_format_new', 'unrelated'), RED.replace(':377', ':372'), RED.replace('12 byte(s) leaked', '13 byte(s) leaked'), RED + 'runtime error: bad\n', RED + 'unexpected failure\n'):
            with self.assertRaises(ValueError): check_run('original', 23, OUT, bad)
    def test_reject_fatal(self):
        with self.assertRaises(ValueError): check_run('original', 23, OUT, 'LeakSanitizer has encountered a fatal error')
    def test_reject_in_block_diagnostic(self):
        for extra in ('unexpected failure', 'ERROR: unrelated failure', '    #9 0xabc out_of_sequence'):
            bad = RED.replace('\nSUMMARY:', '\n' + extra + '\nSUMMARY:')
            with self.assertRaises(ValueError): check_run('original', 23, OUT, bad)
    def test_reject_green_stderr(self):
        with self.assertRaises(ValueError): check_run('patched', 0, OUT, RED)
    def test_reject_incomplete(self):
        with self.assertRaises(ValueError): check_run('patched', 0, '', '')
    def test_hardening(self):
        check_elf('FLAGS BIND_NOW', 'GNU_RELRO')
        for dynamic, program in [('FLAGS BIND_NOW (RUNPATH)', 'GNU_RELRO'), ('', 'GNU_RELRO'), ('FLAGS BIND_NOW', '')]:
            with self.assertRaises(ValueError): check_elf(dynamic, program)
    def test_inventory(self):
        self.assertEqual(loaded_paths('LOADED\t/a.so\n'), ['/a.so'])
        with self.assertRaises(ValueError): loaded_paths('LOADED\t/a.so\nLOADED\t/a.so\n')


class PkgConfig(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.lib = self.root / 'arm' / 'lib'
        self.own = self.lib / 'pkgconfig'
        self.own.mkdir(parents=True)
        self.logs = self.root / 'logs'
        self.logs.mkdir()
        self.external = self.root / 'system-lcms2.pc'
        self.external.write_text('prefix=/fixture-lcms\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\nName: lcms2\nDescription: fixture only\nVersion: 2.16\nLibs: -L${libdir} -llcms2\nCflags: -I${includedir}\n')
        self.dep = self.root / 'dependency-pkgconfig'
        self.metadata = stage_lcms_metadata(self.external, self.dep)
        (self.own / 'babl-0.1.pc').write_text('prefix=/fixture-owned-babl\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\nName: babl\nDescription: fixture only\nVersion: 0.1.114\nRequires: lcms2\nLibs: -L${libdir} -lbabl-0.1\nCflags: -I${includedir}\n')
        self.env = {'PATH': '/usr/bin:/bin', 'LANG': 'C', 'LC_ALL': 'C',
                    'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'LSAN_OPTIONS': 'exitcode=23'}

    def test_old_isolation_reproduces_missing_lcms(self):
        p = subprocess.run(['pkg-config', '--cflags', '--libs', 'babl-0.1'],
                           env=dict(self.env, PKG_CONFIG_LIBDIR=str(self.own)),
                           capture_output=True, text=True, timeout=10)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('lcms2', p.stderr)

    def test_fixed_exact_dependency_closure(self):
        before = dict(self.env)
        flags = probe_pkg_config(self.lib, self.dep, self.env, self.logs)
        self.assertIn('-L/fixture-owned-babl/lib', flags)
        self.assertIn('-L/fixture-lcms/lib', flags)
        self.assertIn('-lbabl-0.1', flags)
        self.assertIn('-llcms2', flags)
        self.assertEqual(self.env, before)
        self.assertEqual((self.logs / 'pkg-config-origin-babl-0.1.stdout').read_text().strip(), str(self.own))
        self.assertEqual((self.logs / 'pkg-config-origin-lcms2.stdout').read_text().strip(), str(self.dep))

    def test_inherited_pkg_config_path_cannot_shadow_owned_babl(self):
        poison = self.root / 'poison'
        poison.mkdir()
        (poison / 'babl-0.1.pc').write_text((self.own / 'babl-0.1.pc').read_text().replace('fixture-owned-babl', 'wrong-babl'))
        flags = probe_pkg_config(self.lib, self.dep,
                                 dict(self.env, PKG_CONFIG_PATH=str(poison), PKG_CONFIG_SYSROOT_DIR='/wrong-root'), self.logs)
        self.assertIn('fixture-owned-babl', flags)
        self.assertNotIn('wrong-', flags)

    def test_missing_owned_metadata_rejected(self):
        (self.own / 'babl-0.1.pc').unlink()
        with self.assertRaises(ValueError): probe_pkg_config(self.lib, self.dep, self.env, self.logs)

    def test_missing_external_metadata_rejected(self):
        (self.dep / 'lcms2.pc').unlink()
        with self.assertRaises(ValueError): probe_pkg_config(self.lib, self.dep, self.env, self.logs)

    def test_unlisted_transitive_dependency_cannot_use_host(self):
        with (self.dep / 'lcms2.pc').open('a') as f: f.write('Requires: unlisted-proof-dependency\n')
        with self.assertRaises(RuntimeError): probe_pkg_config(self.lib, self.dep, self.env, self.logs)

    def test_staged_bytes_and_digest_are_exact(self):
        import hashlib
        self.assertEqual((self.dep / 'lcms2.pc').read_bytes(), self.external.read_bytes())
        self.assertEqual(self.metadata['sha256'], hashlib.sha256(self.external.read_bytes()).hexdigest())
        self.assertEqual(self.metadata['sha256'], self.metadata['staged_sha256'])
        self.assertEqual(self.dep.stat().st_mode & 0o777, 0o700)

    def test_symlink_metadata_rejected(self):
        link = self.root / 'linked.pc'
        link.symlink_to(self.external)
        with self.assertRaises(ValueError): stage_lcms_metadata(link, self.root / 'new-deps')
        (self.own / 'babl-0.1.pc').unlink()
        (self.own / 'babl-0.1.pc').symlink_to(self.dep / 'lcms2.pc')
        with self.assertRaises(ValueError): probe_pkg_config(self.lib, self.dep, self.env, self.logs)

    def test_oversized_metadata_rejected(self):
        self.external.write_bytes(b'x' * (MAX_PC_BYTES + 1))
        with self.assertRaises(ValueError): stage_lcms_metadata(self.external, self.root / 'new-deps')

    def test_pcfiledir_relative_metadata_rejected(self):
        self.external.write_text('prefix=${pcfiledir}/../..\n')
        with self.assertRaises(ValueError): stage_lcms_metadata(self.external, self.root / 'new-deps')

    def test_existing_staging_directory_rejected(self):
        with self.assertRaises(FileExistsError): stage_lcms_metadata(self.external, self.dep)

    def test_origin_mismatch_rejected(self):
        with patch('prove.run', return_value=subprocess.CompletedProcess([], 0, '/unexpected\n', '')):
            with self.assertRaises(ValueError): probe_pkg_config(self.lib, self.dep, self.env, self.logs)

if __name__ == '__main__': unittest.main()
