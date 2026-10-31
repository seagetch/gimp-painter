import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('collector',Path(__file__).with_name('collect_matched_preclick.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def stdout():
    fields={k:0 for k in m.FIELDS}
    fields.update(primary_elapsed_us=13000000,primary_cpu_us=12000000,primary_frames=1,primary_iterations=3,longest_iteration_us=12900000,first_frame_us=12999000,final_elapsed_us=13000001,final_frames=1,initial_mapped=1,initial_realized=1,initial_viewable=1,setup_us=42,factory_us=20,show_drain_us=20)
    phases=[f'# Geometry timing wait=1 probe={label} hooked=1 entries=1 exits=1 unmatched=0 orphan-exits=0 total-us=2 longest-us=2' for label in sorted(m.LABELS)]
    return '\n'.join(['TAP version 14',*phases,'# MATCHED_PRECLICK '+' '.join(f'{k}={v}' for k,v in fields.items()),'# MATCHED_FRAME ordinal=1 elapsed_us=12999000 stable=0','# MATCHED_FRAMES truncated=0','ok 1 '+m.PRIMARY,'1..1'])+'\n'

def record():return {'name':'gimp:diagnostic-only / matched-preclick','result':'OK','returncode':0,'stdout':stdout(),'stderr':''}

class Collection(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.path=Path(self.temp.name)/'log.json'
    def run_record(self,r,**kw):
        self.path.write_text(json.dumps(r)+'\n')
        return m.collect(self.path,kw.get('cell','baseline-normal'),kw.get('rep',1),kw.get('order',1),kw.get('kind','primary'))
    def test_complete_collection_keeps_failed_primary(self):
        j=self.run_record(record());self.assertTrue(j['comparison_eligible']);self.assertFalse(j['metrics']['original_primary_pass']);self.assertTrue(j['metrics']['elapsed_over_3s']);self.assertFalse(j['acceptance_eligible'])
    def test_raw_bytes_digest_and_producer(self):
        j=self.run_record(record())
        self.assertEqual(j['raw_log_sha256'],m.hashlib.sha256(self.path.read_bytes()).hexdigest())
        self.assertEqual(j['collector_sha256'],m.hashlib.sha256(Path(m.__file__).read_bytes()).hexdigest())
    def test_duplicate_summary(self):
        r=record();r['stdout']+=next(x for x in r['stdout'].splitlines() if 'MATCHED_PRECLICK' in x)+'\n';self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_no_frame(self):
        r=record();r['stdout']=r['stdout'].replace('# MATCHED_FRAME ordinal=1 elapsed_us=12999000 stable=0\n','');self.assertFalse(self.run_record(r)['metrics_available'])
    def test_missing_final_marker(self):
        r=record();r['stdout']=r['stdout'].replace('# MATCHED_FRAMES truncated=0\n','');self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_bad_primary_upgrade(self):
        r=record();r['stdout']=r['stdout'].replace('primary_pass=0','primary_pass=1');self.assertFalse(self.run_record(r)['metrics_available'])
    def test_extended_rejected(self):
        r=record();r['stdout']=r['stdout'].replace('extended=0','extended=1');self.assertFalse(self.run_record(r)['metrics_available'])
    def test_wrong_leaf(self):
        r=record();r['stdout']=r['stdout'].replace(m.PRIMARY,'/private/secret');j=self.run_record(r);self.assertFalse(j['comparison_eligible']);self.assertNotIn('secret',json.dumps(j))
    def test_extra_test(self):
        r=record();r['stdout']+='ok 2 '+m.PRIMARY+'\n';self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_skip_and_timeout(self):
        for result,code in [('SKIP',77),('TIMEOUT',-15)]:
            r=record();r.update(result=result,returncode=code);self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_skip_tap(self):
        r=record();r['stdout']=r['stdout'].replace('ok 1 '+m.PRIMARY,'ok 1 '+m.PRIMARY+' # SKIP unavailable');self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_leak_metrics_not_clean(self):
        r=record();r.update(result='FAIL',returncode=1,stderr='ERROR: LeakSanitizer: detected memory leaks\n/private/path 0x123\nSUMMARY: AddressSanitizer: 7 byte(s) leaked in 1 allocation(s)\n')
        j=self.run_record(r);self.assertTrue(j['metrics_available']);self.assertFalse(j['process']['passed']);self.assertFalse(j['comparison_eligible']);self.assertEqual(j['sanitizer_status'],'leak-only-reported-order-unknown');self.assertNotIn('/private',json.dumps(j));self.assertNotIn('0x123',json.dumps(j))
    def test_fault_not_eligible_even_exit_zero(self):
        r=record();r['stderr']='ERROR: AddressSanitizer: heap-use-after-free';self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_incomplete_sanitizer(self):
        r=record();r['stderr']='ERROR: LeakSanitizer: detected memory leaks';self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_phase_bound(self):
        r=record();r['stdout']+=('# Geometry timing wait=1 probe=frame-paint event=enter elapsed-us=9\n'*33);self.assertFalse(self.run_record(r)['metrics_available'])
    def test_unknown_phase(self):
        r=record();r['stdout']=r['stdout'].replace('probe=frame-paint','probe=secret');j=self.run_record(r);self.assertFalse(j['metrics_available']);self.assertNotIn('secret',json.dumps(j))
    def test_frame_summary_stability_mismatch(self):
        r=record();r['stdout']=r['stdout'].replace('primary_stable=0','primary_stable=1').replace('final_stable=0','final_stable=1');self.assertFalse(self.run_record(r)['metrics_available'])
    def test_reordered_record_blocks(self):
        r=record();line='# MATCHED_FRAMES truncated=0\n';r['stdout']=line+r['stdout'].replace(line,'');self.assertFalse(self.run_record(r)['metrics_available'])
    def test_meson_omitted_empty_stderr(self):
        r=record();del r['stderr'];self.assertTrue(self.run_record(r)['comparison_eligible'])
    def test_generic_warning_is_ambiguous(self):
        r=record();r['stderr']='GLib-GObject-WARNING **: private details';j=self.run_record(r);self.assertFalse(j['comparison_eligible']);self.assertEqual(j['sanitizer_status'],'unknown-diagnostic');self.assertNotIn('private',json.dumps(j))
    def test_wrong_order(self):self.assertFalse(self.run_record(record(),order=2)['comparison_eligible'])
    def test_extra_json_record(self):
        self.path.write_text((json.dumps(record())+'\n')*2);self.assertFalse(m.collect(self.path,'baseline-normal',1,1,'primary')['comparison_eligible'])
    def test_duplicate_json_key(self):
        self.path.write_text('{"stdout":"secret","stdout":"other"}\n');j=m.collect(self.path,'baseline-normal',1,1,'primary');self.assertFalse(j['comparison_eligible']);self.assertNotIn('secret',json.dumps(j))
    def test_fidelity_assert_failure_retains_first_wait(self):
        r=record();r.update(name='gimp:app / painter-compact-options',result='FAIL',returncode=-6)
        r['stdout']='# Geometry wait=1 caller=brush_popup_open:425 elapsed-us=12970767 iterations=3 callbacks=1 stable=0 changes=0 longest-dispatch-us=12966932 initial-mapped/realized/viewable=1/1/1 final-mapped/realized/viewable=1/1/1 same-clock=1\nBail out!\n'
        j=self.run_record(r,cell='current-sanitizer',order=14,kind='fidelity');self.assertTrue(j['metrics_available']);self.assertFalse(j['comparison_eligible']);self.assertFalse(j['metrics']['original_primary_pass'])

if __name__=='__main__':unittest.main()

class OrderedLeaks(unittest.TestCase):
    setUp=Collection.setUp
    run_record=Collection.run_record
    def merged(self):
        r=record();r.update(result='FAIL',returncode=1)
        r['command']=['python3',str(Path(m.__file__).with_name('matched_preclick_exec.py').resolve()),'/build/matched-preclick','-p',m.PRIMARY]
        r['stdout']+='ERROR: LeakSanitizer: detected memory leaks\nSUMMARY: AddressSanitizer: 7 byte(s) leaked in 1 allocation(s)\n'
        return r
    def test_late_leak_retains_failed_process(self):
        j=self.run_record(self.merged());self.assertTrue(j['comparison_eligible'],j);self.assertFalse(j['process']['passed']);self.assertFalse(j['acceptance_eligible']);self.assertEqual(j['status'],'qualified-late-leak-metrics')
    def test_same_line_diagnostic_is_not_hidden(self):
        for extra in [' WARNING: unrelated',' ERROR: unrelated']:
            r=self.merged();r['stdout']=r['stdout'].replace('detected memory leaks','detected memory leaks'+extra)
            self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_exact_executable_pid_header(self):
        r=self.merged();r['stdout']=r['stdout'].replace('ERROR: LeakSanitizer','==matched-preclick==123==ERROR: LeakSanitizer')
        self.assertTrue(self.run_record(r)['comparison_eligible'])
    def test_early_leak_not_qualified(self):
        r=self.merged();marker='ERROR: LeakSanitizer: detected memory leaks\n';r['stdout']=marker+r['stdout'].replace(marker,'');self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_wrong_wrapper_not_orderproof(self):
        r=self.merged();r['command'][1]='/private/other.py';j=self.run_record(r);self.assertFalse(j['comparison_eligible']);self.assertNotIn('/private',json.dumps(j))
    def test_nonempty_split_stderr_not_orderproof(self):
        r=self.merged();r['stderr']='unmerged';self.assertFalse(self.run_record(r)['comparison_eligible'])
    def test_late_leak_plus_other_fault_not_qualified(self):
        r=self.merged();r['stdout']+='runtime error: invalid arithmetic\n';self.assertFalse(self.run_record(r)['comparison_eligible'])

class Wrapper(unittest.TestCase):
    def test_exact_exec_and_unchanged_environment(self):
        from unittest import mock
        import os
        spec=importlib.util.spec_from_file_location('wrapper',Path(m.__file__).with_name('matched_preclick_exec.py'));w=importlib.util.module_from_spec(spec);spec.loader.exec_module(w)
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'matched-preclick';p.write_text('fixture not executed');p.chmod(0o700)
            argv=[str(p),'-p',m.PRIMARY];before=dict(os.environ)
            with mock.patch.object(w.os,'dup2') as dup,mock.patch.object(w.os,'execv',side_effect=RuntimeError('exec-called')) as exe:
                with self.assertRaises(RuntimeError):w.main(argv)
                dup.assert_called_once_with(1,2);exe.assert_called_once_with(str(p),argv)
            self.assertEqual(before,dict(os.environ))
            for bad in [argv+['-x','/other'],[str(p),'-p',m.FIDELITY],[str(p),'--run-prefix',m.PRIMARY]]:
                self.assertEqual(w.main(bad),2)

class Aggregate(unittest.TestCase):
    def test_configuration_pins_match_provenance(self):
        import matched_provenance
        for cell in m.CELLS:
            self.assertEqual(m.expected_configuration_overlay(cell),matched_provenance.configuration_overlay(cell.split('-')[0]))

    def test_complete_four_cells_and_fourteen_rows(self):
        from unittest import mock
        import hashlib
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);reports=root/'reports';reports.mkdir();producer=root/'matched_provenance.py';producer.write_text('frozen producer fixture')
            identity={'schema':'matched-preclick-comparison-identity-v1','acceptance_eligible':False,'comparison_eligible':True,'closure_comparable':True,'producer_sha256':hashlib.sha256(producer.read_bytes()).hexdigest(),'errors':[],'cells':{},'runs':[]}
            for i,c in enumerate(m.CELLS):
                commit=m.BASE if c.startswith('baseline') else m.CURRENT
                identity['cells'][c]={'source_commit':commit,'source_tree':m.TREES[commit],'closure_comparable':True,'configuration_overlay':m.expected_configuration_overlay(c),'identity_sha256':f'{i+1:064x}','runtime_inventory_sha256':[f'{i*10+j+1:064x}' for j in range(3 if c.startswith('baseline') else 4)]}
            collector=root/'collect.py';collector.write_bytes(Path(m.__file__).read_bytes())
            collector_hash=hashlib.sha256(collector.read_bytes()).hexdigest()
            ip=root/'identity.json'
            for order,(cell,rep) in enumerate(m.ORDER+[('current-normal',1),('current-sanitizer',1)],1):
                kind='primary' if order<=12 else 'fidelity'
                summary={'primary_elapsed_us':10,'primary_cpu_us':8,'longest_iteration_us':9} if kind=='primary' else {'elapsed_us':10,'longest_iteration_us':9}
                raw_hash=f'{order+100:064x}'
                inventory=identity['cells'][cell]['runtime_inventory_sha256'][rep-1 if kind=='primary' else 3]
                identity['runs'].append(dict(order=order,cell=cell,repetition=rep,kind=kind,raw_log_sha256=raw_hash,runtime_inventory_sha256=inventory))
                report={'raw_log_sha256':raw_hash,'collector_sha256':collector_hash,'format':1,'acceptance_eligible':False,'kind':kind,'cell':cell,'repetition':rep,'order':order,'metrics_available':True,'comparison_eligible':True,'collection_complete':True,'tap_status':'exact-leaf-pass','sanitizer_status':'no-finding-reported','process':{'meson_result':'OK','returncode':0,'passed':True},'metrics':{'original_primary_pass':True,'summary':summary}}
                (reports/f'{order}.json').write_text(json.dumps(report))
            ip.write_text(json.dumps(identity))
            with mock.patch.object(m,'__file__',str(root/'collect.py')):
                out=m.aggregate(reports,ip);self.assertTrue(out['comparison_eligible']);self.assertEqual(len(out['samples']),14);self.assertFalse(out['acceptance_eligible'])
                self.assertTrue(out['baseline_build_metadata_modified'])
                self.assertFalse(out['pristine_baseline_build_comparison'])
                self.assertIn('not a pristine-baseline',out['interpretation'])
                for cell in m.CELLS:
                    self.assertEqual(out['identity_refs'][cell]['configuration_overlay'],m.expected_configuration_overlay(cell))
                original_descriptor=identity['cells']['baseline-normal']['configuration_overlay']
                for bad in (None,{},m.expected_configuration_overlay('current-normal'),{**original_descriptor,'applied':1},{**original_descriptor,'arbitrary':'private text'}):
                    identity['cells']['baseline-normal']['configuration_overlay']=bad;ip.write_text(json.dumps(identity))
                    with self.assertRaises(m.Invalid):m.aggregate(reports,ip)
                identity['cells']['baseline-normal']['configuration_overlay']=original_descriptor;ip.write_text(json.dumps(identity))
                first=reports/'1.json';original=json.loads(first.read_text())
                for field,value in [('raw_log_sha256',identity['runs'][1]['raw_log_sha256']),('collector_sha256','0'*64)]:
                    bad=dict(original);bad[field]=value;first.write_text(json.dumps(bad))
                    with self.assertRaises(m.Invalid):m.aggregate(reports,ip)
                first.write_text(json.dumps(original))
                saved=identity['runs'];identity['runs']=saved[:-1];ip.write_text(json.dumps(identity))
                with self.assertRaises(m.Invalid):m.aggregate(reports,ip)
                identity['runs']=saved
                identity['closure_comparable']=False;ip.write_text(json.dumps(identity));self.assertFalse(m.aggregate(reports,ip)['comparison_eligible'])
                (reports/'14.json').unlink()
                with self.assertRaises(m.Invalid):m.aggregate(reports,ip)
