#!/usr/bin/env python3
"""Synthetic provenance/negative-control contracts, not native execution proof."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('frame_red',Path(__file__).with_name('frame_style_red.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
CASE=m.CASES[0]

def row():
    error='ERROR:../source/libgimpwidgets/test-frame-style-idempotence.c:69:assert_unchanged_update: assertion failed (count == 0): (3 == 0)'
    return dict(name='gimp:painter-negative-control / frame-style-old-provider',result='EXPECTEDFAIL',returncode=-6,
                command=['/build/libgimpwidgets/frame-style-old-provider','-p',CASE],duration=0.3,
                env={},stdout='TAP version 14\n# Start of gimp-frame-style tests\nnot ok '+CASE+' - '+error+'\nBail out!\n',stderr='**\n'+error+'\n')

def sym(defs=(),undefs=()):
    lines=['Symbol table:']
    for i,name in enumerate(defs):lines.append(f' {i+1}: 00000000 42 FUNC GLOBAL DEFAULT 1 {name}')
    for i,name in enumerate(undefs):lines.append(f' {100+i}: 00000000 0 NOTYPE GLOBAL DEFAULT UND {name}')
    return '\n'.join(lines)+'\n'

class Cases(unittest.TestCase):
    def reject(self,r):self.assertFalse(m.check_case([r],CASE)['control_passed'])
    def test_accept_exact(self):self.assertTrue(m.check_case([row()],CASE)['control_passed'])
    def test_accept_old_glib_bail(self):
        r=row();r['stdout']=r['stdout'].replace('not ok '+CASE+' - ERROR:','Bail out! ERROR:').replace('\nBail out!\n','\n');self.assertTrue(m.check_case([r],CASE)['control_passed'])
    def test_exact_compiled_log_domain(self):
        r=row()
        for key in ('stdout','stderr'):r[key]=r[key].replace('ERROR:','LibGimpWidgets:ERROR:')
        self.assertTrue(m.check_case([r],CASE)['control_passed'])
    def test_domain_does_not_allow_wrong_assertion_or_unrelated_errors(self):
        for old,new in [('count','other'),('(3 == 0)','(2 == 0)'),('assert_unchanged_update','other'),('test-frame-style-idempotence.c','other.c')]:
            r=row()
            for key in ('stdout','stderr'):r[key]=r[key].replace('ERROR:','LibGimpWidgets:ERROR:').replace(old,new)
            self.reject(r)
        r=row()
        for key in ('stdout','stderr'):r[key]=r[key].replace('ERROR:','LibGimpWidgets:ERROR:')
        r['stderr']+='LibGimpWidgets:ERROR: unrelated\n';self.reject(r)
    def test_other_or_repeated_domain_rejected(self):
        for domain in ('Other:', 'libgimpwidgets:', 'LibGimpWidgets:LibGimpWidgets:', 'LibGimpWidgetsExtra:'):
            r=row()
            for key in ('stdout','stderr'):r[key]=r[key].replace('ERROR:',domain+'ERROR:')
            self.reject(r)
    def test_domain_wrong_leaf_rejected(self):
        r=row()
        for key in ('stdout','stderr'):r[key]=r[key].replace('ERROR:','LibGimpWidgets:ERROR:')
        r['stdout']=r['stdout'].replace(CASE,'/gimp-frame-style/wrong');self.reject(r)
    def test_only_one(self):
        for rows in ([],[row(),row()],[None]):self.assertFalse(m.check_case(rows,CASE)['control_passed'])
    def test_wrong_result(self):
        for result in ('OK','FAIL','TIMEOUT','SKIP','UNEXPECTEDPASS',None):r=row();r['result']=result;self.reject(r)
    def test_wrong_signal(self):
        for code in (0,1,77,134,-11,-15,True,'-6'):r=row();r['returncode']=code;self.reject(r)
    def test_wrong_assertion(self):
        for old,new in [('count','other'),('(3 == 0)','(1 == 0)'),('assert_unchanged_update','other'),('test-frame-style-idempotence.c','other.c')]:
            r=row();r['stdout']=r['stdout'].replace(old,new);r['stderr']=r['stderr'].replace(old,new);self.reject(r)
    def test_wrong_leaf(self):
        r=row();r['command'][-1]='/gimp-frame-style/different-weight';self.reject(r)
        r=row();r['stdout']=r['stdout'].replace(CASE,'/gimp-frame-style/other');self.reject(r)
    def test_wrong_registration(self):r=row();r['name']='gimp-frame-style-idempotence';self.reject(r)
    def test_wrong_binary(self):r=row();r['command'][0]='/build/test-frame-style-idempotence';self.reject(r)
    def test_extra_selection(self):
        for more in (['-s',CASE],['-p',CASE],['--verbose'],['--list']):r=row();r['command']+=more;self.reject(r)
    def test_missing_execution(self):
        for token in ('TAP version 14','# Start of gimp-frame-style tests','Bail out!'):
            r=row();r['stdout']=r['stdout'].replace(token,'');self.reject(r)
        for key in ('stdout','stderr','command'):
            r=row();r[key]=None;self.reject(r)
    def test_runtime_errors(self):
        for extra in ('GLib-GObject-WARNING cannot register existing type GimpFrame','CRITICAL assertion failure','AddressSanitizer: crash','LeakSanitizer: leak','runtime error: overflow','ERROR: unrelated','Traceback','timeout','ok 1 '+CASE,'not ok /other - ERROR: unrelated','1..0','# SKIP no display'):
            r=row();r['stderr']+='\n'+extra+'\n';self.reject(r)
    def test_injected_env(self):
        for key in ('LD_PRELOAD','LD_AUDIT'):r=row();r['env']={key:'/bad.so'};self.reject(r)
    def test_positive_unrelated_debug(self):
        r=row();r['stdout']='# GLib-GIO-DEBUG: local VFS selected\n'+r['stdout'];self.assertTrue(m.check_case([r],CASE)['control_passed'])
    def test_similar_text_not_assertion(self):
        r=row();r['stderr']='count == 0 (3 == 0)';self.reject(r)

class Fixture:
    def __init__(self,root):
        self.root=root;self.source=root/'source';self.build=root/'build';self.paths={}
        def put(path,text='synthetic artifact'):
            path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text);return path
        self.fixture=put(self.source/m.FIXTURE,'old frame');self.test=put(self.source/m.TEST,'same test')
        self.pins={}
        for name in m.PINNED:self.pins[name]=m.sha(put(self.source/name,'pinned '+name))
        self.production=put(self.source/'libgimpwidgets/gimpframe.c','fixed frame')
        self.config=put(self.build/'config.h')
        self.objects={
            'old':put(self.build/'libgimpwidgets/frame-style-old-provider.p/fixtures_gimpframe-c14.c.o'),
            'old_test':put(self.build/'libgimpwidgets/frame-style-old-provider.p/test-frame-style-idempotence.c.o'),
            'candidate_test':put(self.build/'libgimpwidgets/test-frame-style-idempotence.p/test-frame-style-idempotence.c.o'),
            'utils':put(self.build/'libgimpwidgets/libgimpwidgets-3.0.so.0.0.0.p/gimpwidgetsutils.c.o'),
            'production':put(self.build/'libgimpwidgets/libgimpwidgets-3.0.so.0.0.0.p/gimpframe.c.o')}
        self.binary=put(self.build/'libgimpwidgets/frame-style-old-provider')
        self.candidate=put(self.build/'libgimpwidgets/test-frame-style-idempotence')
        self.widgets=put(self.build/'libgimpwidgets/libgimpwidgets-3.0.so.0.0.0')
        self.base=put(self.build/'libgimpbase/libgimpbase-3.0.so.0.0.0')
        self.tool=put(root/'tools/fake-tool')
        self.cc=[]
        for key,src in [('old',self.fixture),('old_test',self.test),('candidate_test',self.test),('utils',self.source/'libgimpwidgets/gimpwidgetsutils.c'),('production',self.production)]:
            args=['cc','-c',str(src),'-o',str(self.objects[key])]
            if key in ('old','old_test'):args+=sorted(m.MACROS)
            self.cc.append(dict(directory=str(self.build),file=str(src),arguments=args))
        self.options=[dict(name='b_sanitize',value='none')]
        self.map=self.build/'libgimpwidgets/frame-style-old-provider.map'
        self.helpermap=self.build/'libgimpwidgets/frame-helper-provider.map'
        self.candidatemap=self.build/'libgimpwidgets/frame-candidate-provider.map'
        self.candidatetext='LOAD '+str(self.objects['candidate_test'])+'\nOUTPUT('+str(self.candidate)+' elf64-x86-64)\nCross Reference Table\nmain '+str(self.objects['candidate_test'])+'\n'
        self.maptext='LOAD '+str(self.objects['old'])+'\nLOAD '+str(self.objects['old_test'])+'\nOUTPUT('+str(self.binary)+' elf64-x86-64)\nCross Reference Table\n'+''.join(x+' '+str(self.objects['old'])+'\n' for x in m.APIS)+m.HELPER+' '+str(self.widgets)+'\n'
        self.helpertext='LOAD '+str(self.objects['utils'])+'\nOUTPUT('+str(self.widgets)+' elf64-x86-64)\nCross Reference Table\n'+m.HELPER+' '+str(self.objects['utils'])+'\n'
        self.helpertext=self.helpertext.replace('OUTPUT(', 'LOAD '+str(self.objects['production'])+'\nOUTPUT(')+''.join(api+' '+str(self.objects['production'])+'\n' for api in m.CANONICAL)
        self.candidatetext+='gimp_frame_new '+str(self.widgets)+'\n'
        self.syms={'old':sym(m.APIS,(m.HELPER,)), 'old_test':sym((),('gimp_frame_c14_new',)),
                   'candidate_test':sym(('main',),('gimp_frame_new',)), 'utils':sym((m.HELPER,)), 'production':sym(m.CANONICAL), 'candidate':sym(('main',),('gimp_frame_new',)),
                   'binary':sym(m.APIS,(m.HELPER,)), 'widgets':sym((m.HELPER,'gimp_frame_new','gimp_frame_get_type'))}
        self.deptexts={key:str(obj.relative_to(self.build))+': #deps 5, deps mtime 1 (VALID)\n'+''.join('    '+str(x)+'\n' for x in [self.config,*[self.source/p for p in self.pins if p.endswith('.h')]]) for key,obj in self.objects.items()}
        self.loader='[Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]\n'
        self.listing=f' libgimpwidgets-3.0.so.0 => {self.widgets} (0x1234)\n libgimpbase-3.0.so.0 => {self.base} (0x5678)\n'
        self.dynamic='Dynamic section';self.extra=[]
        self.metadata=[dict(name=m.TARGET,cmd=[str(self.binary)],env={}),dict(name='gimp-frame-style-idempotence',cmd=[str(self.candidate)],env={})]
    def write(self):
        (self.build/'compile_commands.json').write_text(json.dumps(self.cc));(self.build/'meson-info').mkdir(exist_ok=True)
        (self.build/'meson-info/intro-buildoptions.json').write_text(json.dumps(self.options));self.map.write_text(self.maptext);self.helpermap.write_text(self.helpertext);self.candidatemap.write_text(self.candidatetext)
        (self.build/'meson-info/intro-tests.json').write_text(json.dumps(self.metadata))
    def inspect(self,args,loader_env=None,cwd=None):
        if args[0]=='ninja':
            obj=self.build/args[-1];key=next(k for k,v in self.objects.items() if v==obj);return self.deptexts[key]
        if args[0]=='readelf':
            if args[1]=='-lW':return self.loader
            if args[1]=='-dW':return self.dynamic
            path=Path(args[-1]);key='binary' if path==self.binary else 'candidate' if path==self.candidate else 'widgets' if path==self.widgets else next(k for k,v in self.objects.items() if v==path);return self.syms[key]
        if args[1]=='--list':
            if cwd!=str(self.build):raise AssertionError('Loader cwd differs from Meson build working directory')
            return self.listing
        raise AssertionError('Unexpected native invocation '+str(args))

class Provider(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.f=Fixture(Path(self.tmp.name));self.addCleanup(patch.stopall)
        patch.object(m,'PRODUCTION_SHA',m.sha(self.f.production)).start();patch.object(m,'OLD_SHA',m.sha(self.f.fixture)).start();patch.object(m,'TEST_SHA',m.sha(self.f.test)).start();patch.object(m,'PINNED',self.f.pins).start()
        patch.object(m,'run',side_effect=self.f.inspect).start();patch.object(m.shutil,'which',return_value=str(self.f.tool)).start()
        patch.dict(m.os.environ,{'LD_PRELOAD':'','LD_AUDIT':''}).start()
    def check(self):self.f.write();return m.check_provider(self.f.source,self.f.build,self.f.map)
    def reject(self):
        with self.assertRaises((ValueError,OSError)):self.check()
    def test_valid(self):self.assertTrue(self.check()['control_passed'])
    def test_map_size_bound(self):
        with patch.object(m,'MAX_MAP_BYTES',1):self.reject()
    def test_none_lists(self):
        for value in ([],['none']):self.f.options[0]['value']=value;self.assertTrue(self.check()['control_passed'])
    def test_changed_fixture(self):self.f.fixture.write_text('changed');self.reject()
    def test_changed_test(self):self.f.test.write_text('changed');self.reject()
    def test_changed_utils(self):(self.f.source/'libgimpwidgets/gimpwidgetsutils.c').write_text('changed');self.reject()
    def test_changed_frame_header(self):(self.f.source/'libgimpwidgets/gimpframe.h').write_text('changed');self.reject()
    def test_wrong_checkout(self):
        self.f.cc[0]['file']=str(self.f.root/'other'/m.FIXTURE);self.reject()
    def test_missing_namespace_one_tu(self):self.f.cc[1]['arguments'].pop();self.reject()
    def test_extra_namespace(self):self.f.cc[0]['arguments'].append('-Dgimp_frame_new=evil');self.reject()
    def test_namespace_leaks_candidate(self):self.f.cc[2]['arguments']+=list(m.MACROS);self.reject()
    def test_override_namespace(self):self.f.cc[0]['arguments'].append('-Ugimp_frame_new');self.reject()
    def test_sanitized(self):self.f.options[0]['value']=['address'];self.reject()
    def test_sanitized_compile(self):self.f.cc[0]['arguments'].append('-fsanitize=address');self.reject()
    def test_joined_forced_include(self):
        for arg in ('-include/evil.h','-imacros/evil.h'):
            self.f.cc[0]['arguments'].append(arg);self.reject();self.f.cc[0]['arguments'].pop()
    def test_wrong_production(self):self.f.production.write_text('changed');self.reject()
    def test_candidate_unique_namespace(self):self.f.syms['candidate_test']=sym((),('gimp_frame_c14_new',));self.reject()
    def test_candidate_wrong_loader_provider(self):self.f.syms['candidate']=sym(('gimp_frame_new',));self.reject()
    def test_wrong_workdir(self):self.f.metadata[0]['workdir']=str(self.f.root);self.reject()
    def test_bad_metadata(self):self.f.metadata[0]['cmd']=['other'];self.reject()
    def test_forced_include(self):self.f.cc[0]['arguments']+=['-include','evil.h'];self.reject()
    def test_response_file(self):self.f.cc[0]['arguments'].append('@evil');self.reject()
    def test_wrong_object(self):self.f.cc[0]['arguments'][4]=str(self.f.root/'outside.o');self.reject()
    def test_duplicate_compile(self):self.f.cc.append(self.f.cc[0]);self.reject()
    def test_stale_deps(self):self.f.deptexts['old']=self.f.deptexts['old'].replace('(VALID)','(STALE)');self.reject()
    def test_missing_header_dep(self):self.f.deptexts['old']=self.f.deptexts['old'].replace('    '+str(self.f.source/'libgimpwidgets/gimpframe.h')+'\n','');self.reject()
    def test_wrong_object_deps(self):self.f.deptexts['old']=self.f.deptexts['old'].replace(str(self.f.objects['old'].relative_to(self.f.build)),'wrong.o');self.reject()
    def test_canonical_old_import(self):self.f.syms['old']+=sym((),('gimp_frame_new',));self.reject()
    def test_canonical_test_import(self):self.f.syms['old_test']=sym((),('gimp_frame_new',));self.reject()
    def test_canonical_binary_import(self):self.f.syms['binary']+=sym((),('gimp_frame_get_type',));self.reject()
    def test_missing_unique_definition(self):self.f.syms['old']=sym(('gimp_frame_c14_new',),(m.HELPER,));self.reject()
    def test_missing_test_reference(self):self.f.syms['old_test']=sym((),('other',));self.reject()
    def test_test_provides_new(self):self.f.syms['old_test']=sym(('gimp_frame_c14_new',));self.reject()
    def test_optimized_internal_get_type_ok(self):self.assertTrue(self.check()['control_passed'])
    def test_helper_interposition(self):self.f.syms['binary']+=sym((m.HELPER,));self.reject()
    def test_old_not_using_helper(self):self.f.syms['old']=sym(m.APIS);self.reject()
    def test_wrong_map_output(self):self.f.maptext=self.f.maptext.replace('OUTPUT('+str(self.f.binary),'OUTPUT(/tmp/wrong');self.reject()
    def test_missing_old_load(self):self.f.maptext=self.f.maptext.replace('LOAD '+str(self.f.objects['old'])+'\n','');self.reject()
    def test_missing_test_load(self):self.f.maptext=self.f.maptext.replace('LOAD '+str(self.f.objects['old_test'])+'\n','');self.reject()
    def test_wrong_map_def(self):self.f.maptext=self.f.maptext.replace('gimp_frame_c14_new '+str(self.f.objects['old']),'gimp_frame_c14_new other.o');self.reject()
    def test_canonical_archive_extracted(self):self.f.maptext+='lib.a(gimpframe.c.o)\n';self.reject()
    def test_wrong_helper_map_def(self):self.f.helpertext=self.f.helpertext.replace(m.HELPER+' '+str(self.f.objects['utils']),m.HELPER+' other.o');self.reject()
    def test_wrong_dso_output(self):self.f.helpertext=self.f.helpertext.replace('OUTPUT('+str(self.f.widgets),'OUTPUT(/tmp/wrong');self.reject()
    def test_wrong_helper_dso_binding(self):self.f.maptext=self.f.maptext.replace(m.HELPER+' '+str(self.f.widgets),m.HELPER+' /tmp/wrong.so');self.reject()
    def test_loaded_alternate_widgets(self):self.f.listing=self.f.listing.replace(str(self.f.widgets),'/tmp/alternate.so');self.reject()
    def test_loaded_alternate_base(self):self.f.listing=self.f.listing.replace(str(self.f.base),'/tmp/alternate.so');self.reject()
    def test_duplicate_widgets(self):self.f.listing+=self.f.listing.splitlines()[0]+'\n';self.reject()
    def test_unresolved_library(self):self.f.listing+=' libother.so => not found\n';self.reject()
    def test_missing_helper_export(self):self.f.syms['widgets']=sym(('gimp_frame_new',));self.reject()
    def test_unique_dso_export(self):self.f.syms['widgets']+=sym(m.APIS);self.reject()
    def test_injected_loader_env(self):
        for key in ('LD_PRELOAD','LD_AUDIT'):
            with patch.dict(m.os.environ,{key:'/bad.so'}):self.reject()
    def test_elf_audit(self):self.f.dynamic+=' (AUDIT) bad.so';self.reject()
    def test_wrong_loader(self):self.f.loader='[Requesting program interpreter: /tmp/evil]\n';self.reject()

class Aggregate(Provider):
    # Reuse the full synthetic provenance factory; inherited provider tests also
    # validate the evidence path used by aggregate. No binaries are executed.
    def setUp(self):
        super().setUp();proof=self.check();self.proof=self.f.root/'provider.json';self.proof.write_text(json.dumps(proof))
        r=row();r['command'][0]=str(self.f.binary);self.log=self.f.root/'raw.jsonl';self.log.write_text(json.dumps(r)+'\n')
        receipt=m.check_case([r],CASE);receipt.update(log_path=str(self.log),log_sha256=m.sha(self.log));self.receipt=self.f.root/'receipt.json';self.receipt.write_text(json.dumps(receipt))
        candidate=dict(name='gimp:libgimpwidgets / gimp-frame-style-idempotence',result='OK',returncode=0,command=[str(self.f.candidate)],env={},stderr='',stdout='TAP version 14\n1..7\n'+''.join('ok '+str(i+1)+' /gimp-frame-style/'+leaf+'\n' for i,leaf in enumerate(m.POSITIVE_CASES)))
        self.candidate_log=self.f.root/'candidate-raw.jsonl';self.candidate_log.write_text(json.dumps(candidate))
    def test_empty_provider_schema(self):
        proof=json.loads(self.proof.read_text());proof['inputs']={};proof['loaded_libraries']={};self.proof.write_text(json.dumps(proof))
        with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
    def test_missing_raw_env(self):
        row=json.loads(self.log.read_text());row.pop('env');self.log.write_text(json.dumps(row));receipt=json.loads(self.receipt.read_text());receipt['log_sha256']=m.sha(self.log);self.receipt.write_text(json.dumps(receipt))
        with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
    def test_unknown_loader_env(self):
        row=json.loads(self.candidate_log.read_text());row['env']['LD_ORIGIN_PATH']='/bad';self.candidate_log.write_text(json.dumps(row))
        with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
    def test_candidate_zero_cases(self):
        row=json.loads(self.candidate_log.read_text());row['stdout']='1..0';self.candidate_log.write_text(json.dumps(row))
        with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
    def test_candidate_skip(self):
        row=json.loads(self.candidate_log.read_text());row['stdout']+=' # SKIP no display';self.candidate_log.write_text(json.dumps(row))
        with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
    def test_no_replay_after_header_change(self):
        (self.f.source/'libgimpwidgets/gimpframe.h').write_text('changed')
        with patch.object(m,'run') as replay:
            with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
            replay.assert_not_called()
    def test_no_replay_after_loader_hash_change(self):
        proof=json.loads(self.proof.read_text());proof['inputs']['external/'+proof['loader_path'].lstrip('/')]='0'*64;self.proof.write_text(json.dumps(proof))
        with patch.object(m,'run') as replay:
            with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
            replay.assert_not_called()
    def test_no_replay_untrusted_loader_path(self):
        proof=json.loads(self.proof.read_text());proof['loader_path']='/tmp/evil';self.proof.write_text(json.dumps(proof))
        with patch.object(m,'run') as replay:
            with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
            replay.assert_not_called()
    def test_replay_preserves_empty_component_and_cwd(self):
        candidate=json.loads(self.candidate_log.read_text());candidate['env']['LD_LIBRARY_PATH']='relative:';self.candidate_log.write_text(json.dumps(candidate))
        with patch.object(m,'run',side_effect=self.f.inspect) as replay:
            self.assertTrue(m.aggregate([self.receipt],self.proof,self.candidate_log)['control_passed'])
            self.assertEqual(replay.call_args_list[0].kwargs['cwd'],str(self.f.build))
            self.assertEqual(replay.call_args_list[0].kwargs['loader_env']['LD_LIBRARY_PATH'],'relative:')
    def test_aggregate_valid(self):self.assertTrue(m.aggregate([self.receipt],self.proof,self.candidate_log)['control_passed'])
    def test_aggregate_empty(self):
        with self.assertRaises(ValueError):m.aggregate([],self.proof,self.candidate_log)
    def test_aggregate_duplicate(self):
        with self.assertRaises(ValueError):m.aggregate([self.receipt,self.receipt],self.proof,self.candidate_log)
    def test_aggregate_binary_changed(self):
        self.f.binary.write_text('changed')
        with patch.object(m,'run') as replay:
            with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
            replay.assert_not_called()
    def test_aggregate_dso_changed(self):
        self.f.widgets.write_text('changed')
        with self.assertRaises(ValueError):m.aggregate([self.receipt],self.proof,self.candidate_log)
    def test_aggregate_wrong_execution(self):
        r=json.loads(self.log.read_text());r['command'][0]='/tmp/frame-style-old-provider';self.log.write_text(json.dumps(r));receipt=json.loads(self.receipt.read_text());receipt['log_sha256']=m.sha(self.log);self.receipt.write_text(json.dumps(receipt));self.assertRaises(ValueError,m.aggregate,[self.receipt],self.proof,self.candidate_log)
    def test_aggregate_changed_raw(self):self.log.write_text(json.dumps(row()));self.assertRaises(ValueError,m.aggregate,[self.receipt],self.proof,self.candidate_log)

if __name__=='__main__':unittest.main()
