import unittest
from unittest.mock import mock_open, patch
from prove import public_identity
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

if __name__ == '__main__': unittest.main()
