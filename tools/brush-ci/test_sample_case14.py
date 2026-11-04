import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

spec=importlib.util.spec_from_file_location('diag',Path(__file__).with_name('sample_case14.py'))
d=importlib.util.module_from_spec(spec);spec.loader.exec_module(d)

class TestMI(unittest.TestCase):
    def test_breakpoint(self):
        r=d.record('7^done,bkpt={number="1",addr="0x123",func="'+d.FUNCTION+'"}')
        self.assertEqual(d.field(d.field(r[3],'bkpt'),'func'),d.FUNCTION)
    def test_array(self):
        self.assertEqual(d.field(d.record('1^done,features=["async","exec"]')[3],'features'),['async','exec'])
    def test_drop_stream(self):
        self.assertIsNone(d.record('~"secret process arguments"'))
        self.assertIsNone(d.record('(gdb)'))
    def test_stack_filters(self):
        r=d.record('8^done,stack=[frame={level="0",addr="0x123",func="some_func",file="/private/path/foo.c",line="22",args=[{name="key",value="secret"}]}]')
        out=d.safe_stack(r[3],'1')
        self.assertEqual(out,[{'thread_id':'1','function':'some_func','file':'foo.c','line':22}])
        self.assertNotIn('private',json.dumps(out));self.assertNotIn('secret',json.dumps(out));self.assertNotIn('0x',json.dumps(out))
    def test_no_address_symbols(self):
        self.assertIsNone(d.safe_text('0x123'))
    def test_controls(self):
        self.assertIsNone(d.safe_text('foo\nbar'))
    def test_duplicate(self):
        with self.assertRaises(d.DiagnosticError): d.field(d.MI('a="1",a="2"').fields(),'a')
    def test_deep(self):
        with self.assertRaises(d.DiagnosticError): d.MI('a='+'['*18+'"a"'+']'*18).fields()
    def test_unclosed(self):
        with self.assertRaises(d.DiagnosticError): d.MI('a=["x"').fields()
    def test_unsupported_escape(self):
        with self.assertRaises(d.DiagnosticError): d.MI(r'a="\377"').fields()
    def test_line_bound(self):
        with self.assertRaises(d.DiagnosticError): d.MI('x'*(d.MAX_LINE+1))
    def test_malformed_containers(self):
        for value in ('text', 1, None, {}):
            with self.assertRaises(d.DiagnosticError): d.field(value,'x')
            with self.assertRaises(d.DiagnosticError): d.safe_stack([('stack',[('frame',value)])],'1')

    def test_stack_bound(self):
        with self.assertRaises(d.DiagnosticError): d.safe_stack([('stack',[('frame',[])]*(d.MAX_FRAMES+1))],'1')
    def test_wrong_case(self):
        with tempfile.NamedTemporaryFile() as f:
            import hashlib
            with self.assertRaises(d.DiagnosticError): d.sample('unused',f.name,['-p','/other'],hashlib.sha256(b'').hexdigest())
    def test_extra_case_arguments_rejected(self):
        import hashlib
        with tempfile.NamedTemporaryFile() as f:
            for extra in (['--run-prefix=/other'],['-x','/other'],['--GTestSubprocess'],['--verbose']):
                with self.assertRaises(d.DiagnosticError):
                    d.sample('unused',f.name,['-p',d.CASE]+extra,hashlib.sha256(b'').hexdigest())
    def test_missing_gdb_is_inconclusive(self):
        import hashlib
        with tempfile.NamedTemporaryFile() as f:
            r=d.sample('/no/such/gdb',f.name,['-p',d.CASE],hashlib.sha256(b'').hexdigest())
            self.assertEqual(r['status'],'inconclusive');self.assertEqual(r['failure_category'],'Debugger launch/setup failed')

    def test_wrong_hash(self):
        with tempfile.NamedTemporaryFile() as f:
            with self.assertRaises(d.DiagnosticError): d.sample('unused',f.name,['-p',d.CASE],'0'*64)
    def test_no_sanitizer_mutation(self):
        src=Path(d.__file__).read_text()
        self.assertNotIn('os.environ[',src);self.assertNotIn('set environment',src)
        self.assertNotIn('-target-attach',src);self.assertNotIn('ptrace_scope',src)
        self.assertNotIn('stack-list-arguments',src);self.assertNotIn('stack-list-variables',src)

class TestChildCleanup(unittest.TestCase):
    def test_owned_child_cleanup(self):
        # A real child of the fake debugger process, not a fake cleanup mock.
        code='import subprocess,sys,time\np=subprocess.Popen([sys.executable,"-c","import time; time.sleep(60)"])\nprint(\'=thread-group-started,id="i1",pid="%s"\'%p.pid,flush=True)\nfor line in sys.stdin:\n if "gdb-exit" in line:\n  p.wait(timeout=5);break\n'
        c=d.Controller([sys.executable,'-u','-c',code],timeout=5)
        try:
            rec=c.next();self.assertEqual(rec[2],'thread-group-started')
            pid=c.inferior;fd=os.pidfd_open(pid)
            self.assertIsNotNone(c.pidfd)
            c.cleanup()
            import selectors
            sel=selectors.DefaultSelector();sel.register(fd,selectors.EVENT_READ)
            self.assertTrue(sel.select(2),'owned inferior did not exit')
            sel.close();os.close(fd)
        finally:
            if c.p.poll() is None:c.cleanup()
    def test_pid_recycling_closes_unverified_fd_without_signal(self):
        from unittest import mock
        from types import SimpleNamespace
        c=object.__new__(d.Controller);c.p=SimpleNamespace(pid=123);c.pidfd=None;c.inferior=None
        def stat(parent,start):
            tail=['S',str(parent)]+['0']*17+[str(start)]+['0']*10
            return '456 (child) '+ ' '.join(tail)
        for second in (stat(999,11),stat(123,12)):
            with mock.patch.object(d.Path,'read_text',side_effect=[stat(123,11),second]), \
                 mock.patch.object(d.os,'pidfd_open',return_value=77), \
                 mock.patch.object(d.os,'close') as close, \
                 mock.patch.object(d.signal,'pidfd_send_signal') as kill:
                with self.assertRaises(d.DiagnosticError):c.own_inferior('456')
                self.assertIsNone(c.pidfd);self.assertIsNone(c.inferior)
                close.assert_called_once_with(77);kill.assert_not_called()

    def test_constructor_setup_failure_reaps_debugger(self):
        from unittest import mock
        original=d.subprocess.Popen;created=[]
        def launch(*a,**kw):
            p=original(*a,**kw);created.append(p);return p
        with mock.patch.object(d.subprocess,'Popen',side_effect=launch), \
             mock.patch.object(d.selectors,'DefaultSelector',side_effect=OSError('injected')):
            with self.assertRaises(OSError):
                d.Controller([sys.executable,'-u','-c','import time;time.sleep(60)'])
        self.assertEqual(len(created),1);self.assertIsNotNone(created[0].poll())
        self.assertTrue(created[0].stdout.closed)
    def test_additional_inferior_forces_incomplete(self):
        c=d.Controller([sys.executable,'-u','-c','import time;time.sleep(60)'])
        c.inferior=1
        with self.assertRaises(d.DiagnosticError):c.own_inferior('2')
        self.assertTrue(c.unknown_inferior);self.assertFalse(c.cleanup())

    def test_unrelated_pid_rejected(self):
        c=d.Controller([sys.executable,'-u','-c','import time; time.sleep(60)'],timeout=5)
        try:
            with self.assertRaises(d.DiagnosticError):c.own_inferior(str(os.getpid()))
            self.assertIsNone(c.pidfd)
        finally:c.cleanup()
    def test_command_timeout_cleanup(self):
        c=d.Controller([sys.executable,'-u','-c','import time; time.sleep(60)'],timeout=.1)
        with self.assertRaises(d.DiagnosticError):c.command('-gdb-version',seconds=.05)
        c.cleanup();self.assertIsNotNone(c.p.poll())
    def test_signal_failure_still_reaps_debugger(self):
        from unittest import mock
        c=d.Controller([sys.executable,'-u','-c','import time; time.sleep(60)'],timeout=5)
        c.run_requested=True;c.pidfd=os.open('/dev/null',os.O_RDONLY)
        with mock.patch.object(d.signal,'pidfd_send_signal',side_effect=PermissionError()):
            self.assertFalse(c.cleanup())
        self.assertIsNotNone(c.p.poll());self.assertTrue(c.p.stdout.closed)
        self.assertEqual(c.cleanup_evidence['pidfd_signal'], 'error')
        self.assertEqual(c.cleanup_evidence['pidfd_wait'], 'not-attempted')
        self.assertEqual(c.cleanup_evidence['debugger_final_wait'], 'observed')

    def test_unobserved_inferior_is_incomplete(self):
        c=d.Controller([sys.executable,'-u','-c','import time; time.sleep(60)'],timeout=5)
        c.run_requested=True
        self.assertFalse(c.cleanup())
    def test_pidfd_not_ready_is_incomplete(self):
        from unittest import mock
        c=d.Controller([sys.executable,'-u','-c','import time; time.sleep(60)'],timeout=5)
        c.run_requested=True
        # Independent fake readiness is never accepted as termination proof.
        c.pidfd=os.open('/dev/null',os.O_RDONLY)
        with mock.patch.object(d.signal,'pidfd_send_signal'), \
             mock.patch.object(d.selectors,'DefaultSelector') as sel:
            sel.return_value.__enter__.return_value.select.return_value=[]
            self.assertFalse(c.cleanup())
        self.assertEqual(c.cleanup_evidence['pidfd_wait'], 'timeout')
        self.assertEqual(c.cleanup_evidence['pidfd_final_wait'], 'timeout')
        self.assertTrue(c.cleanup_evidence['debugger_termination_observed'])
        self.assertFalse(c.cleanup_evidence['child_termination_observed'])

    def test_output_bound(self):
        c=d.Controller([sys.executable,'-u','-c',f'import sys,time;print("x"*{d.MAX_LINE+1},flush=True);time.sleep(10)'],timeout=5)
        try:
            with self.assertRaises(d.DiagnosticError):c.next()
        finally:c.cleanup()

class TestPostDebuggerReadiness(unittest.TestCase):
    def test_ordinary_child_exits_on_debugger_shutdown(self):
        # No ptrace: model delayed termination with one ordinary owned child.
        # Signal delivery is injected as a no-op; actual pidfd waits are real.
        from unittest import mock
        code = """import subprocess,sys
child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)'])
print('=thread-group-started,pid="%s"'%child.pid,flush=True)
try:
 for line in sys.stdin:
  if line.strip()=='-gdb-exit':
   child.kill();child.wait(timeout=3);break
finally:
 if child.poll() is None:child.kill();child.wait(timeout=3)
"""
        c=d.Controller([sys.executable,'-u','-c',code],timeout=10)
        c.run_requested=True
        try:
            c.next(time.monotonic()+3)
            self.assertIsNotNone(c.pidfd)
            with mock.patch.object(d.signal,'pidfd_send_signal'):
                self.assertTrue(c.cleanup())
            e=c.cleanup_evidence
            self.assertEqual(e['pidfd_wait'],'timeout')
            self.assertEqual(e['debugger_final_wait'],'observed')
            self.assertEqual(e['pidfd_final_wait'],'observed')
            self.assertEqual(e['pidfd_close'],'closed')
            self.assertTrue(e['child_termination_observed'])
        finally:
            if c.p.poll() is None:c.cleanup()
    def test_final_observation_error_is_not_success(self):
        from unittest import mock
        from types import SimpleNamespace
        c=object.__new__(d.Controller)
        c.run_requested=True;c.unknown_inferior=False
        c.pidfd=os.open('/dev/null',os.O_RDONLY)
        c.p=mock.Mock();c.p.poll.return_value=0
        c.selector=mock.Mock()
        first=mock.MagicMock();first.__enter__.return_value.select.return_value=[object()]
        second=mock.MagicMock();second.__enter__.return_value.register.side_effect=PermissionError()
        with mock.patch.object(d.signal,'pidfd_send_signal'), mock.patch.object(d.selectors,'DefaultSelector',side_effect=[first,second]):
            self.assertFalse(c.cleanup())
        self.assertEqual(c.cleanup_evidence['pidfd_wait'],'observed')
        self.assertEqual(c.cleanup_evidence['pidfd_final_wait'],'error')
        self.assertIsNone(c.pidfd)
    def test_initial_selector_value_error_still_shuts_down_and_closes(self):
        from unittest import mock
        c=object.__new__(d.Controller)
        c.run_requested=True;c.unknown_inferior=False
        c.pidfd=os.open('/dev/null',os.O_RDONLY)
        fd=c.pidfd
        c.p=mock.Mock();c.p.poll.return_value=None;c.selector=mock.Mock()
        first=mock.MagicMock();first.__enter__.return_value.register.side_effect=ValueError('private exception')
        second=mock.MagicMock();second.__enter__.return_value.select.return_value=[object()]
        with mock.patch.object(d.signal,'pidfd_send_signal'), mock.patch.object(d.selectors,'DefaultSelector',side_effect=[first,second]):
            self.assertTrue(c.cleanup())
        self.assertEqual(c.cleanup_evidence['pidfd_wait'],'error')
        self.assertEqual(c.cleanup_evidence['debugger_exit'],'requested')
        self.assertEqual(c.cleanup_evidence['pidfd_final_wait'],'observed')
        self.assertIsNone(c.pidfd)
        with self.assertRaises(OSError):os.fstat(fd)
        self.assertNotIn('private',json.dumps(c.cleanup_evidence))
    def test_shutdown_precedes_final_wait_and_descriptor_close(self):
        from unittest import mock
        events=[]
        c=object.__new__(d.Controller)
        c.run_requested=True;c.unknown_inferior=False;c.pidfd=123
        c.p=mock.Mock();c.p.poll.return_value=None
        c.p.wait.side_effect=lambda **kw: events.append('debugger-wait')
        c.selector=mock.Mock()
        first=mock.MagicMock();first.__enter__.return_value.select.side_effect=lambda n:events.append('initial-wait') or []
        second=mock.MagicMock();second.__enter__.return_value.select.side_effect=lambda n:events.append('final-wait') or [object()]
        with mock.patch.object(d.signal,'pidfd_send_signal'), mock.patch.object(d.selectors,'DefaultSelector',side_effect=[first,second]), mock.patch.object(d.os,'close',side_effect=lambda fd:events.append('close')):
            self.assertTrue(c.cleanup())
        self.assertEqual(events,['initial-wait','debugger-wait','debugger-wait','final-wait','close'])

class TestStackEvidence(unittest.TestCase):
    def test_cap_is_possible_truncation_not_complete_stack(self):
        data=[('stack', [('frame',[('func','unknown')])]*d.MAX_FRAMES)]
        result=d.stack_evidence(data,'1')
        self.assertTrue(result['truncation_possible'])
        self.assertFalse(result['geometry_wait_frame_present'])
        self.assertFalse(result['same_wait_invocation_as_entry_proven'])
    def test_present_caller_does_not_prove_same_invocation(self):
        data=[('stack', [('frame',[('func',d.FUNCTION)])])]
        result=d.stack_evidence(data,'1')
        self.assertTrue(result['geometry_wait_frame_present'])
        self.assertFalse(result['same_wait_invocation_as_entry_proven'])
        self.assertFalse(result['truncation_possible'])

class TestReportBounds(unittest.TestCase):
    def test_thread_id_bound(self):
        for value in ('1'*21, '0', '-1', 1):
            with self.subTest(value=value):
                with self.assertRaises(d.DiagnosticError): d.safe_stack([('stack',[])],value)
        self.assertEqual(d.safe_stack([('stack',[])],'1'*20),[])
    def test_report_cap_preserves_cleanup_status(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'report.json'
            for done in (True,False):
                original={'status':'captured','owned_child_cleanup_completed':done,'private':'X'*(d.MAX_REPORT_BYTES+1)}
                result=d.write_report(p,original)
                self.assertEqual(result['status'],'inconclusive')
                self.assertIs(result['owned_child_cleanup_completed'],done)
                self.assertFalse(result['acceptance_eligible'])
                self.assertNotIn('private',p.read_text())
                self.assertLessEqual(p.stat().st_size,d.MAX_REPORT_BYTES)
    def test_max_bounded_frames_fit_report(self):
        fields=[('stack',[('frame',[('func','x'*180),('file','f'*180),('line','12345678')])]*96)]
        stack=d.stack_evidence(fields,'1'*20)
        report={'entry_stack':stack,'samples':[stack]*8,'status':'captured','owned_child_cleanup_completed':True}
        with tempfile.TemporaryDirectory() as td:
            result=d.write_report(Path(td)/'report.json',report)
            self.assertEqual(result['status'],'captured')

class TestEndToEndSimulation(unittest.TestCase):
    def test_full_mi_controller_and_cleanup(self):
        import hashlib
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);binary=root/'dummy';binary.write_bytes(b'only hash identity, never run')
            fake=root/'fake-gdb'
            fake.write_text("""#!/usr/bin/env python3
import re,subprocess,sys
child=None
for line in sys.stdin:
 m=re.match(r'(\\d*)(.*)',line.strip()); token,cmd=m.groups()
 if cmd=='-gdb-exit':
  if child:child.wait(timeout=5)
  break
 if cmd.startswith('-break-insert'):
  print(token+'^done,bkpt={number="1",addr="0x123",func="transform_popup_wait_stable_geometry_observed"}',flush=True)
 elif cmd=='-exec-run':
  child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)'])
  print('=thread-group-started,id="i1",pid="%s"'%child.pid,flush=True)
  print(token+'^running',flush=True)
  print('*stopped,reason="breakpoint-hit",bkptno="1",thread-id="1"',flush=True)
 elif cmd=='-list-target-features':print(token+'^done,features=["async"]',flush=True)
 elif cmd=='-exec-continue':
  print(token+'^running',flush=True)
  print('*stopped,reason="signal-received",signal-name="SIGINT",thread-id="1"',flush=True)
 elif cmd.startswith('-stack-list-frames'):
  print(token+'^done,stack=[frame={level="0",addr="0x123",func="expensive_callback",file="/private/source/test.c",line="42"}]',flush=True)
 else:print(token+'^done',flush=True)
""")
            fake.chmod(0o700)
            r=d.sample(str(fake),str(binary),['-p',d.CASE],hashlib.sha256(binary.read_bytes()).hexdigest(),timeout=10)
            self.assertEqual(r['status'],'captured',r)
            self.assertEqual(len(r['samples']),8)
            self.assertTrue(r['owned_child_cleanup_completed'])
            self.assertFalse(r['acceptance_eligible'])
            self.assertIn('entry_stack',r)
            self.assertFalse(r['entry_stack']['geometry_wait_frame_present'])
            self.assertFalse(r['entry_stack']['truncation_possible'])
            self.assertEqual(r['cleanup_evidence']['pidfd_wait'], 'observed')
            self.assertNotIn('0x123',json.dumps(r));self.assertNotIn('/private/',json.dumps(r))

if __name__=='__main__':unittest.main(verbosity=2)
