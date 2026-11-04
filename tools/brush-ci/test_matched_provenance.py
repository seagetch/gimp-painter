import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import matched_provenance as m

class CoreTests(unittest.TestCase):
 def test_normalization(self):
  self.assertEqual(m.normalize('/tmp/cell/source/a -I/tmp/cell/build',{'source':'/tmp/cell/source','build':'/tmp/cell/build'}),'@source@/a -I@build@')
 def test_foreign_cell(self):
  with self.assertRaisesRegex(m.EvidenceError,'foreign-private'):m.normalize('-L/tmp/other/build',{'build':'/tmp/own/build'})
 def test_loader_rejects_preload(self):
  with self.assertRaises(m.EvidenceError):m.no_loader_injection({'LD_PRELOAD':'x'})
 def test_loader_rejects_audit(self):
  with self.assertRaises(m.EvidenceError):m.no_loader_injection({'LD_AUDIT':'x'})
 def test_loader_rejects_debug(self):
  with self.assertRaises(m.EvidenceError):m.no_loader_injection({'LD_DEBUG':'all'})
 def test_lsan_even_empty(self):
  with self.assertRaises(m.EvidenceError):m.no_loader_injection({'LSAN_OPTIONS':''})
 def test_actual_env_required(self):
  with self.assertRaises(m.EvidenceError):m.actual_environment({})
 def test_actual_env_not_list(self):
  with self.assertRaises(m.EvidenceError):m.actual_environment({'env':[]})
 def test_actual_env_bad_value(self):
  with self.assertRaises(m.EvidenceError):m.actual_environment({'env':{'X':1}})
 def test_actual_env_empty_record_valid(self):self.assertEqual(m.actual_environment({'env':{}}),{})
 def test_link_policy_retains_target(self):
  self.assertEqual(m.link_policy('cc -shared -Wl,--as-needed -o liba.so a.o'),{'liba.so':['-shared','-Wl,--as-needed']})
 def test_link_policy_different_graph(self):
  self.assertEqual(m.link_policy('cc -shared -o a.so a.o'),m.link_policy('cc -shared -o a.so a.o b.o'))
 def test_link_policy_different_flag(self):self.assertNotEqual(m.link_policy('cc -shared -o a.so a.o'),m.link_policy('cc -shared -O3 -o a.so a.o'))
 def test_no_link_commands(self):
  with self.assertRaises(m.EvidenceError):m.link_policy('cc -c a.c -o a.o')
 def test_duplicate_link_outputs(self):
  with self.assertRaises(m.EvidenceError):m.link_policy('cc -shared -o a.so a.o\ncc -shared -o a.so b.o')

class FilesTests(unittest.TestCase):
 def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def file(self,name,data='data'):
  p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data);return p
 def test_duplicate_json(self):
  p=self.file('x','{"a":1,"a":2}')
  with self.assertRaises(m.EvidenceError):m.read(p)
 def test_symlink_input(self):
  p=self.file('x');q=self.root/'q';q.symlink_to(p)
  with self.assertRaises(m.EvidenceError):m.regular(q)
 def test_empty_inventory(self):
  empty=self.root/'empty';empty.mkdir()
  with self.assertRaises(m.EvidenceError):m.tree_hash(empty)
 def test_tree_hash_changes(self):
  p=self.file('x');before=m.tree_hash(self.root);p.write_text('new');self.assertNotEqual(before,m.tree_hash(self.root))
 def test_preflight_valid(self):
  self.file('build.ninja');r=m.runtime_preflight(self.root,'baseline-normal',1);self.assertTrue(r['plugin_cache_absent']);self.assertNotIn(str(self.root),json.dumps(r))
 def test_preflight_stale(self):
  self.file('build.ninja');self.file('app/tests/gimpdir-output/pluginrc')
  with self.assertRaises(m.EvidenceError):m.runtime_preflight(self.root,'baseline-normal',1)
 def test_preflight_no_build(self):
  with self.assertRaises(m.EvidenceError):m.runtime_preflight(self.root,'baseline-normal',1)
 def test_preflight_wrong_cell(self):
  self.file('build.ninja')
  with self.assertRaises(m.EvidenceError):m.runtime_preflight(self.root,'other',1)
 def identity(self):
  p=self.file('app/tests/matched-preclick');q=self.file('lib/libgimp.so')
  return {'schema':'matched-preclick-build-identity-v1','verified':True,'producer_sha256':m.sha(m.__file__),'cell':'baseline-normal','build_root_identity':m.digest(str(self.root).encode()),'binaries':{'app/tests/matched-preclick':m.sha(p)},'project_libraries':{'lib/libgimp.so':m.sha(q)}}
 def test_runtime_identity_valid(self):m.verify_runtime_identity(self.identity(),self.root,'baseline-normal')
 def test_runtime_identity_empty(self):
  x=self.identity();x['project_libraries']={}
  with self.assertRaises(m.EvidenceError):m.verify_runtime_identity(x,self.root,'baseline-normal')
 def test_runtime_identity_tampered(self):
  x=self.identity();self.file('lib/libgimp.so','tamper')
  with self.assertRaises(m.EvidenceError):m.verify_runtime_identity(x,self.root,'baseline-normal')
 def test_runtime_identity_other_build(self):
  x=self.identity();x['build_root_identity']='0'*64
  with self.assertRaises(m.EvidenceError):m.verify_runtime_identity(x,self.root,'baseline-normal')
 def test_runtime_identity_path_escape(self):
  x=self.identity();x['binaries']['../outside']='0'*64
  with self.assertRaises(m.EvidenceError):m.verify_runtime_identity(x,self.root,'baseline-normal')
 def test_runtime_identity_wrong_producer(self):
  x=self.identity();x['producer_sha256']='0'*64
  with self.assertRaises(m.EvidenceError):m.verify_runtime_identity(x,self.root,'baseline-normal')

class CompareTests(FilesTests):
 def setUp(self):
  super().setUp(); self.ids=[]; self.runs=[]
  for n,cell in enumerate(m.CELLS):
   arm=cell.split('-')[0];commit,tree=m.COMMITS[arm]
   x={'schema':'matched-preclick-build-identity-v1','verified':True,'errors':[], 'cell':cell,'source_commit':commit,'source_tree':tree,'source_root_identity':str(n),'build_root_identity':str(n),
    'producer_sha256':m.sha(m.__file__),'resources_helper_sha256':m.sha(Path(m.__file__).with_name('matched_resources.py')),'project_libraries':{'lib/libgimp.so':'library'},'options':{'selected':'fixed'},'dependency_archives':{'a':'x'},'dependency_root_sha256':'x','dependency_external_files':{},'build_environment_sha256':'env','compilers':{'cc':'x'},'inspection_tools':{'ninja':'x'},'common_inputs':m.COMMON,'overlay':m.OVERLAY,'configuration_overlay':m.configuration_overlay(cell.split('-')[0]),
    'compiled':{'a.o':{'source':'source/a.c','source_sha256':arm,'recipe_sha256':'r','object_sha256':cell}},'link_policy':{'a.so':['-shared']},'binaries':{'app/tests/matched-preclick':'b','app/tests/painter-compact-options':'f'}}
   x['generator_role']={'link_recipe_sha256':'linkhash','policy':m.GENERATOR,'target_machine':'build','installed':False,'generated_resources':{p:'hash' for p in m.GENERATOR_RESOURCES}}
   x['compiled'][m.GENERATOR['object']]={'source':'source/'+m.GENERATOR['source'],'source_sha256':m.GENERATOR['source_sha256'],'recipe_sha256':m.GENERATOR['recipe_sha256']}
   p=self.file(cell+'.json',json.dumps(x));self.ids.append((p,x))
  lookup={x['cell']:(p,x) for p,x in self.ids}
  for i,cell in enumerate(m.ORDER,1):
   p,x=lookup[cell];y={'schema':'matched-preclick-runtime-inputs-v1','verified':True,'errors':[],'cell':cell,'order':i,'configuration_overlay':x['configuration_overlay'],'identity_sha256':m.sha(p),'wrapper_sha256':m.WRAPPER_SHA,'raw_log_sha256':m.digest(str(i).encode()),'profile_root_identity':m.digest(('profile'+str(i)).encode()),'runtime_settings':{'same':'settings'},'producer_sha256':m.sha(m.__file__),'resources_helper_sha256':m.sha(Path(m.__file__).with_name('matched_resources.py')),'loader':{'binary_sha256':'b' if i<=12 else 'f','providers':{'build/lib/libgimp.so':'library','system/usr/lib/libc.so.6':'libc'},'resolution_replay_not_live_modules':True},'resources':{'comparison_eligible':True,'comparison_inventory':{'catalogs':{'a':'sha'}}}}
   q=self.file('runtime'+str(i)+'.json',json.dumps(y));self.runs.append((q,y))
 def assert_bad(self,code):
  r=m.compare(self.ids,self.runs);self.assertFalse(r['comparison_eligible']);self.assertIn(code,r['errors']);return r
 def test_compare_missing_configuration(self):
  del self.ids[0][1]['configuration_overlay'];self.assert_bad('configuration-identity-mismatch')
 def test_compare_wrong_arm_configuration(self):
  self.ids[0][1]['configuration_overlay']=m.configuration_overlay('current');self.assert_bad('configuration-identity-mismatch')
 def test_runtime_configuration_mismatch(self):
  self.runs[0][1]['configuration_overlay']=m.configuration_overlay('current');self.assert_bad('runtime-configuration-mismatch')
 def test_compare_generator_resource_mismatch(self):
  self.ids[2][1]['generator_role']['generated_resources'][m.GENERATOR_RESOURCES[0]]='different'
  self.assert_bad('generator-resource-mismatch')
 def test_compare_generator_link_mismatch(self):
  self.ids[2][1]['generator_role']['link_recipe_sha256']='different'
  self.assert_bad('generator-link-recipe-mismatch')
 def test_compare_generator_recipe_mismatch(self):
  self.ids[2][1]['compiled'][m.GENERATOR['object']]['recipe_sha256']='different'
  self.assert_bad('generator-object-recipe-mismatch')
 def test_compare_valid(self):self.assertTrue(m.compare(self.ids,self.runs)['comparison_eligible'])
 def test_compare_no_paths(self):self.assertNotIn(str(self.root),json.dumps(m.compare(self.ids,self.runs)))
 def test_compare_missing_cell(self):self.ids.pop();self.assert_bad('identity-cell-set')
 def test_compare_reused_build(self):self.ids[1][1]['build_root_identity']=self.ids[0][1]['build_root_identity'];self.assert_bad('reused-cell-directory')
 def test_compare_reused_source(self):self.ids[1][1]['source_root_identity']=self.ids[0][1]['source_root_identity'];self.assert_bad('reused-cell-directory')
 def test_compare_missing_run(self):self.runs.pop();self.assert_bad('runtime-order-set')
 def test_compare_duplicate_order(self):self.runs[1][1]['order']=1;self.assert_bad('runtime-order-set')
 def test_compare_unproven_resource(self):self.runs[0][1]['resources']['comparison_eligible']=False;self.assert_bad('unproven-resource-inventory')
 def test_compare_resource_mismatch(self):self.runs[0][1]['resources']['comparison_inventory']['catalogs']['a']='other';self.assert_bad('runtime-resource-mismatch')
 def test_compare_wrong_wrapper(self):self.runs[0][1]['wrapper_sha256']='wrong';self.assert_bad('runtime-identity-binding-mismatch')
 def test_compare_wrong_binary(self):self.runs[0][1]['loader']['binary_sha256']='wrong';self.assert_bad('runtime-binary-hash-mismatch')
 def test_compare_wrong_receipt(self):self.runs[0][1]['identity_sha256']='wrong';self.assert_bad('runtime-identity-binding-mismatch')
 def test_compare_wrong_source(self):self.ids[0][1]['source_commit']='wrong';self.assert_bad('identity-source-mismatch')
 def test_compare_wrong_schema(self):self.ids[0][1]['schema']='wrong';self.assert_bad('unverified-build-identity')
 def test_compare_external_public_file_hash_mismatch(self):self.ids[0][1]['dependency_external_files']={'usr/lib/ssl/cert.pem':{'host_file_sha256':'changed'}};self.assert_bad('common-build-input-mismatch')
 def test_compare_dependency_mismatch(self):self.ids[0][1]['dependency_root_sha256']='wrong';self.assert_bad('common-build-input-mismatch')
 def test_compare_options_mismatch(self):self.ids[0][1]['options']={'optimization':'3'};self.assert_bad('common-build-input-mismatch')
 def test_compare_compiler_mismatch(self):self.ids[0][1]['compilers']={'cc':'other'};self.assert_bad('common-build-input-mismatch')
 def test_compare_recipe_mismatch(self):self.ids[0][1]['compiled']['a.o']['recipe_sha256']='other';self.assert_bad('common-object-recipe-mismatch')
 def test_compare_changed_source_graph_allowed(self):
  self.ids[2][1]['compiled']['new.o']={'source':'source/new.c','recipe_sha256':'new'};self.assertTrue(m.compare(self.ids,self.runs)['comparison_eligible'])
 def test_compare_link_policy_mismatch(self):self.ids[0][1]['link_policy']['a.so']=['-O3'];self.assert_bad('link-policy-mismatch')
 def test_compare_external_provider_mismatch(self):self.runs[0][1]['loader']['providers']['system/usr/lib/libc.so.6']='other';self.assert_bad('external-runtime-provider-mismatch')
 def test_compare_external_provider_missing(self):del self.runs[0][1]['loader']['providers']['system/usr/lib/libc.so.6'];self.assert_bad('external-runtime-provider-mismatch')
 def test_compare_project_provider_changed(self):self.runs[0][1]['loader']['providers']['build/lib/libgimp.so']='other';self.assert_bad('runtime-library-hash-mismatch')
 def test_compare_empty_archives(self):self.ids[0][1]['dependency_archives']={};self.assert_bad('incomplete-build-evidence')
 def test_compare_empty_compiled(self):self.ids[0][1]['compiled']={};self.assert_bad('incomplete-build-evidence')
 def test_compare_missing_rawlog(self):del self.runs[0][1]['raw_log_sha256'];self.assert_bad('malformed-identity')
 def test_compare_producer_changed(self):self.runs[0][1]['producer_sha256']='other';self.assert_bad('producer-mismatch')
 def test_compare_module_changed(self):self.ids[0][1]['resources_helper_sha256']='other';self.assert_bad('producer-mismatch')
 def test_compare_per_order_bindings(self):
  r=m.compare(self.ids,self.runs);self.assertEqual([x['order'] for x in r['runs']],list(range(1,15)));self.assertEqual(r['runs'][12]['kind'],'fidelity');self.assertEqual(r['runs'][8]['repetition'],3)
 def test_compare_native_failure_not_acceptance(self):self.assertFalse(m.compare(self.ids,self.runs)['acceptance_eligible'])

class SourceTests(FilesTests):
 def setUp(self):
  super().setUp(); self.common={'tools/common.py':'common'}; self.overlay={'test-matched-preclick.c':'observer','matched-preclick-phases.inc':'phase','meson.append':'append'}
  self.file('tools/common.py','common');self.file('app/tests/meson.build','originalappend');self.file('meson.build','config-after')
  for name,data in self.overlay.items():
   if name!='meson.append':self.file('app/tests/'+name,data)
  self.patches=[patch.object(m,'COMMON',{k:m.digest(v.encode()) for k,v in self.common.items()}),patch.object(m,'OVERLAY',{k:m.digest(v.encode()) for k,v in self.overlay.items()}),patch.object(m,'MESON_SHA',m.digest(b'original')),patch.object(m,'CONFIGURATION_POLICY',{**m.CONFIGURATION_POLICY,'baseline_before':m.digest(b'config-before'),'baseline_after':m.digest(b'config-after'),'current_unchanged':m.digest(b'config-current')})]
  for p in self.patches:p.start();self.addCleanup(p.stop)
  self.responses={('rev-parse','HEAD'):m.COMMITS['baseline'][0],('rev-parse','HEAD^{tree}'):m.COMMITS['baseline'][1],('diff','--cached','--name-only'):'',('diff','--name-only'):'app/tests/meson.build\nmeson.build\n',('show','HEAD:meson.build'):'config-before',('ls-files','--others','--exclude-standard'):'app/tests/test-matched-preclick.c\napp/tests/matched-preclick-phases.inc\n',('ls-files','--others','--ignored','--exclude-standard'):'',('show','HEAD:app/tests/meson.build'):'original',('ls-tree','HEAD','gimp-data'):'160000 commit '+m.SUBMODULE+'\tgimp-data'}
 def git(self,src,*args):
  if Path(src).name=='gimp-data':return m.SUBMODULE if args==('rev-parse','HEAD') else ''
  return self.responses[args]
 def invoke(self):
  with patch.object(m,'git',side_effect=self.git):return m.source_identity(self.root)
 def test_exact_overlay(self):self.assertEqual(self.invoke()['arm'],'baseline')
 def test_baseline_configuration_missing(self):
  self.responses[('diff','--name-only')]='app/tests/meson.build\n'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_configuration_extra_edit(self):
  self.file('meson.build','config-after-extra')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_configuration_wrong_before(self):
  self.responses[('show','HEAD:meson.build')]='wrong'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_configuration_symlink(self):
  (self.root/'meson.build').unlink();(self.root/'meson.build').symlink_to(self.root/'app/tests/meson.build')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_current_unchanged(self):
  self.responses[('rev-parse','HEAD')],self.responses[('rev-parse','HEAD^{tree}')]=m.COMMITS['current']
  self.responses[('diff','--name-only')]='app/tests/meson.build\n'
  self.responses[('show','HEAD:meson.build')]='config-current'
  self.file('meson.build','config-current')
  self.assertFalse(self.invoke()['configuration_overlay']['applied'])
 def test_current_configuration_change_rejected(self):
  self.responses[('rev-parse','HEAD')],self.responses[('rev-parse','HEAD^{tree}')]=m.COMMITS['current']
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_extra_production_file_change_rejected(self):
  self.responses[('diff','--name-only')]+='app/core/gimp.c\n'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_policy_input_mismatch(self):
  self.file(m.CONFIGURATION_POLICY['file'],'wrong')
  with patch.object(m,'git',side_effect=self.git):
   with self.assertRaisesRegex(m.EvidenceError,'configuration-policy-mismatch'):m.source_identity(self.root,self.root)
 def test_wrong_tree(self):
  self.responses[('rev-parse','HEAD^{tree}')]='wrong'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_protected_edit(self):
  self.file('app/tests/meson.build','editedappend')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_extra_append(self):
  self.file('app/tests/meson.build','originalappendextra')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_untracked_extra(self):
  self.responses[('ls-files','--others','--exclude-standard')]+='bad.h\n'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_ignored_extra(self):
  self.responses[('ls-files','--others','--ignored','--exclude-standard')]='bad.h'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_staged_change(self):
  self.responses[('diff','--cached','--name-only')]='file'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_observer_tamper(self):
  self.file('app/tests/test-matched-preclick.c','wrong')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_common_tamper(self):
  self.file('tools/common.py','wrong')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_gitlink_tamper(self):
  self.responses[('ls-tree','HEAD','gimp-data')]='160000 commit wrong gimp-data'
  with self.assertRaises(m.EvidenceError):self.invoke()

class LoaderTests(FilesTests):
 def replay(self,output=None,env=None):
  binary=self.file('bin/test');provider=self.file('lib/libgimpwidgets.so')
  output=output or ('libgimpwidgets.so => '+str(provider)+' (0x1234)\nlibc.so.6 => /usr/lib/libc.so.6 (0x1234)\n/lib64/ld-linux-x86-64.so.2 (0x1234)\n')
  with patch.object(m,'run',side_effect=[' [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]\n',output]) as run,patch.object(m,'sha',return_value='h'):
   result=m.loader_providers(binary,self.root,self.root/'deps',env or {'LD_LIBRARY_PATH':':relative'})
   return result,run
 def test_loader_cwd(self):
  _,run=self.replay();self.assertEqual(run.call_args.kwargs['cwd'],self.root)
 def test_loader_exact_env(self):
  _,run=self.replay();self.assertEqual(run.call_args.kwargs['env']['LD_LIBRARY_PATH'],':relative')
 def test_loader_ambient_removed(self):
  with patch.dict(m.os.environ,{'LD_PRELOAD':'host.so','GLIBC_TUNABLES':'host'}):
   _,run=self.replay();self.assertNotIn('LD_PRELOAD',run.call_args.kwargs['env']);self.assertNotIn('GLIBC_TUNABLES',run.call_args.kwargs['env'])
 def test_loader_foreign_cell(self):
  with self.assertRaises(m.EvidenceError):self.replay('libgimpwidgets.so => /tmp/other/libgimpwidgets.so (0x1234)')
 def test_loader_system_gimp(self):
  with self.assertRaises(m.EvidenceError):self.replay('libgimpwidgets.so => /usr/lib/libgimpwidgets.so (0x1234)')
 def test_loader_malformed(self):
  with self.assertRaises(m.EvidenceError):self.replay('unexpected output')
 def test_loader_no_private_paths(self):self.assertNotIn(str(self.root),json.dumps(self.replay()[0]))
 def test_compile_relative_foreign_path(self):
  with self.assertRaises(m.EvidenceError):m.validate_compile_paths(['cc','-I../../other'],self.root,{'build':self.root})
 def test_compile_own_relative_path(self):m.validate_compile_paths(['cc','-I.'],self.root,{'build':self.root})
 def test_compile_system_path(self):m.validate_compile_paths(['cc','-isystem','/usr/include'],self.root,{'build':self.root})
 def test_compile_joined_foreign(self):
  with self.assertRaises(m.EvidenceError):m.validate_compile_paths(['cc','-isystem/tmp/other'],self.root,{'build':self.root})


class BuildTests(FilesTests):
 def setUp(self):
  super().setUp();self.s=self.root/'source';self.b=self.root/'build';self.d=self.root/'deps';self.p=self.root/'prefix'
  self.s.mkdir();self.b.mkdir();self.d.mkdir();self.p.mkdir();self.tool=self.file('tools/cc','tool')
  self.opts={'prefix':str(self.p),'buildtype':'debugoptimized','auto_features':'disabled','headless-tests':'disabled','painter-http':'disabled','gi-docgen':'disabled','appdata-test':'disabled','javascript':'disabled','vala':'disabled','libunwind':False,'libbacktrace':False,'enable-console-bin':False,'print':False,'check-update':'no','b_sanitize':'none'}
  self.targets=[]
  for name in m.TARGETS:
   if name in m.ALIASES:continue
   self.file('build/'+name,'binary');self.targets.append({'name':Path(name).name,'filename':[str(self.b/name)],'type':'custom' if name.startswith('menus/') else 'executable'})
  self.file('build/libgimp/libgimp.so','shared');self.file('deps/root/usr/lib/library.so','dependency');archive=self.file('deps/apt/archives/p.deb','archive')
  self.file('source/migration/baseline/debian13-package-lock.json',json.dumps({'packages':[{'filename':'p.deb','sha256':m.sha(archive)}]}))
  self.cc=[]
  for name in ['app/tests/test-matched-preclick.c']+['plug-ins/common/'+p+'.c' for p in m.PLUGINS]:
   src=self.file('source/'+name,'source');out=self.file('build/'+name+'.o','object');self.cc.append({'directory':str(self.b),'file':str(src),'arguments':['cc','-I'+str(self.s),'-c',str(src),'-o',str(out)]})
  role={'generated_resources':{p:'hash' for p in m.GENERATOR_RESOURCES}}
  for name in [m.GENERATOR['source']]+list(m.GENERATOR_RESOURCES[2:]):
   generated=name in m.GENERATOR_RESOURCES
   src=self.file(('build/' if generated else 'source/')+name,'source')
   obj=m.GENERATOR['object'] if not generated else name+'.o'
   out=self.file('build/'+obj,'object');args=['cc','-c',str(src),'-o',str(out)]
   self.cc.append({'directory':str(self.b),'file':str(src),'arguments':args})
   if generated:role['generated_resources'][name]=m.sha(src)
   else:
    recipe=m.normalize('\0'.join(args),{'source':self.s,'build':self.b,'deps':self.d,'prefix':self.p})
    change=patch.dict(m.GENERATOR,recipe_sha256=m.digest(recipe.encode()));change.start();self.addCleanup(change.stop)
  role_patch=patch.object(m,'generator_role',return_value=role);role_patch.start();self.addCleanup(role_patch.stop)
  self.commands='cc -shared -Wl,--as-needed -o libgimp/libgimp.so a.o\ncc -o app/tests/matched-preclick b.o libgimp/libgimp.so\n'
 def invoke(self):
  self.file('build/meson-info/intro-buildoptions.json',json.dumps([{'name':k,'value':v} for k,v in self.opts.items()]))
  self.file('build/meson-info/intro-targets.json',json.dumps(self.targets));self.file('build/meson-info/intro-tests.json',json.dumps([{'name':'matched-preclick','cmd':[str(self.b/'app/tests/matched-preclick')],'workdir':None}]))
  self.file('build/compile_commands.json',json.dumps(self.cc))
  def run(args,**kw):
   if args[3:5]==['-t','query']:return args[-1]+':\n  input: phony\n    po/a.gmo\n  outputs:\n'
   return self.commands
  identity={'source_commit':m.COMMITS['baseline'][0],'source_tree':m.COMMITS['baseline'][1],'arm':'baseline','source_root_identity':'source','configuration_overlay':m.configuration_overlay('baseline'),'common_inputs':m.COMMON,'overlay':m.OVERLAY}
  with patch.object(m,'source_identity',return_value=identity),patch.object(m,'run',side_effect=run),patch.object(m.shutil,'which',return_value=str(self.tool)):
   return m.build_identity(self.s,self.b,'baseline-normal',self.d)
 def test_synthetic_build_proof(self):self.assertTrue(self.invoke()['verified'])
 def test_wrong_sanitizer_option(self):
  self.opts['b_sanitize']='address,undefined'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_wrong_optimization(self):
  self.opts['buildtype']='release'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_wrong_unwind(self):
  self.opts['libunwind']=True
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_missing_plugin_target(self):
  self.targets.pop()
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_missing_plugin_object(self):
  self.cc.pop()
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_wrong_compile_cwd(self):
  self.cc[0]['directory']=str(self.s)
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_foreign_object(self):
  self.cc[0]['arguments'][-1]='/tmp/foreign.o'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_joined_forced_include(self):
  self.cc[0]['arguments'].append('-includeevil.h')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_response_file(self):
  self.cc[0]['arguments'].append('@args')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_sanitizer_in_normal_recipe(self):
  self.cc[0]['arguments'].append('-fsanitize=address,undefined')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_missing_observer_object(self):
  self.cc.pop(0)
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_wrong_archive(self):
  self.file('deps/apt/archives/p.deb','tampered')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_unreviewed_map_exception(self):
  self.commands+='cc -o other -Wl,--cref,-Map,/tmp/map a.o\n'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_baseline_map_exception_rejected(self):
  self.commands+='cc -o other -Wl,--cref,-Map,'+str(self.b/'libgimpwidgets/frame-helper-provider.map')+' a.o\n'
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_compile_foreign_search_path(self):
  self.cc[0]['arguments'].append('-I../other-source')
  with self.assertRaises(m.EvidenceError):self.invoke()
 def test_receipt_no_private_paths(self):self.assertNotIn(str(self.root),json.dumps(self.invoke()))


class RuntimeSettingsTests(unittest.TestCase):
 def env(self):return {'LANG':'C.UTF-8','LC_ALL':'C.UTF-8','GDK_BACKEND':'x11','GDK_SCALE':'1','GDK_DPI_SCALE':'1','GIMP_TESTING_SESSIONRC_NAME':'sessionrc-native-oncanvas','UI_TEST':'yes'}
 def test_binding_controls_presence(self):
  a=self.env();b=dict(a,LD_BIND_NOW='');self.assertNotEqual(m.runtime_settings(a),m.runtime_settings(b))
 def test_binding_controls_value(self):
  a=self.env();b=dict(a,LD_BIND_NOW='1');self.assertNotEqual(m.runtime_settings(a),m.runtime_settings(b))
 def test_tunables_value(self):
  a=self.env();b=dict(a,GLIBC_TUNABLES='glibc.malloc.tcache_count=0');self.assertNotEqual(m.runtime_settings(a),m.runtime_settings(b))
 def test_wrong_scale(self):
  a=self.env();a['GDK_SCALE']='2'
  with self.assertRaises(m.EvidenceError):m.runtime_settings(a)
 def test_unknown_test_override(self):
  a=self.env();a['GIMP_TEST_TOOLBAR_SINGLE']='1'
  with self.assertRaises(m.EvidenceError):m.runtime_settings(a)
 def test_unknown_brush_override(self):
  a=self.env();a['GIMP_BRUSH_POPUP_CAPTURE']='private'
  with self.assertRaises(m.EvidenceError):m.runtime_settings(a)
 def test_private_control_value_not_exported(self):
  a=self.env();a['GTK_THEME']='secret';self.assertNotIn('secret',json.dumps(m.runtime_settings(a)))
 def test_build_environment_foreign_include(self):
  with patch.dict(m.os.environ,{'CPATH':'/tmp/other/include'},clear=True):
   with self.assertRaises(m.EvidenceError):m.build_environment({'build':Path('/tmp/own')},Path('/tmp/own'))
 def test_build_environment_normalizes_cells(self):
  with patch.dict(m.os.environ,{'CPATH':'/tmp/one/include'},clear=True):a=m.build_environment({'build':Path('/tmp/one')},Path('/tmp/one'))
  with patch.dict(m.os.environ,{'CPATH':'/tmp/two/include'},clear=True):b=m.build_environment({'build':Path('/tmp/two')},Path('/tmp/two'))
  self.assertEqual(a,b)


if __name__=='__main__':unittest.main()
