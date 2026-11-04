import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def module(name):
    spec=importlib.util.spec_from_file_location(name,HERE/(name+'.py'))
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
collector=module('collect');generator=module('generate')
class OwnerTests(unittest.TestCase):
    def test_overlay_preserves_primary(self):
        originals={name:(ROOT/'app/tests'/name).read_bytes() for name in
                   ('test-painter-compact-options.c','test-painter-transform-popup.inc','test-painter-brush-thumbnail-popup.inc')}
        with tempfile.TemporaryDirectory() as directory:
            generator.generate(ROOT,directory)
            out=Path(directory)
            main=(out/'readiness-owner-main.c').read_text()
            self.assertEqual(main.replace('readiness-owner-transform.inc','test-painter-transform-popup.inc').replace('readiness-owner-brush.inc','test-painter-brush-thumbnail-popup.inc'),originals['test-painter-compact-options.c'].decode())
            helper=(out/'readiness-owner-transform.inc').read_text()
            for addition in ('#include "readiness-owner-icon.inc"\n','  icon_observe_begin ();\n','  icon_observe_finish ();\n','#include "readiness-owner-phase.inc"\n','#include "readiness-owner-leaf.inc"\n','#include "readiness-owner-image.inc"\n','  image_observe_begin ();\n','  image_observe_finish ();\n','  leaf_observe_begin (button, window);\n','  leaf_observe_finish ();\n','#include "readiness-owner-probe.inc"\n','  phase_observe_mark (10);\n','  phase_observe_mark (11);\n','  phase_observe_mark (12);\n','  phase_observe_finish ();\n','  owner_observe_begin (button, window);\n',
                '    owner_observe_dispatch (TRUE);\n','    owner_observe_dispatch (FALSE);\n','  owner_observe_finish ();\n'):
                self.assertEqual(helper.count(addition),1);helper=helper.replace(addition,'')
            self.assertEqual(helper,originals['test-painter-transform-popup.inc'].decode())
            brush=(out/'readiness-owner-brush.inc').read_text()
            for old,new in reversed(generator.BRUSH_PHASES):
                self.assertEqual(brush.count(new),1);brush=brush.replace(new,old)
            self.assertEqual(brush,originals['test-painter-brush-thumbnail-popup.inc'].decode())
        for name,expected in originals.items():self.assertEqual((ROOT/'app/tests'/name).read_bytes(),expected)
    def test_fail_closed_anchors(self):
        for text in ('none','aa'):
            with self.assertRaises(ValueError):generator.once(text,'a','b')
    def test_stream_rejects_secrets(self):
        r=collector.Records()
        for line in ('~"OWNER_DIAG complete"','secret title','OWNER_DIAG window 0 1 1 1 /private/title','OWNER_DIAG event 0 1 0x123'):
            r.consume(line)
        self.assertEqual(r.rows,[]);self.assertEqual(r.result()['status'],'inconclusive')
    def test_finish_alone_not_success(self):
        r=collector.Records();r.consume('OWNER_DIAG complete');self.assertEqual(r.result()['status'],'inconclusive')
    def test_real_gtk_signal_contract(self):
        import ctypes as c
        g=c.CDLL('libgobject-2.0.so.0');gtk=c.CDLL('libgtk-3.so.0');gdk=c.CDLL('libgdk-3.so.0')
        class Query(c.Structure):
            _fields_=[('id',c.c_uint),('name',c.c_char_p),('itype',c.c_ulong),('flags',c.c_uint),
                      ('result',c.c_ulong),('n',c.c_uint),('params',c.c_void_p)]
        g.g_type_class_ref.argtypes=[c.c_ulong];g.g_type_class_ref.restype=c.c_void_p
        g.g_type_class_unref.argtypes=[c.c_void_p]
        g.g_signal_lookup.argtypes=[c.c_char_p,c.c_ulong];g.g_signal_lookup.restype=c.c_uint
        g.g_signal_query.argtypes=[c.c_uint,c.POINTER(Query)]
        for lib,typename,signal in ((gtk,'gtk_container_get_type',b'check-resize'),(gdk,'gdk_frame_clock_get_type',b'layout')):
            get_type=getattr(lib,typename);get_type.restype=c.c_ulong
            kind=get_type();klass=g.g_type_class_ref(kind)
            try:
                query=Query();g.g_signal_query(g.g_signal_lookup(signal,kind),c.byref(query))
                self.assertNotEqual(query.id,0);self.assertFalse(query.flags&64)
                self.assertEqual(query.n,0);self.assertEqual(query.result,4)
            finally:g.g_type_class_unref(klass)
    def test_actual_gobject_lifetimes(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);c=d/'proof.c';exe=d/'proof'
            c.write_text('\n'.join((HERE/n).read_text() for n in ('synthetic-prefix.c','probe.inc','synthetic-suffix.c')))
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(c),'-o',str(exe),'-l:libgobject-2.0.so.0','-l:libglib-2.0.so.0'],check=True)
            run=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
            print(run.stderr.strip())
            self.assertNotIn('# OWNER_DIAG ', run.stdout)
            self.assertIn('OWNER_DIAG complete\n', run.stdout)
            r=collector.Records()
            for line in run.stdout.splitlines():r.consume(line)
            self.assertEqual(r.result()['status'],'captured')
            self.assertEqual(len(r.rows),2+16+18+32)
            duplicate=collector.Records();duplicate.consume('@'+json.dumps(run.stdout));self.assertEqual(duplicate.result(),r.result())
            duplicate.consume('OWNER_DIAG complete');self.assertEqual(duplicate.result()['status'],'inconclusive')
class TransportTests(unittest.TestCase):
    stream='\n'.join('OWNER_DIAG '+line for line in (
        'summary 1 2 0 0','window 0 1 1 1 1',
        'probe 0 0 1 1 1 0 0 0 0 0 0','probe 1 0 2 1 1 0 0 0 0 0 0','complete'))+'\n'
    def parse(self, chunks):
        r=collector.Records()
        for chunk in chunks:r.consume('@'+json.dumps(chunk))
        return r
    def test_every_split_and_character_chunks(self):
        expected=self.parse([self.stream]).result()
        self.assertEqual(expected['status'],'captured')
        for split in range(len(self.stream)+1):
            self.assertEqual(self.parse([self.stream[:split],self.stream[split:]]).result(),expected)
        self.assertEqual(self.parse(list(self.stream)).result(),expected)
    def test_every_truncation_fails_closed(self):
        for end in range(len(self.stream)):
            self.assertEqual(self.parse([self.stream[:end]]).result()['status'],'inconclusive')
    def test_no_cross_transport_join_or_console_spoof(self):
        r=collector.Records();r.consume('@'+json.dumps('OWNER_DIAG '))
        r.consume('~'+json.dumps('complete\n'));r.consume('&'+json.dumps('complete\n'))
        self.assertEqual(r.rows,[])
        r.consume('OWNER_DIAG complete');self.assertTrue(r.invalid)
        r=self.parse([self.stream]);r.consume('OWNER_DIAG complete');self.assertTrue(r.invalid)
        r=collector.Records();r.consume('OWNER_DIAG summary 1 2 0 0')
        r.consume('@'+json.dumps(self.stream.split('\n',1)[1]));self.assertTrue(r.invalid)
    def test_malformed_and_overlong_chunks_fail_closed(self):
        for chunk in ('@not-json','@42','@null','@'+json.dumps('x'*(collector.MAX_TARGET_LINE+1))):
            r=collector.Records();r.consume(chunk)
            self.assertTrue(r.invalid);self.assertLessEqual(len(r.pending),collector.MAX_TARGET_LINE)
            r.consume('@'+json.dumps(self.stream));self.assertEqual(r.result()['status'],'inconclusive')
    def test_stop_evidence_is_finite_and_identity_checked(self):
        def stop(reason,number):return ('','*','stopped',[('reason',reason),('bkptno',number)])
        self.assertEqual(collector.stop_evidence(stop('breakpoint-hit','2'),'2'),
                         {'reason':'breakpoint-hit','finish_breakpoint_matched':True})
        self.assertFalse(collector.stop_evidence(stop('breakpoint-hit','3'),'2')['finish_breakpoint_matched'])
        self.assertEqual(collector.stop_evidence(stop('/private/data','2'),'2'),
                         {'reason':'other','finish_breakpoint_matched':False})

class ControllerTests(unittest.TestCase):
    def test_same_child_finish_after_eight_samples(self):
        from unittest import mock
        import hashlib
        commands=[]
        class Fake:
            leaf_restore_failure=False
            image_count_mismatch=False
            icon_count_mismatch=False
            def __init__(self,*args):self.deadline=collector.base.time.monotonic()+180;self.started=False;self.samples=0;self.cleanup_evidence={'stage':'complete'}
            def command(self,cmd):
                commands.append(cmd)
                if cmd.startswith('-break-insert'):
                    func=cmd.split()[-1]
                    return ('1','^','done',[('bkpt',[('number','1'),('addr','0x123'),('func',func)])])
                if cmd.startswith('-stack-list'):
                    return ('1','^','done',[('stack', [('frame',[('func',name)]) for name in
                      (collector.base.FUNCTION,'brush_popup_open','brush_popup_private_target_release')])])
                if cmd=='-list-target-features':return ('1','^','done',[('features',['async'])])
                if cmd=='-exec-continue':
                    self.samples+=1
                return ('1','^','done',[])
            def stopped(self,timeout):
                if not self.started:self.started=True;return ('','*','stopped',[('reason','breakpoint-hit'),('bkptno','1'),('thread-id','1')])
                if self.samples==9:
                    phase_lines=['summary 1 12 0 0','window 0 1 1 0 1 0 0']+[f'event {i} 16 0 {i}' for i in range(1,13)]+['complete']
                    for line in phase_lines:collector.base.record('PHASE_DIAG '+line)
                    leaf_lines=['summary 1 1 1 1 0']+[f'family {i} 1 1 1 1 1 0 0 0 0 0' for i in range(1,4)]+[f'metric {i} {j} 0 0 0 0 0 0 0' for i in range(1,4) for j in range(5)]+['union 0 0 0 0 0 0 0','complete']
                    if self.leaf_restore_failure:leaf_lines[0]='summary 1 1 0 0 16'
                    for line in leaf_lines:collector.base.record('LEAF_DIAG '+line)
                    image_lines=['summary 1 1 0 0 0 0 0 0 0','meta 0 1 0 0 -1 -1 0 0 0 0 0 0 0 0','meta 0 2 0 0 -1 -1 0 0 0 0 0 0 0 0','metric 0 0 0 0 0 0 0 0','complete']
                    if self.image_count_mismatch:
                        image_lines=['summary 1 1 0 0 0 0 1 1 0','meta 0 1 1 5 1 -1 1 1 1 1 128 1 6 18','meta 0 2 1 5 1 -1 1 1 1 1 128 1 6 18','metric 0 0 1 1 0 0 1 1','complete']
                    for line in image_lines:collector.base.record('IMAGE_DIAG '+line)
                    icon_lines=['summary 1 1 0 0 0 0 0 0 0','overflow 0 0 0 0 0 0','complete']
                    for line in icon_lines:collector.base.record('ICON_DIAG '+line)
                    for line in ('summary 1 2 0 0','window 0 1 1 1 1',
                      'probe 0 0 1 1 1 0 0 0 0 0 0','probe 1 0 2 1 1 0 0 0 0 0 0','complete'):
                        collector.base.record('OWNER_DIAG '+line)
                    return ('','*','stopped',[('reason','breakpoint-hit'),('bkptno','1')])
                if timeout==0.5:return None
                return ('','*','stopped',[('reason','signal-received'),('signal-name','SIGINT')])
            def cleanup(self):commands.append('CLEANUP');return True
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'readiness-owner';binary.write_bytes(b'fake')
            with mock.patch.object(collector.base,'Controller',Fake):
                result=collector.collect(binary,hashlib.sha256(b'fake').hexdigest())
        self.assertEqual(result['status'],'captured');self.assertEqual(len(result['samples']),8)
        self.assertEqual(commands.count('-exec-continue'),9)
        self.assertEqual(result['last_observation_stop'],
                         {'reason':'breakpoint-hit','finish_breakpoint_matched':True})
        self.assertTrue(result['owned_child_cleanup_completed'])
        self.assertFalse(any('evaluate' in cmd or 'attach' in cmd or 'environment' in cmd for cmd in commands))
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'readiness-owner';binary.write_bytes(b'fake')
            with mock.patch.object(collector.base,'Controller',Fake),mock.patch.object(collector.phase_module.PhaseRecords,'result',return_value={'status':'inconclusive'}):
                partial=collector.collect(binary,hashlib.sha256(b'fake').hexdigest())
        self.assertEqual(partial['owner_observation']['status'],'captured')
        self.assertEqual(partial['status'],'inconclusive')
        commands.clear()
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'readiness-owner';binary.write_bytes(b'fake')
            with mock.patch.object(collector.base,'Controller',Fake),mock.patch.object(Fake,'leaf_restore_failure',True):
                unsafe=collector.collect(binary,hashlib.sha256(b'fake').hexdigest())
        self.assertEqual(unsafe['owner_observation']['status'],'captured')
        self.assertEqual(unsafe['leaf_observation']['status'],'inconclusive')
        self.assertFalse(unsafe['leaf_observation']['restoration_complete'])
        self.assertEqual(unsafe['status'],'inconclusive')
        self.assertTrue(unsafe['owned_child_cleanup_completed'])
        self.assertEqual(commands.count('-exec-continue'),9)
        self.assertEqual(commands[-1],'CLEANUP')
        commands.clear()
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'readiness-owner';binary.write_bytes(b'fake')
            with mock.patch.object(collector.base,'Controller',Fake),mock.patch.object(Fake,'image_count_mismatch',True):
                mismatch=collector.collect(binary,hashlib.sha256(b'fake').hexdigest())
        self.assertEqual(mismatch['status'],'inconclusive')
        self.assertEqual(mismatch['image_observation']['failure_category'],'Image and leaf call counts disagree')
        self.assertTrue(mismatch['owned_child_cleanup_completed'])
        self.assertEqual(commands.count('-exec-continue'),9)
        self.assertEqual(commands[-1],'CLEANUP')
    def test_icon_image_cross_stream_counts(self):
        image={'status':'captured','records':[{'values':[1,1,0,1,3,2,4,4,0]}]}
        icon={'status':'captured','records':[{'values':[1,1,0,2,2,4,4,0,0]}]}
        self.assertTrue(collector.icon_image_agree(image,icon))
        for index in (5,6):
            changed=json.loads(json.dumps(icon));changed['records'][0]['values'][index]=3
            self.assertFalse(collector.icon_image_agree(image,changed))
        self.assertTrue(collector.icon_image_agree({'status':'inconclusive'},icon))

    def test_timeout_and_cleanup_failure_are_inconclusive(self):
        from unittest import mock
        import hashlib
        class Fail:
            def __init__(self,*a):self.cleanup_evidence={'stage':'failed'}
            def command(self,*a):raise collector.base.DiagnosticError('Diagnostic timeout')
            def cleanup(self):return False
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'readiness-owner';binary.write_bytes(b'fake')
            with mock.patch.object(collector.base,'Controller',Fail):
                result=collector.collect(binary,hashlib.sha256(b'fake').hexdigest())
        self.assertEqual(result['status'],'inconclusive');self.assertFalse(result['owned_child_cleanup_completed'])
        self.assertEqual(result['owner_observation']['status'],'inconclusive')

if __name__=='__main__':unittest.main()
