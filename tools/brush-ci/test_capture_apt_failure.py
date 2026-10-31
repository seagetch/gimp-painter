#!/usr/bin/env python3
"""Display/network-free synthetic filesystem tests, not real APT/CI evidence."""
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import capture_apt_failure as gate

class Capture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='apt-diagnostic-unit-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.temp = self.root/'runner'
        self.temp.mkdir()
        self.artifact = self.root/'workspace'/'ci-results'
        self.artifact.mkdir(parents=True)
        self.run = self.temp/'painter-brush-ci.aB123456'
        self.logs = self.run/'deps'/'apt'/'logs'
        self.logs.mkdir(parents=True)
        self.status('dependencies',1)
        (self.logs/'download-000.log').write_text('E: Failed to fetch https://snapshot.debian.org/archive/debian/20261024T000000Z/pool/a.deb  503 Service Unavailable\n')
        (self.logs/'update.log').write_text('Reading package lists...\n')
    def status(self, stage, code):
        (self.artifact/'status.txt').write_text(f'stage={stage}\nexit_code={code}\nmode=positive\n')
    def collect(self):
        return gate.collect(self.temp,self.artifact)
    def test_dependency_failure_captures_only_allowed_logs(self):
        (self.logs/'apt.conf').write_text('password=secret')
        (self.logs/'download-028.log').write_text('password=not-allowed')
        (self.run/'environment.txt').write_text('token=secret')
        result=self.collect();self.assertEqual([x[0]for x in result],['download-000.log','update.log'])
        self.assertIn(b'503 Service Unavailable',result[0][1])
    def test_native_driver_prefix(self):
        self.run.rename(self.temp/'painter-native-ui.aB123456');self.assertEqual(len(self.collect()),2)
    def test_success_does_not_scan_run_directory(self):
        self.status('dependencies',0);self.run.rename(self.temp/'irrelevant');self.assertIsNone(self.collect())
    def test_non_dependency_failure_does_not_capture(self):
        self.status('compile',1);self.assertIsNone(self.collect())
    def test_missing_status_does_not_capture(self):
        (self.artifact/'status.txt').unlink();self.assertIsNone(self.collect())
    def test_duplicate_status_rejected(self):
        with (self.artifact/'status.txt').open('a') as f:f.write('exit_code=0\n')
        with self.assertRaises(gate.Refused):self.collect()
    def test_oversized_status_rejected(self):
        (self.artifact/'status.txt').write_bytes(b'x'*5000)
        with self.assertRaises(gate.Refused):self.collect()
    def test_parent_traversal_rejected(self):
        with self.assertRaises(gate.Refused):gate.collect(str(self.temp)+'/../runner',self.artifact)
    def test_symlink_temp_root_rejected(self):
        p=self.root/'alias';p.symlink_to(self.temp,target_is_directory=True)
        with self.assertRaises(gate.Refused):gate.collect(p,self.artifact)
    def test_symlink_run_rejected(self):
        self.run.rename(self.temp/'other');self.run.symlink_to(self.temp/'other',target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_symlink_component_rejected(self):
        self.logs.rename(self.run/'real-logs');self.logs.symlink_to(self.run/'real-logs',target_is_directory=True)
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_symlink_log_rejected(self):
        p=self.logs/'download-000.log';p.unlink();p.symlink_to(self.root/'private')
        with self.assertRaises((gate.Refused,OSError)):self.collect()
    def test_hardlink_log_rejected(self):
        p=self.logs/'download-000.log';os.link(p,self.root/'alias')
        with self.assertRaises(gate.Refused):self.collect()
    def test_fifo_never_blocks_or_reads(self):
        p=self.logs/'download-000.log';p.unlink();os.mkfifo(p)
        with self.assertRaises(gate.Refused):self.collect()
    def test_world_writable_directory_rejected(self):
        self.logs.chmod(0o777)
        with self.assertRaises(gate.Refused):self.collect()
    def test_wrong_owner_rejected(self):
        with patch.object(gate.os,'geteuid',return_value=os.geteuid()+1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_ambiguous_driver_directories_rejected(self):
        (self.temp/'painter-native-ui.12345678').mkdir()
        with self.assertRaises(gate.Refused):self.collect()
    def test_unknown_prefix_not_scanned(self):
        self.run.rename(self.temp/'painter-other.12345678')
        with self.assertRaises(gate.Refused):self.collect()
    def test_file_count_bound(self):
        with patch.object(gate,'MAX_LOG_FILES',1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_directory_scan_bound(self):
        with patch.object(gate,'MAX_DIRECTORY_ENTRIES',1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_total_byte_bound(self):
        with patch.object(gate,'MAX_TOTAL_BYTES',1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_truncation_keeps_complete_lines_and_tail_error(self):
        p=self.logs/'download-000.log';p.write_bytes(b'head\n'+b'x'*(gate.MAX_FILE_BYTES*2)+b'\nE: tail 503\n')
        item=self.collect()[0];self.assertGreater(item[2],gate.MAX_FILE_BYTES);self.assertIn(b'E: tail 503',item[1]);self.assertNotIn(b'x'*100,item[1]);self.assertLess(len(item[1]),gate.MAX_FILE_BYTES+100)
    def test_url_credentials_query_proxy_and_headers_redacted(self):
        raw=b'E: https://user:pass@snapshot.debian.org/archive/a.deb?token=secret#x 503\nhttps://proxy.private/a?x=y\nAuthorization: Bearer abc\nProxy-Authorization: Basic xyz\nCookie: session=a\npassword=qwerty\n'
        clean=gate.sanitize(raw)
        self.assertIn(b'503',clean)
        for secret in (b'user',b'pass@',b'token=',b'secret',b'proxy.private',b'Bearer abc',b'Basic xyz',b'session=a',b'qwerty'):self.assertNotIn(secret,clean)
    def test_quoted_composite_credentials_drop_entire_line(self):
        raw = b'{"access_token":"secret123"}\n{"token":"secret456"}\nclient_secret=abc\npass\x00word=hidden\n'
        clean = gate.sanitize(raw)
        for item in (b'secret123', b'secret456', b'abc', b'hidden'):
            self.assertNotIn(item, clean)
        self.assertEqual(clean.count(b'[credential-bearing line redacted]'), 4)
    def test_oversized_single_line_credentials_are_not_fragments(self):
        p=self.logs/'download-000.log'
        p.write_bytes(b'password='+b'z'*(gate.MAX_FILE_BYTES*2))
        result=self.collect()[0][1]
        self.assertNotIn(b'z',result)
    def test_expanding_sanitized_text_is_bounded(self):
        clean=gate.sanitize(b'http://x\n'*16000)
        self.assertGreater(len(clean),gate.MAX_CAPTURE_BYTES)
        bounded,truncated=gate.bound_capture(clean)
        self.assertTrue(truncated);self.assertLessEqual(len(bounded),gate.MAX_CAPTURE_BYTES)
    def test_total_sanitized_cap(self):
        with patch.object(gate,'MAX_TOTAL_CAPTURE_BYTES',1):
            with self.assertRaises(gate.Refused):self.collect()
    def test_control_sequences_paths_and_unicode_redacted(self):
        clean=gate.sanitize(b'\x1b[31mE:\x00 /__w/_temp/private /home/user/auth\r\x1b[0m\n'+chr(0x202e).encode())
        for item in (b'\x1b',b'\x00',b'/__w',b'/home',chr(0x202e).encode()):self.assertNotIn(item,clean)
    def test_receipt_is_diagnostic_and_overwrite_refused(self):
        gate.preserve(self.artifact,self.collect(),'a'*40)
        dest=self.artifact/'apt-acquisition-diagnostics';r=json.loads((dest/'receipt.json').read_text())
        self.assertTrue(r['diagnostic_only']);self.assertFalse(r['acceptance_eligible']);self.assertFalse(r['raw_logs_exported'])
        for f in r['files']:self.assertEqual(hashlib.sha256((dest/f['name']).read_bytes()).hexdigest(),f['sanitized_sha256'])
        with self.assertRaises(FileExistsError):gate.preserve(self.artifact,self.collect(),'a'*40)
    def test_workflow_additive_failure_steps_and_test_count(self):
        p=Path(__file__).resolve().parents[2]/'.github/workflows/painter-native-ui.yml'
        s=p.read_text();self.assertEqual(s.count('name: Preserve bounded APT acquisition diagnostics'),2);self.assertEqual(s.count("if: ${{ failure() && steps.runtime_seal.outcome == 'success' && steps.native_driver.outcome == 'failure' }}"),2)
        self.assertEqual(s.count('python3 -B tools/brush-ci/test_capture_apt_failure.py'),2)
        self.assertEqual(s.count('timeout-minutes: 90'),2)
        self.assertIn('target: [candidate, baseline-carrier-red, dockbook-controlled-order-3d6f]',s);self.assertIn('target: [positive, baseline-red, sanitizer]',s)

if __name__=='__main__':unittest.main()
