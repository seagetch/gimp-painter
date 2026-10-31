import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('mask',Path(__file__).with_name('canvas_mask_provider.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Cases(unittest.TestCase):
 def row(self,old=False,full=None):
  binary='/build/'+m.SUBDIR+'/'+(m.OLD if old else m.FIXED)
  stdout='TAP version 14\n# Start of painter-canvas-ui tests\n'
  if old:stdout+='not ok '+m.CASE+' - '+m.CRITICAL+'\nBail out!\n'
  else:
   cases=full or [m.CASE];stdout+='1..'+str(len(cases))+'\n'+''.join('ok '+str(i+1)+' '+c+'\n' for i,c in enumerate(cases))
  return {'name':m.OLD if old else m.FIXED,'result':'EXPECTEDFAIL' if old else 'OK','returncode':-5 if old else 0,'stdout':stdout,'stderr':'','command':[binary]+([] if full else ['-p',m.CASE])}
 def test_exact_old(self):
  r=self.row(True);self.assertEqual(m.check_row([r],r['command'][0],True),r)
 def test_missing_stderr_is_empty(self):
  r=self.row();del r['stderr'];m.check_row([r],r['command'][0])
  r['stderr']=None
  with self.assertRaises(ValueError):m.check_row([r],r['command'][0])
 def test_exact_fixed(self):
  r=self.row();self.assertEqual(m.check_row([r],r['command'][0]),r)
 def test_full107(self):
  cases=['/painter-canvas-ui/test'+str(i) for i in range(107)];r=self.row(full=cases);m.check_row([r],r['command'][0],full=cases)
 def test_old_wrong_results(self):
  for code,status in ((0,'UNEXPECTEDPASS'),(-6,'EXPECTEDFAIL'),(77,'SKIP'),(-5,'FAIL'),(-5,'TIMEOUT'),(1,'EXPECTEDFAIL')):
   with self.subTest(code=code,status=status):
    r=self.row(True);r.update(returncode=code,result=status)
    with self.assertRaises(ValueError):m.check_row([r],r['command'][0],True)
 def test_old_wrong_assertion(self):
  for wrong in ('other_function','layer->other','Gimp-Other'):
   r=self.row(True);r['stdout']=r['stdout'].replace('gimp_layer_get_edit_mask' if wrong=='other_function' else 'layer->mask' if wrong=='layer->other' else 'Gimp-Core',wrong)
   with self.assertRaises(ValueError):m.check_row([r],r['command'][0],True)
 def test_old_additional_diagnostics(self):
  for text in ('WARNING: unrelated','ERROR: unrelated','CRITICAL: unrelated','assertion unrelated','AddressSanitizer','LeakSanitizer','runtime error: bad','# SKIP','ok 1 '+m.CASE,'1..0','not ok /painter-canvas-ui/other - '+m.CRITICAL):
   with self.subTest(text=text):
    r=self.row(True);r['stdout']+='\n'+text+'\n'
    with self.assertRaises(ValueError):m.check_row([r],r['command'][0],True)
 def test_old_extra_selector(self):
  r=self.row(True);r['command']+=['-p','/other']
  with self.assertRaises(ValueError):m.check_row([r],r['command'][0],True)
 def test_fixed_leaks_not_waived(self):
  for text in ('LeakSanitizer','AddressSanitizer','UndefinedBehaviorSanitizer','WARNING','ERROR:','CRITICAL','runtime error:','# SKIP','not ok 2 /other'):
   r=self.row();r['stderr']=text
   with self.assertRaises(ValueError):m.check_row([r],r['command'][0])
 def test_no_zero_or_duplicate(self):
  r=self.row()
  for rows in ([],[r,r]):
   with self.assertRaises(ValueError):m.check_row(rows,r['command'][0])
 def test_fixed_wrong_leaf_plan(self):
  for a,b in ((m.CASE,'/other'),('1..1','1..0'),('ok 1','ok 2')):
   r=self.row();r['stdout']=r['stdout'].replace(a,b)
   with self.assertRaises(ValueError):m.check_row([r],r['command'][0])
 def test_old_duplicate_assertion(self):
  r=self.row(True);r['stdout']+=r['stdout']
  with self.assertRaises(ValueError):m.check_row([r],r['command'][0],True)
 def test_modes(self):
  for v in ('none',[],['none']):m.check_options([{'name':'b_sanitize','value':v}],'normal')
  for v in ('address,undefined',['address','undefined']):m.check_options([{'name':'b_sanitize','value':v}],'sanitizer')
  for v in ('address','thread',['none','address']):
   with self.assertRaises(ValueError):m.check_options([{'name':'b_sanitize','value':v}],'normal')
 def test_compile_injection(self):
  for flag in ('@args','-include','-include/header','-imacros/header','-Dgimp_painter_layer_tiles_new=other','-Ugimp_layer_tile_view_new','-fno-sanitize=all','-fsanitize=address'):
   with self.assertRaises(ValueError):m.check_compile_flags(['cc',flag],'normal')
 def test_sanitizer_exact_compile(self):
  m.check_compile_flags(['cc','-fsanitize=address,undefined'],'sanitizer')
  for flags in ([],['-fsanitize=address'],['-fsanitize=address,undefined','-fsanitize=thread']):
   with self.assertRaises(ValueError):m.check_compile_flags(flags,'sanitizer')
 def test_ten_exact_public_api_names(self):
  self.assertEqual(len(m.APIS),10);self.assertEqual(len(set(m.APIS)),10)
  self.assertIn('gimp_layer_tile_view_new',m.APIS);self.assertIn('gimp_painter_layer_tiles_long_press_pending',m.APIS)
 def test_manifest_all_original(self):
  names=['x'+str(i) for i in range(107)];source=''.join('ADD('+x+');' for x in names)
  manifest={'registration':m.FIXED,'case_ids':['/painter-canvas-ui/'+x for x in names]}
  self.assertEqual(len(m.full_cases(source,manifest)),107)
  for bad in ({**manifest,'case_ids':[]},{**manifest,'case_ids':manifest['case_ids'][:-1]}, {**manifest,'registration':'other'}):
   with self.assertRaises(ValueError):m.full_cases(source,bad)
 def test_unapproved_loader(self):
  with self.assertRaises(ValueError):m.check_loader(Path('/tmp/ld-linux-x86-64.so.2'))

class ProviderFixture(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.source=self.root/'source';self.build=self.root/'build';self.source.mkdir();self.build.mkdir()
  self.mode='normal';self.targets=[m.FIXED,m.OLD]
  self.cases=['/painter-canvas-ui/test'+str(i) for i in range(107)]
  self.sourcefile(m.TEST,''.join('ADD(test'+str(i)+');' for i in range(107)))
  self.sourcefile(m.MANIFEST,json.dumps({'registration':m.FIXED,'case_ids':self.cases}))
  for name in m.PINNED:
   if name not in (m.TEST,m.MANIFEST):self.sourcefile(name,'input '+name)
  self.pins={name:m.sha(self.source/name) for name in m.PINNED}
  p=patch.object(m,'PINNED',self.pins);p.start();self.addCleanup(p.stop)
  self.tool=self.file(self.root/'tool','tool');self.loader=self.file(self.root/'ld-linux-x86-64.so.2','loader')
  self.widgets=self.file(self.build/'libgimpwidgets/libgimpwidgets-3.0.so.0','widgets');self.baselib=self.file(self.build/'libgimpbase/libgimpbase-3.0.so.0','base')
  self.file(self.build/'config.h','config')
  self.specs={'production':(m.PRODUCTION,'app/widgets/libappwidgets.a.p')}
  for t in self.targets:self.specs[t+'-test']=(m.TEST,m.SUBDIR+'/'+t+'.p');self.specs[t+'-support']=(m.SUPPORT,m.SUBDIR+'/'+t+'.p')
  self.specs['old']=(m.FIXTURE,m.SUBDIR+'/'+m.OLD+'.p');self.objects={};cc=[]
  for key,(src,parent) in self.specs.items():
   obj=self.file(self.build/parent/(Path(src).name+'.o'),'object '+key);self.objects[key]=obj
   cc.append({'file':str(self.source/src),'directory':str(self.build),'arguments':['cc','-c',str(self.source/src),'-o',str(obj)]})
  self.cc=cc;self.write_metadata()
  self.archive=self.file(self.build/'app/widgets/libappwidgets.a','!<arch>\nfixture')
  for t in self.targets:
   binary=self.file(self.build/m.SUBDIR/t,'binary '+t);self.write_map(t)
  self.runpatch=patch.object(m,'run',side_effect=self.fake_run);self.runpatch.start();self.addCleanup(self.runpatch.stop)
  p=patch.object(m.shutil,'which',return_value=str(self.tool));p.start();self.addCleanup(p.stop)
  p=patch.object(m,'check_loader');p.start();self.addCleanup(p.stop)
 def file(self,path,text):
  path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text);return path
 def sourcefile(self,name,text):return self.file(self.source/name,text)
 def write_metadata(self):
  self.file(self.build/'compile_commands.json',json.dumps(self.cc))
  self.file(self.build/'meson-info/intro-buildoptions.json',json.dumps([{'name':'b_sanitize','value':'none'}]))
  self.file(self.build/'meson-info/intro-tests.json',json.dumps([{'name':t,'cmd':[str(self.build/m.SUBDIR/t)],'workdir':None,'env':{'LD_LIBRARY_PATH':str(self.build/'libgimpwidgets')+':'+str(self.build/'libgimpbase')},'timeout':180,'suite':['gimp:painter-mask-proof' if t==m.FIXED else 'gimp:painter-mask-negative-control'],'is_parallel':False,'priority':0,'protocol':'exitcode','depends':[],'extra_paths':[]} for t in self.targets]))
  self.file(self.build/'meson-info/meson-info.json',json.dumps({'meson_version':m.MESON_VERSION,'directories':{'source':str(self.source),'build':str(self.build),'info':str(self.build/'meson-info')},'introspection':{'information':{'tests':{'file':'intro-tests.json','updated':True}}},'error':False}))
 def write_map(self,target):
  binary=self.build/m.SUBDIR/target;test=self.objects[target+'-test']
  provider=self.objects['old'] if target==m.OLD else self.objects['production']
  path=str(provider) if target==m.OLD else str(self.build/'app/widgets/libappwidgets.a')+'('+provider.name+')'
  text='OUTPUT('+str(binary)+')\nLOAD '+str(test)+'\nLOAD '+str(provider if target==m.OLD else self.build/'app/widgets/libappwidgets.a')+'\nLOAD '+str(self.objects[target+'-support'])+'\nCross Reference Table\nmain '+str(test)+'\ngimp_init_for_gui_testing '+str(self.objects[target+'-support'])+'\n'+''.join(a+' '+path+'\n' for a in m.APIS)
  self.file(binary.with_suffix('.map'),text)
 def sym(self,names,undefined=()):
  return '\n'.join(str(i)+': 0000 10 FUNC GLOBAL DEFAULT '+('UND' if n in undefined else '1')+' '+n for i,n in enumerate(names))
 def fake_run(self,args,**kwargs):
  if args[0]=='ar':
   if args[1]=='t':return self.objects['production'].name+'\n'
   return self.objects['production'].read_bytes()
  if args[0]=='ninja':
   obj=self.build/args[-1];deps=[self.build/'config.h']+[self.source/x for x in m.PINNED if x.endswith('.h')]
   return str(obj)+': #deps 5, deps mtime 1 (VALID)\n'+''.join('    '+str(x)+'\n' for x in deps)
  if args[0]=='readelf':
   if args[1]=='-dW':return ''
   if args[1]=='-lW':return '[Requesting program interpreter: '+str(self.loader)+']'
   path=Path(args[-1]);key=next((k for k,p in self.objects.items() if p==path),None)
   if key in ('production','old') or key is None:
    extra=('__asan_init','__ubsan_handle_type_mismatch_v1') if key is None and self.mode=='sanitizer' else ()
    return self.sym(m.APIS+('main','gimp_init_for_gui_testing')+extra,extra)
   if key.endswith('-test'):return self.sym(('main','gimp_painter_layer_tiles_new','gimp_init_for_gui_testing'),('gimp_painter_layer_tiles_new','gimp_init_for_gui_testing'))
   return self.sym(('gimp_init_for_gui_testing',))
  if args[0]==str(self.loader):
   output='libgimpwidgets-3.0.so.0 => '+str(self.widgets)+' (0x1234)\nlibgimpbase-3.0.so.0 => '+str(self.baselib)+' (0x5678)\n'
   if self.mode=='sanitizer':output+='libasan.so.8 => '+str(self.asanlib)+' (0x1234)\nlibubsan.so.1 => '+str(self.ubsanlib)+' (0x5678)\n'
   return output
  raise AssertionError('unexpected inspection '+str(args))
 def invoke(self):return m.provider(self.source,self.build,self.mode)
 def test_provider_exact(self):
  r=self.invoke();self.assertTrue(r['proof_passed']);m.validate_receipt(r);m.verify_files(r)
 def test_witness_both_targets_all_ten(self):
  r=self.invoke();w=r['provider_witness'];self.assertTrue(w['observational_only']);self.assertFalse(w['acceptance_eligible'])
  self.assertEqual(set(w['targets']),{m.OLD,m.FIXED})
  for target in (m.OLD,m.FIXED):
   item=w['targets'][target];self.assertEqual(item['status'],'parsed-single-cref');self.assertEqual([x['api'] for x in item['apis']],list(m.APIS))
   self.assertNotIn(str(self.root),json.dumps(item))
 def test_direct_form_in_regular_archive_remains_rejected(self):
  path=(self.build/m.SUBDIR/m.FIXED).with_suffix('.map');archive_token=str(self.archive)+'('+self.objects['production'].name+')'
  path.write_text(path.read_text().replace(archive_token,str(self.objects['production'])))
  with self.assertRaisesRegex(m.ProviderWitnessError,'wrong-archive-api-provider') as caught:self.invoke()
  witness=caught.exception.witness;self.assertEqual(set(witness['targets']),{m.OLD,m.FIXED})
  for api in witness['targets'][m.FIXED]['apis']:
   entry=api['entries'][0];self.assertEqual(entry['provider']['format'],'direct-expected-object');self.assertTrue(entry['provider']['direct_matches_expected_object'])
 def test_unknown_provider_never_exported_or_read(self):
  secret='/private/SecretToken/other.a(SecretMember.o)'
  with patch.object(Path,'resolve',side_effect=AssertionError('must not follow unknown paths')):
   item=m.token_witness(secret,self.build,self.objects['production'],self.archive,self.objects['production'].name)
  text=json.dumps(item);self.assertNotIn('Secret',text);self.assertNotIn('/private',text)
  self.assertEqual(item['format'],'archive-member');self.assertFalse(item['archive_matches_expected']);self.assertIsNone(item['normalized_member'])
 def test_witness_normalized_only_exact_known_paths(self):
  token='app/widgets/libappwidgets.a('+self.objects['production'].name+')'
  item=m.token_witness(token,self.build,self.objects['production'],self.archive,self.objects['production'].name)
  self.assertEqual(item['normalized_archive'],'build/app/widgets/libappwidgets.a')
  self.assertEqual(item['normalized_member'],'build/'+self.objects['production'].relative_to(self.build).as_posix())
 def test_old_comparison_does_not_mislabel_fixed_archive_member(self):
  token=str(self.archive)+'('+self.objects['production'].name+')'
  item=m.token_witness(token,self.build,self.objects['old'],self.archive,self.objects['production'].name,self.objects['production'])
  self.assertEqual(item['normalized_member'],'build/'+self.objects['production'].relative_to(self.build).as_posix())
  self.assertFalse(item['direct_matches_expected_object'])
 def test_witness_duplicate_definitions_bounded_still_rejected(self):
  path=(self.build/m.SUBDIR/m.FIXED).with_suffix('.map');path.write_text(path.read_text()+('\n'+m.APIS[0]+' /secret/provider')*20+'\n')
  with self.assertRaisesRegex(m.ProviderWitnessError,'ambiguous-api-provider') as caught:self.invoke()
  item=caught.exception.witness['targets'][m.FIXED]['apis'][0]
  self.assertEqual(item['defining_entry_count'],3);self.assertEqual(len(item['entries']),2);self.assertNotIn('/secret',json.dumps(item))
 def test_witness_missing_cref_still_rejected(self):
  path=(self.build/m.SUBDIR/m.FIXED).with_suffix('.map');path.write_text(path.read_text().replace('Cross Reference Table','No section'))
  with self.assertRaisesRegex(m.ProviderWitnessError,'missing-cref') as caught:self.invoke()
  item=caught.exception.witness['targets'][m.FIXED];self.assertEqual(item['status'],'missing-or-ambiguous-cref');self.assertEqual(len(item['apis']),10)
 def test_witness_raw_line_hash(self):
  path=(self.build/m.SUBDIR/m.FIXED).with_suffix('.map');text=path.read_text();w=m.map_witness(path,self.build,self.objects['production'],self.archive,self.objects['production'].name)
  line=next(x for x in text.splitlines() if x.startswith(m.APIS[0]+' '))
  self.assertEqual(w['apis'][0]['entries'][0]['line_sha256'],m.digest(line.encode()))
 def test_witness_oversized_token_only_hash(self):
  item=m.token_witness('SECRET'*1000,self.build,self.objects['production'],self.archive,'x')
  self.assertEqual(item['format'],'unrecognized');self.assertNotIn('SECRET',json.dumps(item))
 def test_witness_missing_map_finite(self):
  path=self.build/'absent.map';w=m.map_witness(path,self.build,self.objects['production'],self.archive,'x')
  self.assertEqual(w['status'],'missing-unsafe-oversized-or-undecodable-map');self.assertNotIn(str(path),json.dumps(w));self.assertEqual([x['api'] for x in w['apis']],list(m.APIS))
 def thin_direct(self):
  self.archive.write_bytes(b'!<thin>\nfixture')
  path=(self.build/m.SUBDIR/m.FIXED).with_suffix('.map');wrapped=str(self.archive)+'('+self.objects['production'].name+')'
  path.write_text(path.read_text().replace(wrapped,self.objects['production'].relative_to(self.build).as_posix()))
  original=self.fake_run
  def fake(args,**kw):
   if args[:2]==['ar','t']:return self.objects['production'].relative_to(self.build).as_posix()+'\n'
   return original(args,**kw)
  p=patch.object(m,'run',side_effect=fake);p.start();self.addCleanup(p.stop)
  return path
 def test_thin_exact_direct_provider_passes(self):
  self.thin_direct();r=self.invoke();self.assertTrue(r['proof_passed'])
  self.assertEqual(r['provider_witness']['archive_kind'],'thin')
  self.assertEqual(r['provider_witness']['ar_selected_member_sha256_scope'],'member-name-utf8')
  self.assertTrue(r['provider_witness']['archive_member_bytes_match_compiled_object'])
 def test_thin_wrong_object_fails(self):
  path=self.thin_direct();path.write_text(path.read_text().replace(self.objects['production'].relative_to(self.build).as_posix(),'other/libappwidgets.a.p/gimppainterlayertiles.cpp.o'))
  with self.assertRaisesRegex(ValueError,'wrong-archive-api-provider'):self.invoke()
 def test_thin_basename_only_fails(self):
  path=self.thin_direct();path.write_text(path.read_text().replace(self.objects['production'].relative_to(self.build).as_posix(),self.objects['production'].name))
  with self.assertRaisesRegex(ValueError,'wrong-archive-api-provider'):self.invoke()
 def test_thin_duplicate_definitions_fails(self):
  path=self.thin_direct();path.write_text(path.read_text()+'\n'+m.APIS[0]+' '+str(self.objects['production'])+'\n')
  with self.assertRaisesRegex(ValueError,'ambiguous-api-provider'):self.invoke()
 def test_thin_archive_load_required(self):
  path=self.thin_direct();path.write_text(path.read_text().replace('LOAD '+str(self.archive)+'\n',''))
  with self.assertRaisesRegex(ValueError,'map-load-missing'):self.invoke()
 def test_thin_unique_ar_membership_required(self):
  self.thin_direct();original=m.run
  with patch.object(m,'run',side_effect=lambda args,**kw:(original(args,**kw)*2 if args[:2]==['ar','t'] else original(args,**kw))):
   with self.assertRaisesRegex(ValueError,'missing-or-duplicate-archive-member'):self.invoke()
 def test_thin_ar_member_bytes_must_match(self):
  self.thin_direct();original=m.run
  with patch.object(m,'run',side_effect=lambda args,**kw:b'wrong' if args[:2]==['ar','p'] else original(args,**kw)):
   with self.assertRaisesRegex(ValueError,'archive-object-bytes-mismatch'):self.invoke()
 def test_thin_does_not_change_old_direct_check(self):
  self.thin_direct();path=(self.build/m.SUBDIR/m.OLD).with_suffix('.map');path.write_text(path.read_text().replace(str(self.objects['old']),str(self.objects['production'])))
  with self.assertRaises(ValueError):self.invoke()
 def test_thin_flag_requires_archive_context(self):
  with self.assertRaisesRegex(ValueError,'invalid-thin-archive-context'):m.map_defs('',self.build,self.build/'bin',self.objects['old'],m.APIS,thin_archive=True)
 def test_thin_flag_not_truthy_string(self):
  with self.assertRaisesRegex(ValueError,'invalid-thin-archive-context'):m.map_defs('',self.build,self.build/'bin',self.objects['production'],m.APIS,self.archive,thin_archive='true')
 def test_old_extracted_fixed_rejected(self):
  path=(self.build/m.SUBDIR/m.OLD).with_suffix('.map')
  path.write_text(path.read_text()+'app/widgets/libappwidgets.a(gimppainterlayertiles.cpp.o)\n')
  with self.assertRaisesRegex(ValueError,'fixed-provider-extracted'):self.invoke()
 def test_old_wrong_api_provider(self):
  path=(self.build/m.SUBDIR/m.OLD).with_suffix('.map');path.write_text(path.read_text().replace(m.APIS[-1]+' '+str(self.objects['old']),m.APIS[-1]+' wrong.o'))
  with self.assertRaises(ValueError):self.invoke()
 def test_fixed_wrong_api_provider(self):
  path=(self.build/m.SUBDIR/m.FIXED).with_suffix('.map');path.write_text(path.read_text().replace('libappwidgets.a(', 'wrong.a('))
  with self.assertRaises(ValueError):self.invoke()
 def test_missing_one_api_symbol(self):
  original=self.fake_run
  def fake(args,**kw):
   out=original(args,**kw)
   return '\n'.join(l for l in out.splitlines() if m.APIS[-1] not in l) if args[:2]==['readelf','-Ws'] else out
  with patch.object(m,'run',side_effect=fake):
   with self.assertRaises(ValueError):self.invoke()
 def test_archive_member_bytes_mismatch(self):
  original=self.fake_run
  with patch.object(m,'run',side_effect=lambda a,**k:b'wrong' if a[:2]==['ar','p'] else original(a,**k)):
   with self.assertRaisesRegex(ValueError,'archive-object-bytes'):self.invoke()
 def test_wrong_source(self):
  self.sourcefile(m.FIXTURE,'wrong')
  with self.assertRaises(ValueError):self.invoke()
 def test_changed_test(self):
  self.sourcefile(m.TEST,'wrong')
  with self.assertRaises(ValueError):self.invoke()
 def test_duplicate_compile_entry(self):
  self.cc.append(self.cc[0]);self.write_metadata()
  with self.assertRaises(ValueError):self.invoke()
 def test_forced_header(self):
  self.cc[0]['arguments']+=['-include/private'];self.write_metadata()
  with self.assertRaises(ValueError):self.invoke()
 def test_cross_build_object(self):
  self.cc[0]['arguments'][-1]=str(self.root/'other/libappwidgets.a.p/gimppainterlayertiles.cpp.o');self.write_metadata()
  with self.assertRaises(ValueError):self.invoke()
 def test_stale_dependencies(self):
  original=self.fake_run
  with patch.object(m,'run',side_effect=lambda a,**k:original(a,**k).replace('(VALID)','(STALE)') if a[0]=='ninja' else original(a,**k)):
   with self.assertRaises(ValueError):self.invoke()
 def test_missing_compiled_header(self):
  original=self.fake_run
  with patch.object(m,'run',side_effect=lambda a,**k:'\n'.join(l for l in original(a,**k).splitlines() if 'gimplayertileview.h' not in l) if a[0]=='ninja' else original(a,**k)):
   with self.assertRaises(ValueError):self.invoke()
 def test_wrong_expected_fail_metadata(self):
  p=self.build/'meson-info/intro-tests.json';v=json.loads(p.read_text());v[1]['should_fail']=False;p.write_text(json.dumps(v))
  with self.assertRaises(ValueError):self.invoke()
 def test_real_schema_missing_should_fail_is_supported_not_proof(self):
  r=self.invoke();self.assertFalse(r['expected_failure_semantics_verified'])
  for t in self.targets:
   d=r['registration_declarations'][t]
   self.assertIs(d['declared_should_fail'],t==m.OLD);self.assertFalse(d['native_result_verified']);self.assertFalse(d['introspection_exports_should_fail'])
 def test_registration_source_changed_rejected(self):
  self.sourcefile(m.REGISTRATION,'should_fail: false')
  with self.assertRaisesRegex(ValueError,'pinned-source-mismatch'):self.invoke()
 def test_registration_source_changed_after_receipt_rejected(self):
  r=self.invoke();self.sourcefile(m.REGISTRATION,'changed')
  with self.assertRaisesRegex(ValueError,'evidence-changed'):m.verify_files(r)
 def test_meson_version_changed_rejected(self):
  p=self.build/'meson-info/meson-info.json';v=json.loads(p.read_text());v['meson_version']['full']='1.8.0';p.write_text(json.dumps(v))
  with self.assertRaisesRegex(ValueError,'unsupported-meson-introspection-version'):self.invoke()
 def test_meson_info_missing_rejected(self):
  (self.build/'meson-info/meson-info.json').unlink()
  with self.assertRaises(ValueError):self.invoke()
 def test_meson_error_rejected(self):
  p=self.build/'meson-info/meson-info.json';v=json.loads(p.read_text());v['error']=True;p.write_text(json.dumps(v))
  with self.assertRaisesRegex(ValueError,'meson-introspection-error'):self.invoke()
 def test_meson_wrong_root_rejected(self):
  p=self.build/'meson-info/meson-info.json';v=json.loads(p.read_text());v['directories']['source']='/other';p.write_text(json.dumps(v))
  with self.assertRaisesRegex(ValueError,'wrong-meson-introspection-roots'):self.invoke()
 def test_meson_wrong_index_rejected(self):
  p=self.build/'meson-info/meson-info.json';v=json.loads(p.read_text());v['introspection']['information']['tests']['file']='other.json';p.write_text(json.dumps(v))
  with self.assertRaisesRegex(ValueError,'wrong-meson-tests-index'):self.invoke()
 def test_meson_unexpected_or_missing_field_rejected(self):
  p=self.build/'meson-info/intro-tests.json';original=p.read_text()
  for key in m.INTRO_TEST_FIELDS:
   v=json.loads(original);del v[0][key];p.write_text(json.dumps(v))
   with self.assertRaises(ValueError):self.invoke()
  for value in (True,False,None,'true',0):
   v=json.loads(original);v[0]['should_fail']=value;p.write_text(json.dumps(v))
   with self.assertRaisesRegex(ValueError,'unsupported-meson-test-schema'):self.invoke()
 def test_preflight_claim_cannot_be_promoted(self):
  r=self.invoke();r['expected_failure_semantics_verified']=True
  with self.assertRaisesRegex(ValueError,'invalid-registration-declaration-receipt'):m.validate_receipt(r)
 def test_receipt_boolean_not_integer(self):
  r=self.invoke();r['registration_declarations'][m.OLD]['declared_should_fail']=1
  with self.assertRaisesRegex(ValueError,'invalid-registration-declaration-types'):m.validate_receipt(r)
 def test_version_integer_not_boolean(self):
  p=self.build/'meson-info/meson-info.json';v=json.loads(p.read_text());v['meson_version']['major']=True;p.write_text(json.dumps(v))
  with self.assertRaisesRegex(ValueError,'unsupported-meson-introspection-version'):self.invoke()
 def test_missing_declared_failure_receipt_rejected(self):
  r=self.invoke();del r['registration_declarations'][m.OLD]
  with self.assertRaisesRegex(ValueError,'invalid-registration-declaration-receipt'):m.validate_receipt(r)
 def test_source_declaration_receipt_wrong_rejected(self):
  r=self.invoke();r['registration_declarations'][m.OLD]['declared_should_fail']=False
  with self.assertRaisesRegex(ValueError,'invalid-registration-declaration-receipt'):m.validate_receipt(r)
 def test_native_wrong_old_result_blocks_semantic_completion(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r)
  row=json.loads(logs['old'].read_text());row['result']='FAIL';logs['old'].write_text(json.dumps(row)+'\n')
  with self.assertRaisesRegex(ValueError,'wrong-native-result'):m.aggregate(p,logs['focused'],logs['old'],logs['full'])
 def test_native_wrong_fixed_result_blocks_semantic_completion(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r)
  row=json.loads(logs['focused'].read_text());row['result']='EXPECTEDFAIL';logs['focused'].write_text(json.dumps(row)+'\n')
  with self.assertRaisesRegex(ValueError,'wrong-native-result'):m.aggregate(p,logs['focused'],logs['old'],logs['full'])
 def test_empty_receipt_rejected(self):
  with self.assertRaises(ValueError):m.validate_receipt({'proof_passed':True})
 def test_missing_compiler_witness(self):
  r=self.invoke();del r['compilers']
  with self.assertRaisesRegex(ValueError,'missing-compiler-witness'):m.validate_receipt(r)
 def test_missing_compiler_hash(self):
  r=self.invoke();del r['inputs'][m.label(self.tool,self.source,self.build)]
  with self.assertRaisesRegex(ValueError,'missing-compiler-hash'):m.validate_receipt(r)
 def test_missing_tool_path(self):
  r=self.invoke();del r['inspection_tool_paths']['ar']
  with self.assertRaisesRegex(ValueError,'missing-inspection-tool-paths'):m.validate_receipt(r)
 def test_missing_core_receipt(self):
  r=self.invoke();del r['inputs']['build/compile_commands.json']
  with self.assertRaises(ValueError):m.validate_receipt(r)
 def test_tampered_binary(self):
  r=self.invoke();(self.build/m.SUBDIR/m.FIXED).write_text('changed')
  with self.assertRaises(ValueError):m.verify_files(r)
 def test_unsafe_receipt_label(self):
  r=self.invoke();r['inputs']['build/../secret']='0'*64
  with self.assertRaises(ValueError):m.verify_files(r)
 def test_missing_config_header_receipt(self):
  r=self.invoke();del r['inputs']['build/config.h']
  with self.assertRaises(ValueError):m.validate_receipt(r)
 def test_missing_pre_run_libraries(self):
  r=self.invoke();r['pre_run_libraries']={}
  with self.assertRaises(ValueError):m.validate_receipt(r)
 def test_pre_run_library_hash_missing(self):
  r=self.invoke();del r['inputs']['build/libgimpwidgets/libgimpwidgets-3.0.so.0']
  with self.assertRaises(ValueError):m.validate_receipt(r)
 def test_support_map_wrong_provider(self):
  path=(self.build/m.SUBDIR/m.OLD).with_suffix('.map');path.write_text(path.read_text().replace('gimp_init_for_gui_testing '+str(self.objects[m.OLD+'-support']),'gimp_init_for_gui_testing wrong.o'))
  with self.assertRaises(ValueError):self.invoke()
 def test_duplicate_json_rejected(self):
  path=self.file(self.root/'duplicate.json','{"a":1,"a":2}')
  with self.assertRaises(ValueError):m.read_json(path)
  with self.assertRaises(ValueError):m.rows(path)
 def test_map_output_does_not_consume_later_archive_parenthesis(self):
  self.assertTrue(self.invoke()['proof_passed'])
 def prepare_sanitizer(self):
  self.mode='sanitizer';self.targets=[m.FIXED]
  self.cc=[x for x in self.cc if m.OLD+'.p' not in x['arguments'][-1]]
  for x in self.cc:x['arguments'].append('-fsanitize=address,undefined')
  self.write_metadata();self.file(self.build/'meson-info/intro-buildoptions.json',json.dumps([{'name':'b_sanitize','value':['address','undefined']}]))
  self.asanlib=self.file(self.root/'libasan.so.8','asan');self.ubsanlib=self.file(self.root/'libubsan.so.1','ubsan')
 def test_sanitizer_provider_positive(self):
  self.prepare_sanitizer();r=self.invoke();m.validate_receipt(r);m.verify_files(r)
  self.assertEqual(set(r['binaries']),{m.FIXED})
 def test_sanitizer_one_leaf_aggregate(self):
  self.prepare_sanitizer();r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));row=Cases().row();row['command'][0]=r['binaries'][m.FIXED];row['env']={**self.env(r),'ASAN_OPTIONS':m.ASAN,'UBSAN_OPTIONS':m.UBSAN}
  log=self.file(self.root/'focused.json',json.dumps(row)+'\n')
  out=m.aggregate(p,log);self.assertTrue(out['proof_passed']);self.assertFalse(out['fixed_full107_passed'])
 def test_sanitizer_rejects_old_or_full_logs(self):
  self.prepare_sanitizer();r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r))
  with self.assertRaisesRegex(ValueError,'wrong-mode-log-set'):m.aggregate(p,'focused','old','full')
 def test_sanitizer_runtime_missing(self):
  self.prepare_sanitizer();original=self.fake_run
  def fake(args,**kw):
   out=original(args,**kw)
   return '\n'.join(l for l in out.splitlines() if 'libasan.so' not in l) if args[0]==str(self.loader) else out
  with patch.object(m,'run',side_effect=fake):
   with self.assertRaisesRegex(ValueError,'missing-sanitizer-runtime'):self.invoke()
 def test_pre_run_library_tamper_stops_before_replay(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r);self.widgets.write_text('tamper')
  with patch.object(m,'replay') as replay:
   with self.assertRaisesRegex(ValueError,'evidence-changed'):m.aggregate(p,logs['focused'],logs['old'],logs['full'])
   replay.assert_not_called()
 def test_sanitizer_controls_absent(self):
  self.mode='sanitizer';self.file(self.build/'meson-info/intro-buildoptions.json',json.dumps([{'name':'b_sanitize','value':'address,undefined'}]))
  with self.assertRaisesRegex(ValueError,'old-control-in-sanitizer'):self.invoke()
 def test_sanitizer_exact_runtime_options(self):
  r=self.invoke();r['mode']='sanitizer';raw={**self.env(r),'ASAN_OPTIONS':m.ASAN,'UBSAN_OPTIONS':m.UBSAN};m.runtime_settings({'env':raw},r)
  for key,value in (('ASAN_OPTIONS',m.ASAN.replace('detect_leaks=1','detect_leaks=0')),('UBSAN_OPTIONS',''),('LSAN_OPTIONS','')):
   with self.assertRaises(ValueError):m.runtime_settings({'env':{**raw,key:value}},r)
 def test_replay_uses_actual_env_and_exact_cwd(self):
  r=self.invoke();row={'env':{'LD_LIBRARY_PATH':':relative','LD_BIND_NOW':'1'}}
  calls=[];original=self.fake_run
  def fake(args,**kw):calls.append((args,kw));return original(args,**kw)
  with patch.object(m,'run',side_effect=fake):m.replay(r,row,m.FIXED)
  self.assertEqual(calls[0][1]['cwd'],str(self.build));self.assertEqual(calls[0][1]['env'],row['env'])
 def test_replay_rejects_injection_or_missing_environment(self):
  r=self.invoke()
  for env in (None,[],{'LD_PRELOAD':'secret'},{'LD_AUDIT':'secret'},{'LD_DEBUG':'libs'}):
   with patch.object(m,'run') as run:
    with self.assertRaises(ValueError):m.replay(r,{'env':env},m.FIXED)
    run.assert_not_called()
 def test_runtime_settings(self):
  r=self.invoke();raw=self.env(r);m.runtime_settings({'env':raw},r)
  for key,value in (('G_TEST_BUILDDIR',str(self.build/'app/tests/color-ownership')),('GIMP_TEST_PAINTER_CANVAS_GROUP','other'),('GDK_BACKEND','wayland'),('LSAN_OPTIONS',''),('ASAN_OPTIONS','anything'),('PAINTER_CANVAS_DEMO',''),('GIMP_TESTING_PAINTER_SESSION_PAIR','x')):
   with self.assertRaises(ValueError):m.runtime_settings({'env':{**raw,key:value}},r)
 def test_missing_icon_fixture_root_rejected(self):
  r=self.invoke();raw=self.env(r);del raw['G_TEST_BUILDDIR']
  with self.assertRaisesRegex(ValueError,'runtime-settings-mismatch'):m.runtime_settings({'env':raw},r)
 def env(self,r):
  return {'G_TEST_BUILDDIR':str(self.build/'app/tests'),**m.MESON_SANITIZER_DEFAULTS,'LD_LIBRARY_PATH':str(self.build/'libgimpwidgets')+':'+str(self.build/'libgimpbase')+(':'+m.os.environ['LD_LIBRARY_PATH'] if 'LD_LIBRARY_PATH' in m.os.environ else ''),'GDK_BACKEND':'x11','GDK_SCALE':'1','GDK_DPI_SCALE':'1','UI_TEST':'yes','GIMP_TEST_PAINTER_CANVAS_GROUP':'original','GIMP_TESTING_SESSIONRC_NAME':'sessionrc-native-oncanvas','GIMP_TESTING_ABS_TOP_SRCDIR':r['source_root'],'GIMP_TESTING_ABS_TOP_BUILDDIR':r['build_root'],'GIMP_TESTING_PLUGINDIRS':str(self.build/'plug-ins/common')}
 def logs(self,r):
  paths={};case=Cases()
  for name,old,full in (('focused',False,None),('old',True,None),('full',False,self.cases)):
   row=case.row(old,full);row['command'][0]=r['binaries'][m.OLD if old else m.FIXED];row['env']=self.env(r)
   paths[name]=self.file(self.root/(name+'.json'),json.dumps(row)+'\n')
  return paths
 def test_aggregate_all_proof(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r)
  closure=r['pre_run_libraries'][m.FIXED]
  with patch.object(m,'replay',return_value=(closure,'env')):
   out=m.aggregate(p,logs['focused'],logs['old'],logs['full'])
  self.assertTrue(out['proof_passed']);self.assertTrue(out['fixed_full107_passed']);self.assertFalse(out['acceptance_eligible']);self.assertTrue(out['expected_failure_semantics_verified'])
 def test_no_replay_after_tamper(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r);(self.build/m.SUBDIR/m.FIXED).write_text('tamper')
  with patch.object(m,'replay') as replay:
   with self.assertRaises(ValueError):m.aggregate(p,logs['focused'],logs['old'],logs['full'])
   replay.assert_not_called()
 def test_post_replay_tamper_rejected(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r)
  closure=r['pre_run_libraries'][m.FIXED]
  def fake(*args):
   (self.build/'config.h').write_text('tampered');return closure,'env'
  with patch.object(m,'replay',side_effect=fake):
   with self.assertRaisesRegex(ValueError,'evidence-changed'):m.aggregate(p,logs['focused'],logs['old'],logs['full'])
 def test_missing_full_log_rejected(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r)
  with self.assertRaises(ValueError):m.aggregate(p,logs['focused'],logs['old'])
 def test_runtime_provider_mismatch(self):
  r=self.invoke();p=self.file(self.root/'provider.json',json.dumps(r));logs=self.logs(r)
  with patch.object(m,'replay',side_effect=[({'a':{'path':'a','sha256':'x'}},'env'),({'b':{'path':'b','sha256':'y'}},'env')]):
   with self.assertRaisesRegex(ValueError,'pre-post-runtime-provider|paired-runtime-provider'):m.aggregate(p,logs['focused'],logs['old'],logs['full'])


def make_diagnostic_source(source):
 (source/'app/tests').mkdir(parents=True);(source/'tools/brush-ci').mkdir(parents=True)
 cases=[m.CASE]+['/painter-canvas-ui/test'+str(i) for i in range(106)]
 (source/m.MANIFEST).write_text(json.dumps({'registration':m.FIXED,'case_ids':cases}))
 (source/m.TEST).write_text('g_assert_true (value);\n'+''.join('ADD('+c.split('/')[-1]+');\n' for c in cases)+'#include "test-painter-fixture.inc"\n')
 (source/'app/tests/test-painter-fixture.inc').write_text('g_test_trap_assert_passed ();\n')
 return source

class NativeDetail(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.source=make_diagnostic_source(Path(self.tmp.name)/'source')
 def detail(self,out='',err=''):return m.native_failure_detail(out,err,self.source)
 def test_allowlisted_leaf_identity(self):
  r=self.detail('ok 1 '+m.CASE+'\nnot ok 2 /painter-canvas-ui/test0 - private-secret\nBail out!\n')
  self.assertEqual(r['last_passed_case'],m.CASE);self.assertEqual(r['failed_cases'],['/painter-canvas-ui/test0']);self.assertTrue(r['bailout_seen']);self.assertNotIn('private-secret',json.dumps(r))
 def test_unnumbered_failure_and_unknown_redacted(self):
  r=self.detail('not ok '+m.CASE+' - secret\nnot ok /private-secret\n')
  self.assertEqual(r['failed_cases'],[m.CASE]);self.assertEqual(r['unknown_leaf_count'],1);self.assertNotIn('secret',json.dumps(r))
 def test_source_location_validated(self):
  r=self.detail(err='ERROR:/secret/test-painter-canvas-ui.c:1:private_function: assertion failed: secret')
  self.assertEqual(r['source_assertions'],[{'source':m.TEST,'line':1,'assertion_kind':'g_assert_true'}]);self.assertNotIn('secret',json.dumps(r));self.assertNotIn('private_function',json.dumps(r))
 def test_include_trap_classification(self):
  r=self.detail(err='ERROR:/secret/test-painter-fixture.inc:1:fn: child process failed unexpectedly')
  self.assertEqual(r['source_assertions'][0]['assertion_kind'],'g_test_trap_assert_passed');self.assertTrue(r['subprocess_failure_seen'])
 def test_unknown_and_nonassert_lines_redacted(self):
  r=self.detail(err='ERROR:/secret/test-painter-private.c:1:x\nERROR:/secret/test-painter-canvas-ui.c:2:x\nERROR:/secret/test-painter-canvas-ui.c:99999:x')
  self.assertEqual(r['source_assertions'],[]);self.assertEqual(r['unmatched_assertion_locations'],3);self.assertNotIn('private',json.dumps(r))
 def test_missing_source_fails_closed(self):
  (self.source/m.TEST).unlink()
  with self.assertRaises(ValueError):self.detail()

class Observations(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.build=Path(self.tmp.name);(self.build/'meson-logs').mkdir()
  self.log=self.build/'meson-logs/canvas-mask-normal-fixed-focused.json';self.row=Cases().row();self.row['command'][0]=str(self.build/m.SUBDIR/m.FIXED)
  source=make_diagnostic_source(self.build/'source');original=m.native_failure_detail
  guard=patch.object(m,'native_failure_detail',side_effect=lambda out,err,unused:original(out,err,source));guard.start();self.addCleanup(guard.stop)
 def observe(self):return m.observe(self.log,self.build,'normal',m.FIXED,'focused',1)
 def write(self,row=None):self.log.write_text(json.dumps(row or self.row)+'\n')
 def test_native_failure_retained_no_acceptance(self):
  self.row.update(result='FAIL',returncode=1,stderr='LeakSanitizer: detected memory leaks');self.write();r=self.observe()
  self.assertTrue(r['observation_complete']);self.assertEqual(r['native_returncode'],1);self.assertEqual(r['meson_shell_exit'],1)
  self.assertIn('leak-sanitizer',r['categories']);self.assertFalse(r['proof_passed']);self.assertFalse(r['acceptance_eligible'])
 def test_no_env_paths_prose_export(self):
  self.row.update(stderr='/private/Secret error: token=TopSecret',env={'PRIVATE_SECRET':'Password123'});self.write();r=self.observe();text=json.dumps(r)
  for token in ('Secret','TopSecret','Password123',str(self.build),'/private'):self.assertNotIn(token,text)
  self.assertEqual(r['raw_log_sha256'],m.sha(self.log))
 def test_unknown_result_not_exported(self):
  self.row['result']='privateSECRET';self.write();r=self.observe();self.assertEqual(r['meson_result'],'unknown');self.assertNotIn('privateSECRET',json.dumps(r))
 def test_missing_log(self):self.assertFalse(self.observe()['observation_complete'])
 def test_duplicate_json(self):
  self.log.write_text('{"result":"OK","result":"FAIL"}');self.assertFalse(self.observe()['observation_complete'])
 def test_symlink_log(self):
  self.log.symlink_to('/etc/passwd');self.assertFalse(self.observe()['observation_complete'])
 def test_wrong_log_path(self):
  with self.assertRaises(ValueError):m.observe(self.build/'other.json',self.build,'normal',m.FIXED,'focused',0)
 def test_wrong_combinations(self):
  for mode,target,selection in (('sanitizer',m.OLD,'focused'),('normal',m.OLD,'full107'),('sanitizer',m.FIXED,'full107')):
   with self.assertRaises(ValueError):m.observe(self.log,self.build,mode,target,selection,0)
 def test_timeout_and_skip(self):
  for status,code,label in (('TIMEOUT',-9,'meson-timeout'),('SKIP',77,'meson-skip')):
   self.row.update(result=status,returncode=code);self.write();self.assertIn(label,self.observe()['categories'])
 def test_oversized_log(self):
  self.log.write_bytes(b'x'*100)
  with patch.object(m,'MAX_LOG',99):self.assertFalse(self.observe()['observation_complete'])
 def test_wrong_command_observation_not_proof(self):
  self.row['command']=['privatecommand'];self.write();r=self.observe();self.assertIn('unexpected-command',r['categories']);self.assertNotIn('privatecommand',json.dumps(r))
 def test_empty_rows(self):
  self.log.write_text('');self.assertFalse(self.observe()['observation_complete'])


class InspectionDiagnostic(unittest.TestCase):
 def test_all_stages_fail_closed_and_redact(self):
  import subprocess
  for stage in m.INSPECTION_STAGES:
   for code,stderr in ((1,b'/secret/Token: failed'),(0,b'readelf: Warning: Secret warning')):
    completed=subprocess.CompletedProcess(['secret'],code,b'0xabcdef Secret stdout',stderr)
    with patch.object(m.subprocess,'run',return_value=completed):
     with self.assertRaises(m.InspectionError) as caught:m.run(['readelf','secret'],stage=stage)
    d=caught.exception.diagnostic
    self.assertEqual(str(caught.exception),'inspection-failed');self.assertEqual(d['stage'],stage)
    self.assertEqual(d['returncode'],code);self.assertEqual(d['stderr_sha256'],m.digest(stderr))
    self.assertFalse(d['acceptance_eligible']);self.assertNotIn('Secret',json.dumps(d));self.assertNotIn('0xabcdef',json.dumps(d))
 def test_unknown_stage_not_executed(self):
  with patch.object(m.subprocess,'run') as run:
   with self.assertRaisesRegex(ValueError,'unknown-inspection-stage'):m.run(['secret'],stage='/secret')
   run.assert_not_called()
 def test_success_is_unchanged(self):
  import subprocess
  with patch.object(m.subprocess,'run',return_value=subprocess.CompletedProcess([],0,b'ok',b'')):
   self.assertEqual(m.run(['readelf'],stage='object-symbols'),'ok')
 def test_failure_propagated_to_provider_witness(self):
  diagnostic={'stage':'runtime-loader-list','returncode':127}
  def fail(source,build,mode,witness):
   witness['targets']={};raise m.InspectionError('inspection-failed',diagnostic)
  with patch.object(m,'_provider',side_effect=fail):
   with self.assertRaises(m.ProviderWitnessError) as caught:m.provider('source','build','normal')
  self.assertEqual(caught.exception.inspection,diagnostic)
 def test_categories_are_finite_and_no_prose(self):
  import subprocess
  stderr=b"/Secret: error while loading shared libraries: private.so\nversion `SECRET' not found\nsymbol lookup error\n"
  d=m.inspection_diagnostic('runtime-loader-list',subprocess.CompletedProcess([],127,b'',stderr))
  self.assertEqual(d['categories'],['missing-shared-library','missing-version','symbol-lookup-error'])
  self.assertNotIn('SECRET',json.dumps(d));self.assertNotIn('private.so',json.dumps(d))

class SanitizerDefaults(unittest.TestCase):
 def raw(self,mode):
  r={'mode':mode,'source_root':'/source','build_root':'/build'}
  env={'G_TEST_BUILDDIR':'/build/app/tests','GDK_BACKEND':'x11','GDK_SCALE':'1','GDK_DPI_SCALE':'1','UI_TEST':'yes','GIMP_TEST_PAINTER_CANVAS_GROUP':'original','GIMP_TESTING_SESSIONRC_NAME':'sessionrc-native-oncanvas','GIMP_TESTING_ABS_TOP_SRCDIR':'/source','GIMP_TESTING_ABS_TOP_BUILDDIR':'/build','GIMP_TESTING_PLUGINDIRS':'/build/plug-ins/common',**m.MESON_SANITIZER_DEFAULTS}
  if mode=='sanitizer':env.update(ASAN_OPTIONS=m.ASAN,UBSAN_OPTIONS=m.UBSAN)
  return r,env
 def test_defaults_and_strict_sanitizer(self):
  for mode in ('normal','sanitizer'):
   r,env=self.raw(mode);m.runtime_settings({'env':env},r)
   for key in m.MESON_SANITIZER_DEFAULTS:
    for value in (None,'','detect_leaks=0',env[key]+':suppressions=/secret'):
     changed=dict(env)
     if value is None:del changed[key]
     else:changed[key]=value
     with self.subTest(mode=mode,key=key,value=value):
      with self.assertRaises(ValueError):m.runtime_settings({'env':changed},r)
   with self.assertRaisesRegex(ValueError,'lsan-override'):m.runtime_settings({'env':{**env,'LSAN_OPTIONS':''}},r)


class PredictedEnvironment(unittest.TestCase):
 def predict(self,raw,inherited=None):
  return m.predicted_registration_environment(inherited or {},{'env':raw},Path('/build'))
 def test_exact_prepend_preserves_inherited_tail(self):
  self.assertEqual(self.predict({'LD_LIBRARY_PATH':'/build/a:/build/b','X':'Y'},{'LD_LIBRARY_PATH':'/deps:/prefix','Z':'V'}),{'LD_LIBRARY_PATH':'/build/a:/build/b:/deps:/prefix','X':'Y','Z':'V'})
 def test_absent_and_empty_inherited_differ(self):
  raw={'LD_LIBRARY_PATH':'/build/a'}
  self.assertEqual(self.predict(raw)['LD_LIBRARY_PATH'],'/build/a')
  self.assertEqual(self.predict(raw,{'LD_LIBRARY_PATH':''})['LD_LIBRARY_PATH'],'/build/a:')
 def test_no_inherited_tail_expansion(self):
  self.assertEqual(self.predict({'LD_LIBRARY_PATH':'/build/a'},{'LD_LIBRARY_PATH':'$SECRET:relative::/deps'})['LD_LIBRARY_PATH'],'/build/a:$SECRET:relative::/deps')
 def test_missing_and_invalid_prefix_rejected(self):
  for prefix in (None,'','/build','/outside','relative','/build/../outside','/build/a:','/build/a::/build/b','/build/a:/build/a'):
   with self.subTest(prefix=prefix):
    with self.assertRaises(ValueError):self.predict({} if prefix is None else {'LD_LIBRARY_PATH':prefix})
 def test_registration_loader_override_rejected(self):
  for key in ('LD_PRELOAD','LD_AUDIT','LD_DEBUG','LD_BIND_NOW','GLIBC_TUNABLES'):
   with self.subTest(key=key):
    with self.assertRaises(ValueError):self.predict({'LD_LIBRARY_PATH':'/build/a',key:''})
 def test_inherited_types_rejected(self):
  with self.assertRaises(ValueError):self.predict({'LD_LIBRARY_PATH':'/build/a'},{'LD_LIBRARY_PATH':None})

if __name__=='__main__':unittest.main()
