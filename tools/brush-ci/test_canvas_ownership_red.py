#!/usr/bin/env python3
"""Synthetic contract tests; not native old-provider execution proof."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('canvas_red', Path(__file__).with_name('canvas_ownership_red.py'))
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)


def row(case=m.CASES[0]):
    error = 'ERROR:../source/app/tests/color-ownership/test-canvas.c:89:padding_lifetime: assertion failed (last_gone == 1): (0 == 1)'
    return dict(name='gimp:painter / canvas-ownership-old-provider', result='EXPECTEDFAIL', returncode=-6,
                command=['/build/app/tests/color-ownership/canvas-ownership-old-provider', '-p', case],
                stdout='TAP version 14\n# Start of ownership tests\n# Start of canvas tests\nBail out! '+error+'\n',
                stderr='**\n'+error+'\n', duration=1.2)


class Cases(unittest.TestCase):
    def check(self, r, valid=False, case=m.CASES[0]):
        self.assertEqual(m.check_case([r], case)['control_passed'], valid)

    def test_both_exact_success_contracts(self):
        for case in m.CASES: self.check(row(case), True, case)

    def test_current_glib_not_ok_bailout(self):
        for case in m.CASES:
            r=row(case);r['stdout']=r['stdout'].replace('Bail out! ERROR:', 'not ok '+case+' - ERROR:')+'Bail out!\n'
            self.check(r, True, case)

    def test_not_ok_wrong_leaf(self):
        r=row();r['stdout']=r['stdout'].replace('Bail out! ERROR:', 'not ok '+m.CASES[1]+' - ERROR:')+'Bail out!\n';self.check(r)

    def test_missing_bailout(self):
        r=row();r['stdout']=r['stdout'].replace('Bail out! ERROR:', 'ERROR:');self.check(r)

    def test_wrong_results(self):
        for result in ('OK', 'SKIP', 'TIMEOUT', 'FAIL', 'UNEXPECTEDPASS', None):
            r=row();r['result']=result;self.check(r)

    def test_wrong_exit(self):
        for code in (0, 1, 6, 134, -11, -15, True, '-6', None):
            r=row();r['returncode']=code;self.check(r)

    def test_wrong_target_and_command(self):
        for change in ('name', 'binary', 'extra', 'skip', 'otherleaf', 'prefix', 'wrapper', 'string', 'missing'):
            r=row()
            if change=='name': r['name']='gimp:painter / canvas-ownership'
            elif change=='binary':r['command'][0]='/build/canvas-ownership'
            elif change=='extra':r['command']+=['--verbose']
            elif change=='skip':r['command']+=['-s',m.CASES[0]]
            elif change=='otherleaf':r['command'][-1]=m.CASES[1]
            elif change=='prefix':r['command'][-1]='/ownership/canvas'
            elif change=='wrapper':r['command'].insert(0,'xvfb-run')
            elif change=='string':r['command']=' '.join(r['command'])
            else:r.pop('command')
            self.check(r)

    def test_no_zero_multiple_or_missing_rows(self):
        for rows in ([],[row(),row()],['bad']):self.assertFalse(m.check_case(rows,m.CASES[0])['control_passed'])
        self.check(row(),case='/ownership/canvas/not-a-case')

    def test_wrong_assertion(self):
        for old,new in [('last_gone','old_gone'),('(0 == 1)','(2 == 1)'),('padding_lifetime','unrelated'),('test-canvas.c','other.c')]:
            r=row();r['stdout']=r['stdout'].replace(old,new);r['stderr']=r['stderr'].replace(old,new);self.check(r)

    def test_missing_assertion_and_tap(self):
        for key in ('stdout','stderr'):
            r=row();r[key]='';self.check(r)
            r=row();r[key]=None;self.check(r)
        for token in ('TAP version 14','# Start of ownership tests','# Start of canvas tests'):
            r=row();r['stdout']=r['stdout'].replace(token,'');self.check(r)

    def test_unrelated_diagnostics(self):
        for extra in ('ERROR: other failure','AddressSanitizer: heap-use-after-free','LeakSanitizer: detected memory leaks','runtime error: overflow','GLib-CRITICAL bad','WARNING bad','Traceback','timeout','ok 1 /ownership/canvas/unrealized-padding','not ok 1 unrelated','1..0','# SKIP no display'):
            r=row();r['stderr']+='\n'+extra+'\n';self.check(r)

    def test_duplicate_assertion(self):
        r=row();r['stderr']+=r['stderr'];self.check(r)


class Provider(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.source=self.root/'source';self.build=self.root/'build'
        self.fixture=self.source/m.FIXTURE;self.test=self.source/m.TEST
        self.fixture.parent.mkdir(parents=True);self.fixture.write_text('old-canvas\n');self.test.write_text('candidate-test\n')
        self.old=self.build/'app/tests/color-ownership/canvas-ownership-old-provider.p/fixtures_gimpcanvas-c9.c.o'
        self.ot=self.old.parent/'test-canvas.c.o'
        self.ct=self.build/'app/tests/color-ownership/canvas-ownership.p/test-canvas.c.o'
        self.binary=self.build/'app/tests/color-ownership/canvas-ownership-old-provider'
        for p in (self.old,self.ot,self.ct,self.binary):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'compiled evidence')
        self.cc=[]
        for src,obj in ((self.fixture,self.old),(self.test,self.ot),(self.test,self.ct)):
            self.cc.append(dict(directory=str(self.build),file=str(src),command='cc -c '+str(src)+' -o '+str(obj)))
        self.options=[dict(name='b_sanitize',value='none')]
        self.map=self.build/'app/tests/color-ownership/canvas-ownership-old-provider.map'
        self.text='LOAD '+str(self.old)+'\nLOAD libappdisplay.a\nOUTPUT('+str(self.binary)+' elf64-x86-64)\nCross Reference Table\nSymbol File\n'+''.join(api+' '+str(self.old)+'\n' for api in m.APIS)
        self.addCleanup(patch.stopall)
        patch.object(m,'OLD_SHA',m.sha(self.fixture)).start();patch.object(m,'TEST_SHA',m.sha(self.test)).start()

    def check(self,valid=False):
        (self.build/'compile_commands.json').write_text(json.dumps(self.cc));(self.build/'meson-info').mkdir(exist_ok=True)
        (self.build/'meson-info/intro-buildoptions.json').write_text(json.dumps(self.options));self.map.write_text(self.text)
        self.assertEqual(m.check_provider(self.source,self.build,self.map)['control_passed'],valid)

    def test_identical_bytes_in_wrong_checkout_do_not_match_actual_compile_sources(self):
        self.check(True)
        wrong=self.root/'original-checkout'
        for source,relative in ((self.fixture,m.FIXTURE),(self.test,m.TEST)):
            target=wrong/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(source.read_bytes())
        rejected=m.check_provider(wrong,self.build,self.map)
        self.assertFalse(rejected['control_passed'])
        self.assertEqual(rejected['objects'],{})
        records=rejected['compile_input_diagnostics']
        self.assertEqual(len(records),3)
        self.assertTrue(all(not row['source_identity_matches'] for row in records))
        self.assertFalse(rejected['compile_input_diagnostics_truncated'])
        for key in ('old','old_test','candidate_test'):
            self.assertIn('Expected one exact compile entry: '+key,rejected['errors'])
        self.assertTrue(m.check_provider(self.source,self.build,self.map)['control_passed'])

    def test_diagnostics_are_bounded_and_do_not_export_compiler_flags(self):
        self.cc[0]['command'] += ' -DSECRET_SENTINEL=do-not-export'
        self.check(True)
        result=m.check_provider(self.source,self.build,self.map)
        records=result['compile_input_diagnostics']
        self.assertEqual(len(records),3)
        self.assertTrue(all(row['source_identity_matches'] for row in records))
        self.assertNotIn('SECRET_SENTINEL',json.dumps(result))
        for record in records:
            self.assertEqual(set(record),{'key','expected_source','observed_source','output_object','source_identity_matches'})
        self.cc.extend([self.cc[0]]*8)
        self.check(False)
        result=m.check_provider(self.source,self.build,self.map)
        self.assertEqual(len(result['compile_input_diagnostics']),6)
        self.assertTrue(result['compile_input_diagnostics_truncated'])
    def test_valid(self):self.check(True)
    def test_changed_fixture(self):self.fixture.write_text('new provider');self.check()
    def test_changed_test(self):self.test.write_text('old test');self.check()
    def test_unsanitized_lists(self):
        for value in ([], ['none']):self.options[0]['value']=value;self.check(True)
    def test_unknown_sanitized_lists(self):
        for value in (['address'],['none','address'],['none','none'],None,'',False):
            self.options[0]['value']=value;self.check()
    def test_map_symlink(self):
        self.check(True);real=self.map.with_suffix('.real');self.map.rename(real);self.map.symlink_to(real)
        with self.assertRaises(ValueError):m.check_provider(self.source,self.build,self.map)
    def test_source_symlink(self):
        self.check(True);real=self.fixture.with_suffix('.real');self.fixture.rename(real);self.fixture.symlink_to(real)
        with self.assertRaises(ValueError):m.check_provider(self.source,self.build,self.map)
    def test_binary_symlink(self):
        self.check(True);real=self.binary.with_suffix('.real');self.binary.rename(real);self.binary.symlink_to(real)
        with self.assertRaises(ValueError):m.check_provider(self.source,self.build,self.map)
    def test_map_wrong_location(self):
        self.check(True);other=self.build/'other.map';other.write_text(self.text)
        with self.assertRaises(ValueError):m.check_provider(self.source,self.build,other)
    def test_compile_output_outside_build(self):
        outside=self.root/'canvas-ownership-old-provider.p/fixtures_gimpcanvas-c9.c.o';outside.parent.mkdir();outside.write_text('bad')
        self.cc[0]['command']=self.cc[0]['command'].replace(str(self.old),str(outside));self.text=self.text.replace(str(self.old),str(outside))
        with self.assertRaises(ValueError):self.check()
    def test_sanitized_build(self):self.options[0]['value']='address,undefined';self.check()
    def test_missing_build_option(self):self.options=[];self.check()
    def test_sanitized_compile(self):self.cc[0]['command']+=' -fsanitize=address';self.check()
    def test_old_source_not_direct(self):self.cc=self.cc[1:];self.check()
    def test_duplicate_old_compile(self):self.cc.append(self.cc[0]);self.check()
    def test_missing_candidate_test(self):self.cc.pop();self.check()
    def test_wrong_old_test(self):self.cc[1]['file']=str(self.fixture);self.check()
    def test_missing_cref(self):self.text=self.text.split('Cross Reference Table')[0];self.check()
    def test_wrong_defining_object(self):self.text=self.text.replace('gimp_canvas_new '+str(self.old),'gimp_canvas_new libappdisplay.a(gimpcanvas.c.o)');self.check()
    def test_missing_api(self):self.text=self.text.replace('gimp_canvas_set_padding '+str(self.old)+'\n','');self.check()
    def test_duplicate_api(self):self.text+='gimp_canvas_new '+str(self.old)+'\n';self.check()
    def test_wrong_load(self):self.text=self.text.replace('LOAD '+str(self.old),'LOAD other.o');self.check()
    def test_wrong_binary(self):self.text=self.text.replace('OUTPUT('+str(self.binary),'OUTPUT(other');self.check()
    def test_archive_extraction(self):self.text+='\napp/display/libappdisplay.a(gimpcanvas.c.o)\n';self.check()
    def test_archive_prefixed_member(self):self.text+='\napp/display/libappdisplay.a(app_display_gimpcanvas.c.o)\n';self.check()
    def test_relative_map_and_argument_array(self):
        self.text=self.text.replace(str(self.build)+'/', '')
        import shlex
        for e in self.cc:e['arguments']=shlex.split(e.pop('command'))
        self.check(True)


class Aggregate(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.paths=[]
        self.binary=self.root/m.TARGET;self.binary.write_bytes(b'old executable')
        self.provider=self.root/'provider.json'
        proof=m.base('canvas-old-provider-provenance');proof.update(control_passed=True,binary_path=str(self.binary),binary_sha256=m.sha(self.binary))
        self.provider.write_text(json.dumps(proof))
        for i,case in enumerate(m.CASES):
            log=self.root/f'{i}.jsonl';raw=row(case);raw['command'][0]=str(self.binary);log.write_text(json.dumps(raw)+'\n')
            report=m.check_case([raw],case);report.update(log_path=str(log),log_sha256=m.sha(log))
            p=self.root/f'{i}-receipt.json';p.write_text(json.dumps(report));self.paths.append(p)
    def test_both(self):self.assertTrue(m.aggregate(self.paths,self.provider)['control_passed'])
    def test_one(self):self.assertFalse(m.aggregate(self.paths[:1],self.provider)['control_passed'])
    def test_duplicate(self):self.assertFalse(m.aggregate([self.paths[0]]*2,self.provider)['control_passed'])
    def test_changed_log(self):
        (self.root/'0.jsonl').write_text(json.dumps(row(m.CASES[1])));self.assertFalse(m.aggregate(self.paths,self.provider)['control_passed'])
    def test_forged_success(self):
        p=self.paths[0];r=json.loads(p.read_text());r['test']['returncode']=0;r['control_passed']=True
        # Changing the original log, even with a new hash, cannot turn wrong raw outcome into success.
        log=Path(r['log_path']);bad=row();bad['returncode']=0;log.write_text(json.dumps(bad));r['log_sha256']=m.sha(log);p.write_text(json.dumps(r));self.assertFalse(m.aggregate(self.paths,self.provider)['control_passed'])
    def test_changed_binary(self):
        self.binary.write_bytes(b'new executable');self.assertFalse(m.aggregate(self.paths,self.provider)['control_passed'])
    def test_invalid_provider_receipt(self):
        r=json.loads(self.provider.read_text());r['control_passed']=False;self.provider.write_text(json.dumps(r));self.assertFalse(m.aggregate(self.paths,self.provider)['control_passed'])
    def test_other_executable_path(self):
        p=self.paths[0];r=json.loads(p.read_text());log=Path(r['log_path']);raw=json.loads(log.read_text());other=self.root/'other'/m.TARGET;other.parent.mkdir();other.write_bytes(self.binary.read_bytes());raw['command'][0]=str(other);log.write_text(json.dumps(raw));r['log_sha256']=m.sha(log);p.write_text(json.dumps(r));self.assertFalse(m.aggregate(self.paths,self.provider)['control_passed'])
    def test_acceptance_eligible(self):
        p=self.paths[0];r=json.loads(p.read_text());r['acceptance_eligible']=True;p.write_text(json.dumps(r));self.assertFalse(m.aggregate(self.paths,self.provider)['control_passed'])

if __name__=='__main__':unittest.main()
