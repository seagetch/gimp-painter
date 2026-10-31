#!/usr/bin/env python3
"""Synthetic filesystem/stack checks only; no native, GUI, network or sanitizer run."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import capture_protocol_stderr as gate
from check_brush_ci import PROTOCOL_CASES

class Capture(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='protocol-stderr-unit-')
        self.addCleanup(self.tmp.cleanup)
        self.base=Path(self.tmp.name)
        self.root=self.base/'runner';self.root.mkdir()
        self.run=self.root/'painter-brush-ci.aB123456'
        self.runtime=self.run/'runtime';self.runtime.mkdir(parents=True)
        self.mode='sanitizer'
        self.logs=self.runtime/'protocol-sanitizer-child-logs';self.logs.mkdir(mode=0o700)
        self.artifact=self.base/'artifacts';self.artifact.mkdir()
        self.query=self.logs/'query.stderr'
        self.query.write_text(self.stack())
    def stack(self):
        return (f'==451==ERROR: LeakSanitizer: detected memory leaks\n'
                f'Direct leak of 24 byte(s) in 1 object(s) allocated from:\n'
                f'    #0 0xabc in gimp_protocol_close {self.run}/source/libgimp/gimpplugin.c:246:12\n'
                f'    #1 0xdef ({self.run}/build/libgimp/tests/plug-in-protocol-fixture+0x123) (BuildId: aabbcc)\n'
                f'    #2 0xeee in private_frame /home/private/other.c:99\n'
                f'SUMMARY: LeakSanitizer: 24 byte(s) leaked in 1 allocation(s).\n')
    def collect(self):return gate.collect(self.root,self.runtime,self.artifact,self.mode)
    def test_exact_twelve_ids_match_protocol_gate(self):
        self.assertEqual(['/plug-in-protocol/'+x for x in gate.CASES],PROTOCOL_CASES)
        manifest=json.loads((Path(__file__).parent/'protocol-cases.json').read_text())
        self.assertEqual(manifest['case_ids'],PROTOCOL_CASES)
        self.assertEqual(len(gate.FILES),12)
    def test_complete_lsan_stack_survives_parent_abort_and_private_paths_do_not(self):
        # Only first child finished before a parent abort; no status/env/stdout needed.
        items=self.collect();self.assertEqual(len(items),1)
        clean=items[0][1]
        for value in ('==451==','ERROR: LeakSanitizer','SUMMARY: LeakSanitizer','gimp_protocol_close',
                      '<source>/libgimp/gimpplugin.c:246:12',
                      '<build>/libgimp/tests/plug-in-protocol-fixture+0x123','BuildId: aabbcc'):
            self.assertIn(value.encode(),clean)
        self.assertNotIn(str(self.base).encode(),clean);self.assertNotIn(b'/home/private',clean)
        gate.preserve(self.artifact,self.mode,items)
        report=json.loads((self.artifact/'protocol-sanitizer-child-stderr/receipt.json').read_text())
        self.assertFalse(report['acceptance_eligible']);self.assertEqual(len(report['missing_allowlisted_files']),11)
    def test_only_exact_stderr_files_are_read(self):
        for name in ('query.stdout','query.events','query.tail-wire','query.observations','env','Xauthority','profile','rogue.stderr'):
            (self.logs/name).symlink_to(self.base/'private-missing')
        self.assertEqual([x[0]for x in self.collect()],['query.stderr'])
    def test_all_twelve_empty_stderr_are_valid_diagnostic_inputs(self):
        for name in gate.FILES:(self.logs/name).write_bytes(b'')
        self.assertEqual(len(self.collect()),12)
    def test_positive_mode_is_separate(self):
        self.logs.rename(self.runtime/'protocol-positive-child-logs');self.mode='positive'
        gate.preserve(self.artifact,self.mode,self.collect())
        self.assertTrue((self.artifact/'protocol-positive-child-stderr/receipt.json').is_file())
    def test_wrong_mode_rejected(self):
        self.mode='baseline-red'
        with self.assertRaises(gate.Refused):self.collect()
    def test_no_completed_stderr_refused(self):
        self.query.unlink()
        with self.assertRaises(gate.Refused):self.collect()
    def test_escape_and_wrong_root_rejected(self):
        for root in (self.base,self.base/'other'):
            with self.assertRaises(gate.Refused):gate.collect(root,self.runtime,self.artifact,self.mode)
    def test_dotdot_and_noncanonical_paths_rejected(self):
        for runtime in (str(self.runtime)+'/../runtime',str(self.runtime)+'/',str(self.runtime).replace('/runtime','//runtime')):
            with self.assertRaises(gate.Refused):gate.collect(self.root,runtime,self.artifact,self.mode)
    def test_wrong_run_prefix_rejected(self):
        other=self.root/'painter-native-ui.aB123456'/'runtime'
        with self.assertRaises(gate.Refused):gate.collect(self.root,other,self.artifact,self.mode)
    def test_symlink_root_rejected(self):
        alias=self.base/'alias';alias.symlink_to(self.root,target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):gate.collect(alias,alias/self.run.name/'runtime',self.artifact,self.mode)
    def test_symlink_run_rejected(self):
        self.run.rename(self.root/'real');self.run.symlink_to(self.root/'real',target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_symlink_runtime_rejected(self):
        self.runtime.rename(self.run/'real');self.runtime.symlink_to(self.run/'real',target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_symlink_child_directory_rejected(self):
        self.logs.rename(self.runtime/'real');self.logs.symlink_to(self.runtime/'real',target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_symlink_stderr_rejected(self):
        self.query.unlink();self.query.symlink_to(self.base/'private-missing')
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_hardlink_stderr_rejected(self):
        os.link(self.query,self.base/'hardlink')
        with self.assertRaises(gate.Refused):self.collect()
    def test_fifo_cannot_block_or_be_read(self):
        self.query.unlink();os.mkfifo(self.query)
        with self.assertRaises(gate.Refused):self.collect()
    def test_child_log_directory_must_be_0700(self):
        self.logs.chmod(0o755)
        with self.assertRaises(gate.Refused):self.collect()
    def test_other_writable_directory_rejected(self):
        self.runtime.chmod(0o777)
        with self.assertRaises(gate.Refused):self.collect()
    def test_other_writable_file_rejected(self):
        self.query.chmod(0o666)
        with self.assertRaises(gate.Refused):self.collect()
    def test_wrong_owner_rejected(self):
        with patch.object(gate.os,'geteuid',return_value=os.geteuid()+1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_artifact_symlink_rejected(self):
        self.artifact.rename(self.base/'real');self.artifact.symlink_to(self.base/'real',target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_artifact_runtime_overlap_rejected(self):
        for artifact in (self.runtime,self.logs,self.run):
            with self.assertRaises(gate.Refused):gate.collect(self.root,self.runtime,artifact,self.mode)
    def test_source_truncation_drops_credential_fragments_preserves_tail_stack(self):
        self.query.write_bytes(b'password='+b'z'*(gate.MAX_INPUT_BYTES*2)+b'\n'+self.stack().encode())
        item=self.collect()[0];self.assertGreater(item[2],gate.MAX_INPUT_BYTES)
        self.assertNotIn(b'z'*40,item[1]);self.assertIn(b'SUMMARY: LeakSanitizer',item[1])
        self.assertLessEqual(len(item[1]),gate.MAX_OUTPUT_BYTES)
    def test_redaction_preserves_no_credentials(self):
        self.query.write_bytes(b'{"access_token":"secret123"}\nclient_secret=abc\nAuthorization: Bearer bad\nhttps://u:p@snapshot.debian.org/archive/a?token=secret#x 503\n')
        clean=self.collect()[0][1]
        for value in (b'secret123',b'abc',b'Bearer bad',b'u:p',b'token=secret'):self.assertNotIn(value,clean)
        self.assertIn(b'503',clean)
    def test_sanitizer_expansion_is_output_bounded(self):
        self.query.write_bytes(b'http://x\n'*12000)
        item=self.collect()[0];self.assertLessEqual(len(item[1]),gate.MAX_OUTPUT_BYTES);self.assertTrue(item[3])
    def test_total_input_cap(self):
        with patch.object(gate,'MAX_TOTAL_INPUT_BYTES',1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_total_output_cap(self):
        with patch.object(gate,'MAX_TOTAL_OUTPUT_BYTES',1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_output_receipt_hashes_and_permissions(self):
        gate.preserve(self.artifact,self.mode,self.collect());dest=self.artifact/'protocol-sanitizer-child-stderr'
        self.assertEqual(dest.stat().st_mode&0o777,0o700)
        report=json.loads((dest/'receipt.json').read_text())
        for item in report['files']:
            f=dest/item['name'];self.assertEqual(f.stat().st_mode&0o777,0o600)
            self.assertEqual(hashlib.sha256(f.read_bytes()).hexdigest(),item['sanitized_sha256'])
    def test_exclusive_output_directory_and_symlink_never_replaced(self):
        dest=self.artifact/'protocol-sanitizer-child-stderr';dest.symlink_to(self.base/'private-missing')
        with self.assertRaises(FileExistsError):gate.preserve(self.artifact,self.mode,self.collect())
    def test_duplicate_and_nonallowlisted_output_names_refused(self):
        good=self.collect()[0]
        for items in ([good,good],[('../escape',b'x',1,False)]):
            with self.assertRaises(gate.Refused):gate.preserve(self.artifact,self.mode,items)
    def test_unknown_prefix_is_not_normalized(self):
        raw=(str(self.base/'other/source/private.c')+':1\n').encode()
        clean=gate.sanitize(gate.normalize_driver_prefixes(raw,self.run))
        self.assertNotIn(b'<source>',clean);self.assertNotIn(b'private.c',clean)
    def test_only_boundary_prefixed_driver_paths_normalized(self):
        raw=('junk'+str(self.run)+'/source/private.c\n').encode()
        self.assertNotIn(b'<source>',gate.normalize_driver_prefixes(raw,self.run))
    def test_cli_refusal_never_echoes_private_argument(self):
        p=subprocess.run([sys.executable,'-B',str(Path(gate.__file__)),'--runner-temp','/private/secret-root','--mode','not-a-mode'],capture_output=True)
        self.assertEqual(p.returncode,2);self.assertIn(b'invalid-arguments',p.stdout)
        self.assertNotIn(b'secret-root',p.stdout+p.stderr)

if __name__=='__main__':unittest.main()
