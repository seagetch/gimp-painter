#!/usr/bin/env python3
"""Safe overlay/identity/orchestration checks; no GIMP, debugger or native build."""
import hashlib
import importlib.util
import json
import os
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest import mock
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('owner_identity',HERE/'readiness-owner/identity.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Overlay(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup)
        self.root=Path(self.t.name);self.src=self.root/'source';self.primary=self.root/'primary'
        self.base='original protected Meson\n';self.append='diagnostic target\n'
        for root in (self.src,self.primary):
            (root/'app/tests').mkdir(parents=True);(root/m.MESON).write_text(self.base)
        d=self.src/'tools/brush-ci/readiness-owner';d.mkdir(parents=True);(d/'meson.append').write_text(self.append)
        (self.src/m.MESON).write_text(self.base+self.append)
        self.j={'github_sha':'1'*40,'source_tree':'2'*40,'source':str(self.primary)}
    def command(self,args):
        if args[3:]==['rev-parse','HEAD']:return '1'*40+'\n' if args[2]==str(self.src) else '3'*40+'\n'
        if args[3:]==['diff','--name-only','HEAD']:return m.MESON+'\n'
        if args[3:]==['show','HEAD:'+m.MESON]:return self.base
        if args[3:]==['rev-parse','HEAD:gimp-data']:return '3'*40+'\n'
        if args[3] in ('ls-files','status'):return ''
        raise AssertionError(args)
    def test_generated_phase_overlay_is_separate_and_reversible(self):
        root=HERE.parents[1]
        names=('test-painter-compact-options.c','test-painter-transform-popup.inc','test-painter-brush-thumbnail-popup.inc')
        before={n:(root/'app/tests'/n).read_bytes() for n in names}
        with tempfile.TemporaryDirectory() as out:
            m.gen.generate(root,out)
            brush=(Path(out)/'readiness-owner-brush.inc').read_text()
            for old,new in reversed(m.gen.BRUSH_PHASES):
                self.assertEqual(brush.count(new),1);brush=brush.replace(new,old)
            self.assertEqual(brush,before[names[2]].decode())
            helper=(Path(out)/'readiness-owner-transform.inc').read_text()
            self.assertIn('3 * G_TIME_SPAN_SECOND',helper)
            self.assertLess(helper.index('phase_observe_finish ();'),helper.index('owner_observe_finish ();'))
            self.assertLess(helper.index('phase_observe_mark (11);'),helper.index('g_assert_cmpuint (layout.stable, >=, 3);'))
        self.assertEqual(before,{n:(root/'app/tests'/n).read_bytes() for n in names})
    def test_strict_phase_parser_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_phase_records.py')],check=True,capture_output=True,text=True)
    def test_strict_leaf_parser_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_leaf_records.py')],check=True,capture_output=True,text=True)
    def test_safe_leaf_forwarding_and_restoration_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_leaf.py')],check=True,capture_output=True,text=True)
    def test_leaf_overlay_closure_and_order(self):
        root=HERE.parents[1]
        with tempfile.TemporaryDirectory() as out:
            m.gen.generate(root,out)
            helper=(Path(out)/'readiness-owner-transform.inc').read_text()
            self.assertLess(helper.index('leaf_observe_begin (button, window);'),helper.index('layout.started ='))
            self.assertLess(helper.index('leaf_observe_finish ();'),helper.index('owner_observe_finish ();'))
            self.assertEqual((Path(out)/'readiness-owner-leaf.inc').read_bytes(),(HERE/'readiness-owner/leaf.inc').read_bytes())
        identity=(HERE/'readiness-owner/identity.py').read_text()
        for name in ('leaf.inc','leaf_records.py','readiness-owner-leaf.inc'):
            self.assertIn(name,identity)
        recipe=(HERE/'readiness-owner/meson.append').read_text()
        self.assertIn("files('../../tools/brush-ci/readiness-owner/leaf.inc')",recipe)
        self.assertIn("'readiness-owner-leaf.inc'",recipe)
    def test_strict_image_parser_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_image_records.py')],check=True,capture_output=True,text=True)
    def test_safe_image_identity_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_image.py')],check=True,capture_output=True,text=True)
    def test_image_overlay_closure_and_exact_leaf_forwarding(self):
        root=HERE.parents[1]
        with tempfile.TemporaryDirectory() as out:
            m.gen.generate(root,out)
            helper=(Path(out)/'readiness-owner-transform.inc').read_text()
            self.assertLess(helper.index('image_observe_begin ();'),helper.index('leaf_observe_begin (button, window);'))
            self.assertLess(helper.index('image_observe_begin ();'),helper.index('layout.started ='))
            self.assertLess(helper.index('image_observe_finish ();'),helper.index('leaf_observe_finish ();'))
            self.assertEqual((Path(out)/'readiness-owner-image.inc').read_bytes(),(HERE/'readiness-owner/image.inc').read_bytes())
        identity=(HERE/'readiness-owner/identity.py').read_text()
        for name in ('image.inc','image_records.py','readiness-owner-image.inc'):
            self.assertIn(name,identity)
        recipe=(HERE/'readiness-owner/meson.append').read_text()
        self.assertIn("files('../../tools/brush-ci/readiness-owner/image.inc')",recipe)
        self.assertIn("'readiness-owner-image.inc'",recipe)
        leaf=(HERE/'readiness-owner/leaf.inc').read_text()
        self.assertLess(leaf.index('ImageCall image_call = image_observe_enter (widget);'),leaf.index('leaf_observation.image_original (widget, minimum, natural);'))
        self.assertLess(leaf.index('leaf_observation.image_original (widget, minimum, natural);'),leaf.index('image_observe_exit (image_call);'))
        for addition in ('  IconCall icon_call = icon_observe_enter (widget);\n','  icon_observe_exit (icon_call);\n','  ImageCall image_call = image_observe_enter (widget);\n','  image_observe_exit (image_call);\n'):
            self.assertEqual(leaf.count(addition),1);leaf=leaf.replace(addition,'')
        self.assertEqual(hashlib.sha256(leaf.encode()).hexdigest(),'bc1754ff6dabe264157a7d5a65d36ee60cd6cc06f3c4790af70129c037666055')
    def test_exact_separate_append(self):
        with mock.patch.object(m.original,'command',side_effect=self.command):j=m.overlay(self.src,self.j)
        self.assertEqual(j['overlay_sha256'],hashlib.sha256((self.base+self.append).encode()).hexdigest())
        self.assertEqual((self.primary/m.MESON).read_text(),self.base)
    def test_primary_registration_mutation_rejected(self):
        (self.primary/m.MESON).write_text('changed')
        with mock.patch.object(m.original,'command',side_effect=self.command),self.assertRaises(m.original.Invalid):m.overlay(self.src,self.j)
    def test_extra_overlay_bytes_rejected(self):
        (self.src/m.MESON).write_text(self.base+self.append+'extra')
        with mock.patch.object(m.original,'command',side_effect=self.command),self.assertRaises(m.original.Invalid):m.overlay(self.src,self.j)
    def test_extra_source_change_rejected(self):
        def command(a):return m.MESON+'\napp/other.c\n' if a[3]=='diff' else self.command(a)
        with mock.patch.object(m.original,'command',side_effect=command),self.assertRaises(m.original.Invalid):m.overlay(self.src,self.j)
    def test_successful_primary_rejected(self):
        with mock.patch.object(m.original,'verify',return_value={'original_exit':0}),self.assertRaises(m.original.Invalid):m.snapshot('unused')
    def test_changed_private_identity_rejected(self):
        with mock.patch.object(m.original,'cleanpath',return_value=Path('/unused')),mock.patch.object(m.original,'readjson',return_value={'original_receipt':'x','binary':'old'}),mock.patch.object(m,'snapshot',return_value={'original_receipt':'x','binary':'new'}),self.assertRaises(m.original.Invalid):m.verify('unused')
    def test_strict_icon_parser_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_icon_records.py')],check=True,capture_output=True,text=True)
    def test_bounded_icon_request_suite(self):
        subprocess.run(['python3','-B',str(HERE/'readiness-owner/test_icon.py')],check=True,capture_output=True,text=True)
    def test_icon_overlay_closure_and_forwarding_order(self):
        root=HERE.parents[1]
        with tempfile.TemporaryDirectory() as out:
            m.gen.generate(root,out)
            helper=(Path(out)/'readiness-owner-transform.inc').read_text()
            self.assertLess(helper.index('#include "readiness-owner-image.inc"'),helper.index('#include "readiness-owner-icon.inc"'))
            self.assertLess(helper.index('#include "readiness-owner-icon.inc"'),helper.index('#include "readiness-owner-leaf.inc"'))
            self.assertLess(helper.index('image_observe_begin ();'),helper.index('icon_observe_begin ();'))
            self.assertLess(helper.index('icon_observe_begin ();'),helper.index('layout.started ='))
            self.assertLess(helper.index('icon_observe_finish ();'),helper.index('image_observe_finish ();'))
            self.assertEqual((Path(out)/'readiness-owner-icon.inc').read_bytes(),(HERE/'readiness-owner/icon.inc').read_bytes())
        identity=(HERE/'readiness-owner/identity.py').read_text()
        for name in ('icon.inc','icon_records.py','readiness-owner-icon.inc'):
            self.assertIn(name,identity)
        recipe=(HERE/'readiness-owner/meson.append').read_text()
        self.assertIn("files('../../tools/brush-ci/readiness-owner/icon.inc')",recipe)
        self.assertIn("'readiness-owner-icon.inc'",recipe)
        leaf=(HERE/'readiness-owner/leaf.inc').read_text()
        order=['ImageCall image_call = image_observe_enter (widget);','IconCall icon_call = icon_observe_enter (widget);','leaf_observation.image_original (widget, minimum, natural);','icon_observe_exit (icon_call);','image_observe_exit (image_call);']
        positions=[leaf.index(x) for x in order];self.assertEqual(positions,sorted(positions))
        for line in ('  IconCall icon_call = icon_observe_enter (widget);\n','  icon_observe_exit (icon_call);\n'):
            self.assertEqual(leaf.count(line),1);leaf=leaf.replace(line,'')
        self.assertEqual(hashlib.sha256(leaf.encode()).hexdigest(),'451188e12334aff2a5c4db533cf775164383b88cec355d96006a5725607b79b8')

    def test_combined_maximum_report_stays_within_unchanged_cap(self):
        def load(name):
            spec=importlib.util.spec_from_file_location(name,HERE/('readiness-owner/'+name+'.py'))
            module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
        im=load('image_records');ic=load('icon_records')
        # Serialization envelope, deliberately NOT a valid diagnostic capture.
        # Maximize every numeric field independently, including counters, changed
        # counts, hosts and unknown metadata; do not assume simultaneous maxima
        # are physically reachable. Valid parser fixtures are tested separately.
        count=im.MAX_COUNT; signed=im.INT_MAX; duration=im.INT64_MAX
        def row(kind,values): return {'kind':kind,'values':values}
        image_rows=[row('summary',[1,1,8191,16,4096,256,count,count,count])]
        image_rows += [row('window',[16,1]) for _ in range(im.MAX_WINDOWS)]
        metadata=[256,2,1,7,signed,signed,1,1,1,1,im.UINT_MAX,signed,6,18]
        metric=[256,2,count,count,count,count,duration,duration]
        for _ in range(im.MAX_IMAGES):
            image_rows += [row('image',[256,16,1,1,count])]
            image_rows += [row('meta',metadata) for _ in range(3)]
            image_rows += [row('metric',metric) for _ in range(2)]
        image_rows += [row('meta',metadata),row('meta',metadata),row('metric',metric),row('complete',[])]
        self.assertEqual(len(image_rows),im.MAX_RECORDS)
        icon_rows=[row('summary',[1,1,255,ic.MAX_NAMES,ic.MAX_GROUPS,count,count,count,count])]
        config=[ic.MAX_GROUPS,6,ic.MAX_NAMES,1,7,signed,signed,signed,ic.UINT_MAX,2,1,6,18,1]
        icon_rows += [row('group',config+[count,count,count,count,duration,duration]) for _ in range(ic.MAX_GROUPS)]
        icon_rows += [row('overflow',[count,count,count,count,duration,duration]),row('complete',[])]
        self.assertEqual(len(icon_rows),ic.MAX_RECORDS)
        frame={'thread_id':'9'*20,'function':'f'*180,'file':'x'*180,'line':99999999}
        stack={'frames':[frame]*96,'frame_limit':96,'truncation_possible':True,'geometry_wait_frame_present':True,'brush_open_frame_present':True,'selected_case_frame_present':True,'same_wait_invocation_as_entry_proven':False}
        # For old streams, overestimate every row by its largest field count
        # and all fields by UINT64_MAX; actual kinds have no longer spelling.
        def upper(rows,fields):
            return {'status':'inconclusive','records':[row('summary',[2**64-1]*fields) for _ in range(rows)]}
        report={'image_observation':{'status':'inconclusive','records':image_rows},
                'icon_observation':{'status':'inconclusive','records':icon_rows},
                'entry_stack':stack,'samples':[dict(stack,sample=i) for i in range(8)],
                'owner_observation':upper(82,11),'phase_observation':upper(274,7),
                'leaf_observation':upper(21,11),'reserved_other_fields':'x'*16384}
        # 16KiB reserve covers all observation descriptions, coverage booleans,
        # error/cleanup/identity/stop metadata outside the bounded record arrays.
        encoded=json.dumps(report,indent=2).encode()
        self.assertLess(len(encoded),1024*1024)
        base=(HERE/'sample_case14.py').read_text()
        self.assertIn('MAX_REPORT_BYTES = 1024 * 1024',base)
        self.assertIn('MAX_WIRE = 2 * 1024 * 1024',base)


class Budget(unittest.TestCase):
    def budget(self):
        return dict(started_monotonic=1000.0,step_seconds=1200,controller_seconds=180,cleanup_seconds=9,outer_margin_seconds=30)
    def test_exact_controller_and_cleanup_reserve(self):
        self.assertTrue(m.observation_admitted(self.budget(),1981.0))
        self.assertFalse(m.observation_admitted(self.budget(),1981.001))
        self.assertFalse(m.observation_admitted(self.budget(),999))
    def test_no_nan_or_reduced_reserve(self):
        for key,value in [('started_monotonic',float('nan')),('cleanup_seconds',0),('outer_margin_seconds',0),('controller_seconds',179),('step_seconds',1201)]:
            b=self.budget();b[key]=value
            with self.assertRaises(m.original.Invalid):m.observation_admitted(b,1001)
    def test_guard_is_after_identity_and_immediately_before_exec(self):
        text=(HERE/'readiness-owner/identity.py').read_text()
        begin=text.index("if a.mode=='wrapper':")
        self.assertGreater(text.index("if not observation_admitted",begin),text.index('original.digest(controller)',begin))
        self.assertLess(text.index("if not observation_admitted",begin),text.index('os.execv',begin))

class Orchestration(unittest.TestCase):
    def test_fresh_build_only(self):
        s=(HERE/'readiness-owner/run.sh').read_text()
        self.assertIn('src="$private/source"; build="$private/build"',s)
        self.assertIn('meson setup "$build" "$src"',s)
        self.assertIn('meson compile -C "$build" -j 2',s)
        self.assertNotIn('meson compile -C "$primary_build"',s)
        self.assertLess(s.index("assert j['original_exit'] == 1"),s.index('worktree add'))
        self.assertIn('--no-rebuild',s)
        self.assertIn('original_driver_exit_unchanged',s)
    def test_replaces_slot_with_explicit_diagnostic_compute_only_expansion(self):
        s=(HERE.parents[1]/'.github/workflows/painter-native-ui.yml').read_text()
        self.assertEqual(s.count('readiness-owner/run.sh'),1)
        self.assertIn('test_readiness_owner_integration.py Overlay Orchestration Budget',s)
        self.assertNotIn('run_preframe_diagnostic.sh',s)
        start=s.index('- name: Isolated failed-primary');part=s[start:s.index('- name: Preserve bounded APT',start)]
        self.assertIn('timeout-minutes: 20',part)
        runner=(HERE/'readiness-owner/run.sh').read_text()
        self.assertIn('float(sys.argv[1])+900-time.monotonic()',runner)
        self.assertEqual(runner.count('timeout --kill-after=5s'),2)
        self.assertIn("steps.native_driver.outcome == 'failure'",part)
        self.assertNotIn("native_driver.outcome == 'success'",part)
    def test_missing_primary_receipt_is_inconclusive_without_build_or_debugger(self):
        with tempfile.TemporaryDirectory() as t:
            env={**os.environ,'GITHUB_ACTIONS':'true','LD_PRELOAD':'','LD_AUDIT':''}
            result=subprocess.run(['bash',str(HERE/'readiness-owner/run.sh'),t],env=env,capture_output=True,text=True)
            self.assertEqual(result.returncode,0)
            receipt=json.loads((Path(t)/'readiness-owner-diagnostic/status.json').read_text())
            self.assertEqual(receipt['stage'],'identity-unavailable')
            self.assertEqual(receipt['state'],'finished')
            self.assertNotEqual(receipt['diagnostic_step_exit'],0)
            self.assertIs(receipt['acceptance_eligible'],False)
    def test_nonhost_rejected_before_artifact_creation(self):
        with tempfile.TemporaryDirectory() as t:
            result=subprocess.run(['bash',str(HERE/'readiness-owner/run.sh'),t],env={**os.environ,'GITHUB_ACTIONS':'false'},capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertEqual(list(Path(t).iterdir()),[])
    def test_protected_registration_is_unmodified(self):
        r=HERE.parents[1];pins=json.loads((HERE/'pins.json').read_text())
        path='app/tests/meson.build';self.assertEqual(hashlib.sha256((r/path).read_bytes()).hexdigest(),pins['common_protected_file_sha256'][path])

def load_tests(loader, tests, pattern):
    spec=importlib.util.spec_from_file_location('readiness_owner_tests',HERE/'readiness-owner/test_owner.py')
    owner=importlib.util.module_from_spec(spec);spec.loader.exec_module(owner)
    tests.addTests(loader.loadTestsFromModule(owner))
    for name in ('test_phase','test_phase_records'):
        spec=importlib.util.spec_from_file_location(name,HERE/'readiness-owner'/('tests/test_phase.py' if name=='test_phase' else name+'.py'))
        child=importlib.util.module_from_spec(spec);spec.loader.exec_module(child)
        tests.addTests(loader.loadTestsFromModule(child))
    return tests

if __name__=='__main__':unittest.main()
