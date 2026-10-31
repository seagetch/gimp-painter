#!/usr/bin/env python3
"""Exact focused canvas mask provider proof; never substitutes for common gates."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

OLD = 'canvas-mask-old-provider'
FIXED = 'canvas-mask-fixed-provider'
SUBDIR = 'app/tests/color-ownership'
FIXTURE = SUBDIR + '/fixtures/gimppainterlayertiles-e25.cpp'
PRODUCTION = 'app/widgets/gimppainterlayertiles.cpp'
TEST = 'app/tests/test-painter-canvas-ui.c'
SUPPORT = 'app/tests/tests.c'
REGISTRATION = SUBDIR + '/meson.build'
MESON_VERSION = {'full':'1.7.0','major':1,'minor':7,'patch':0}
INTRO_TEST_FIELDS = frozenset(('cmd','env','name','workdir','timeout','suite','is_parallel','priority','protocol','depends','extra_paths'))
MANIFEST = 'tools/brush-ci/canvas-mask-full-cases.json'
ASAN = 'halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1'
UBSAN = 'halt_on_error=1:print_stacktrace=1'
MESON_SANITIZER_DEFAULTS = {
 'ASAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1',
 'UBSAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1:print_stacktrace=1',
 'MSAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1:print_stacktrace=1'}
INSPECTION_STAGES = frozenset(('object-dependencies','archive-members','archive-member-bytes',
 'object-symbols','binary-symbols','binary-dynamic','binary-interpreter','runtime-loader-list'))
class InspectionError(ValueError):
 def __init__(self, code, diagnostic):
  super().__init__(code); self.diagnostic=diagnostic

def inspection_diagnostic(stage, result):
 # No argv, environment, path, addresses or tool prose leave the private runner.
 out={'schema':'canvas-mask-inspection-diagnostic-v1','observational_only':True,
      'acceptance_eligible':False,'stage':stage,'returncode':result.returncode,
      'stdout_bytes':len(result.stdout),'stderr_bytes':len(result.stderr),
      'stdout_sha256':digest(result.stdout),'stderr_sha256':digest(result.stderr),
      'categories':[]}
 for label,pattern in (('missing-shared-library',rb'error while loading shared libraries'),
                       ('missing-version',rb'version [`\'][^\n]{0,128}[\'] not found'),
                       ('symbol-lookup-error',rb'symbol lookup error'),
                       ('readelf-error',rb'readelf: Error:'),
                       ('readelf-warning',rb'readelf: Warning:'),
                       ('sanitizer-diagnostic',rb'AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer')):
  if re.search(pattern,result.stderr[:65536]):out['categories'].append(label)
 return out

CASE = '/painter-canvas-ui/renderer_mask_cache_and_release'
APIS = tuple('gimp_painter_layer_tiles_' + x for x in ('get_type','new','set_image','set_display','cancel_interaction','popup','move','long_press_pending')) + ('gimp_layer_tile_view_get_type','gimp_layer_tile_view_new')
PINNED = {
 REGISTRATION:'264ac3573cf1c70186769a9db609e0e10279fe676eb06a6b125e481bcc04f319',
 MANIFEST:'cbeb7f0ec9acbe527acdf37c587f8d6cfa0420e8679c10dfeefd811fcbe89553',
 FIXTURE:'8c6addf0e246a1cb9800e3eb515f4af9797dabdd6b51e72a83c4ca2bc8d7ea6d',
 PRODUCTION:'e92dfef13a7f6cdcf7a2cddcd0f06e6c6c6304b641cf877cfa355ddfabe23dd3',
 TEST:'32cc6ad0b168f902591e713eeabb8d46526969e59c89e2225e81abe0059f321f',
 SUPPORT:'56303878e316e41fcb7c439b199cde5bd9fc4ac84b46b48efcbfbc79737747e7',
 'app/tests/tests.h':'5541a82208a639234d6d91b0f70d8dacefe6ebd59a3082f84487409db55de1fe',
 'app/widgets/gimppainterlayertiles.h':'3d8a51e76366fda4a3ccb3080c6c9541560711a7b2bf9eae424b071922e314b1',
 'app/widgets/gimplayertileview.h':'f01627897a7d5b78d34198eac7a826c0f40bb070aade2a564ac843a35ee441c7'}
MAX_LOG = 32*1024*1024
MAX_MAP = 256*1024*1024
MAX_TOOL = 64*1024*1024
CRITICAL = "Gimp-Core-FATAL-CRITICAL: gimp_layer_get_edit_mask: assertion 'layer->mask' failed"
LOADER_VARS = ('LD_LIBRARY_PATH','LD_BIND_NOW','GLIBC_TUNABLES')
FORBIDDEN_RUNTIME = ('PAINTER_CANVAS_DEMO','GIMP_TESTING_PAINTER_SESSION_DIRECTORY','GIMP_TESTING_PAINTER_SESSION_PAIR',
 'GIMP_TEST_LAYER_UI_FILTER','GIMP_TEST_COLORIFY_ONCANVAS','GIMP_TEST_TOOLBAR_PAINTER','GIMP_TEST_TOOLBAR_SINGLE',
 'GIMP_TEST_TOOLBAR_SESSION_DIRECTORY','GIMP_TEST_TOOLBAR_SESSION_PASS','GIMP_TEST_COMMON_POPUP_SCREENSHOTS',
 'GIMP_TEST_COMMON_POPUP_DIAGNOSTICS','GIMP_TEST_COMMON_POPUP_DOCK_TRACE','GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES',
 'GIMP_TEST_COMMON_LAYER_POPUP','GIMP_TEST_FILTER_LAYER_POPUP','GIMP_TEST_OVERLAY_PAINT_GATE',
 'GIMP_BRUSH_POPUP_CAPTURE','GIMP_BRUSH_POPUP_BOTTOM_CAPTURE')


def require(value, code):
 if not value: raise ValueError(code)
def sha(path):
 with Path(path).open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def digest(data): return hashlib.sha256(data).hexdigest()
def base(kind):
 return {'schema':'canvas-mask-'+kind+'-v1','acceptance_eligible':False,'proof_passed':False,'errors':[],
         'scope':'bounded-provider-substitution-not-full-historical-build-parity','raw_environment_exported':False}
def read_bytes(path, limit):
 p=Path(path);require(p.resolve()==p.absolute() and p.is_file(),'nonregular-or-symlinked-input')
 with p.open('rb') as f: data=f.read(limit+1)
 require(len(data)<=limit,'oversized-input');return data
def unique(items):
 out={}
 for key,value in items:
  require(key not in out,'duplicate-json-key');out[key]=value
 return out
def read_json(path): return json.loads(read_bytes(path,MAX_LOG),object_pairs_hook=unique)
def rows(path):
 data=read_bytes(path,MAX_LOG)
 return [json.loads(x,object_pairs_hook=unique) for x in data.decode().splitlines() if x.strip()],digest(data)
def resolve(value,cwd): return (Path(value) if Path(value).is_absolute() else cwd/value).resolve()
def run(args,cwd=None,env=None,binary=False,stage=None):
 require(stage in INSPECTION_STAGES,'unknown-inspection-stage')
 effective=dict(os.environ,LC_ALL='C')
 if env is not None:
  for k in list(effective):
   if k.startswith('LD_') or k=='GLIBC_TUNABLES':effective.pop(k)
  effective.update(env)
 p=subprocess.run(args,cwd=cwd,env=effective,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90)
 if p.returncode!=0 or p.stderr.strip():raise InspectionError('inspection-failed',inspection_diagnostic(stage,p))
 require(len(p.stdout)<=MAX_TOOL,'oversized-inspection')
 return p.stdout if binary else p.stdout.decode()
def symbols(text):
 out=[]
 for line in text.splitlines():
  m=re.match(r'\s*\d+:\s+[0-9a-fA-F]+\s+\d+\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)',line)
  if m:
   typ,bind,vis,ndx,name=m.groups();out.append((name.split('@')[0],typ,bind,ndx!='UND'))
 require(out,'missing-symbol-table');return out
def defined(table,name):return any(x==(name,'FUNC','GLOBAL',True) for x in table)
def check_options(options,mode):
 vals=[x['value'] for x in options if x.get('name')=='b_sanitize'];require(len(vals)==1,'ambiguous-sanitizer-option')
 value=vals[0];value=','.join(value) if isinstance(value,list) else value
 require(value in (('none','') if mode=='normal' else ('address,undefined','undefined,address')),'wrong-sanitizer-mode')
def check_meson_info(info,source,build):
 # Meson1.7.0 get_test_list does not export should_fail. Do not invent that
 # field or deserialize private pickle data. Pin the producer schema instead.
 require(isinstance(info,dict) and info.get('meson_version')==MESON_VERSION and all(type(info['meson_version'][k]) is type(v) for k,v in MESON_VERSION.items()),'unsupported-meson-introspection-version')
 require(info.get('error') is False,'meson-introspection-error')
 require(info.get('directories')=={'source':str(source),'build':str(build),'info':str(build/'meson-info')},'wrong-meson-introspection-roots')
 tests=info.get('introspection',{}).get('information',{}).get('tests',{})
 require(tests.get('file')=='intro-tests.json' and type(tests.get('updated')) is bool,'wrong-meson-tests-index')
def check_registration(reg,target,binary,build):
 require(isinstance(reg,dict) and set(reg)==INTRO_TEST_FIELDS,'unsupported-meson-test-schema')
 require(reg.get('name')==target and reg.get('cmd')==[str(binary)] and reg.get('workdir') in (None,str(build)),'wrong-registration-command-cwd')
 require(isinstance(reg.get('env'),dict) and all(isinstance(k,str) and isinstance(v,str) for k,v in reg['env'].items()),'invalid-registration-env')
 require(not any(reg['env'].get(x) for x in ('LD_PRELOAD','LD_AUDIT')),'metadata-loader-injection')
 return {'declared_should_fail':target==OLD,'declaration_source':REGISTRATION,
         'declaration_sha256':PINNED[REGISTRATION],
         'introspection_exports_should_fail':False,'native_result_verified':False,
         'scope':'pinned-source-declaration-only; actual semantics require exact native Meson result'}
def predicted_registration_environment(inherited,reg,build):
 # Meson1.7.0 backend prepends build shared-library directories. Introspection
 # evaluates that operation on {}, whereas mtest applies it to inherited env.
 # The whole registration source is pinned and declares no loader variables.
 # Reconstruct only that known prepend, never evaluate shell expansion.
 raw=reg['env'];prefix=raw.get('LD_LIBRARY_PATH')
 require(isinstance(prefix,str) and prefix,'missing-registration-library-prefix')
 require(not any(k.startswith('LD_') and k!='LD_LIBRARY_PATH' or k=='GLIBC_TUNABLES' for k in raw),'unexpected-registration-loader-setting')
 parts=prefix.split(':')
 require(len(parts)==len(set(parts)) and all(x and Path(x).is_absolute() and '..' not in Path(x).parts and Path(os.path.normpath(x))==Path(x) and Path(x).is_relative_to(build) and Path(x)!=build for x in parts),'invalid-registration-library-prefix')
 require(isinstance(inherited,dict) and all(isinstance(k,str) and isinstance(v,str) for k,v in inherited.items()),'invalid-inherited-environment')
 predicted=dict(inherited);predicted.update(raw)
 # An explicitly empty inherited value is retained exactly as Meson's trailing
 # empty component. Actual replay must later match environment/provider hashes.
 if 'LD_LIBRARY_PATH' in inherited:predicted['LD_LIBRARY_PATH']=prefix+':'+inherited['LD_LIBRARY_PATH']
 return predicted

def check_registration_receipt(r,targets):
 expected={t:{'declared_should_fail':t==OLD,'declaration_source':REGISTRATION,
             'declaration_sha256':PINNED[REGISTRATION],
             'introspection_exports_should_fail':False,'native_result_verified':False,
             'scope':'pinned-source-declaration-only; actual semantics require exact native Meson result'} for t in targets}
 require(r.get('registration_declarations')==expected and r.get('meson_version')==MESON_VERSION and r.get('expected_failure_semantics_verified') is False,'invalid-registration-declaration-receipt')
 for target in targets:
  require(all(type(r['registration_declarations'][target][key]) is bool for key in ('declared_should_fail','introspection_exports_should_fail','native_result_verified')),'invalid-registration-declaration-types')
def check_compile_flags(args,mode):
 require(not any(x.startswith(('@','-include','-imacros','-fno-sanitize')) for x in args),'alternate-compile-input')
 require(not any(re.match(r'-[DU]gimp_(?:painter_layer_tiles|layer_tile_view)',x) for x in args),'api-namespace-override')
 flags=[x for x in args if x.startswith('-fsanitize')]
 require((not flags) if mode=='normal' else flags in (['-fsanitize=address,undefined'],['-fsanitize=undefined,address']),'wrong-compile-sanitizer')
class ProviderWitnessError(ValueError):
 def __init__(self,code,witness,inspection=None):
  super().__init__(code);self.witness=witness;self.inspection=inspection

def token_witness(token,build,provider,archive,member,archive_object=None):
 # Never open a path taken from a map token. Only compare its normalized identity
 # to known, already-proven providers. Unknown paths remain hashes/finite labels.
 archive_object=archive_object or provider
 def lexical(value,root):return Path(os.path.normpath(os.path.join(str(root),value)))
 out={'path_comparison':'lexical-only-no-unknown-path-reads','token_sha256':digest(token.encode()),'token_bytes':len(token.encode()),
      'format':'unrecognized','archive_matches_expected':False,
      'member_equals_ar_entry':False,'member_matches_build_object':False,
      'member_matches_archive_relative_object':False,'direct_matches_expected_object':False,
      'normalized_archive':None,'normalized_member':None,'normalized_object':None}
 if len(token)>4096:return out
 parsed=re.fullmatch(r'(.+)\(([^()]+)\)',token)
 if parsed:
  out['format']='archive-member'
  out['archive_matches_expected']=lexical(parsed[1],build)==archive
  out['member_equals_ar_entry']=parsed[2]==member
  out['member_matches_build_object']=lexical(parsed[2],build)==archive_object
  out['member_matches_archive_relative_object']=lexical(parsed[2],archive.parent)==archive_object
  if out['archive_matches_expected']:out['normalized_archive']='build/'+archive.relative_to(build).as_posix()
  if any(out[k] for k in ('member_equals_ar_entry','member_matches_build_object','member_matches_archive_relative_object')):
   out['normalized_member']='build/'+archive_object.relative_to(build).as_posix()
 elif not any(c in token for c in '()'):
  out['format']='path-token'
  out['direct_matches_expected_object']=lexical(token,build)==provider
  if out['direct_matches_expected_object']:
   out['format']='direct-expected-object';out['normalized_object']='build/'+provider.relative_to(build).as_posix()
 return out

def map_witness(path,build,provider,archive,member,archive_object=None):
 out={'status':'unreadable','map_sha256':None,'cross_reference_section_count':0,'output_records':0,
      'expected_object':'build/'+provider.relative_to(build).as_posix(),
      'expected_archive':'build/'+archive.relative_to(build).as_posix(),
      'apis':[{'api':api,'defining_entry_count':0,'entries':[]} for api in APIS]}
 try:
  data=read_bytes(path,MAX_MAP);out['map_sha256']=digest(data);text=data.decode()
  count=text.count('Cross Reference Table');out['cross_reference_section_count']=min(count,3)
  out['output_records']=min(len(re.findall(r'^OUTPUT\(',text,re.M)),3)
  out['status']='parsed-single-cref' if count==1 else 'missing-or-ambiguous-cref'
  table=text.split('Cross Reference Table',1)[1] if count==1 else ''
  for api_index,api in enumerate(APIS):
   lines=[line for line in table.splitlines() if re.match(r'^'+re.escape(api)+r'(?:[ \t]|$)',line)]
   item={'api':api,'defining_entry_count':min(len(lines),3),'entries':[]}
   for line in lines[:2]:
    entry={'line_sha256':digest(line.encode()),'line_bytes':len(line.encode()),'parse_state':'unrecognized-line','provider':None}
    parsed=re.fullmatch(re.escape(api)+r'[ \t]+(\S+)[ \t]*',line)
    if parsed:
     entry['parse_state']='symbol-and-provider-token';entry['provider']=token_witness(parsed[1],build,provider,archive,member,archive_object)
    item['entries'].append(entry)
   out['apis'][api_index]=item
 except (OSError,ValueError,UnicodeError):
  out['status']='missing-unsafe-oversized-or-undecodable-map'
 return out


def map_defs(text,build,binary,provider,apis,archive=None,member=None,thin_archive=False):
 require(type(thin_archive) is bool and (archive is not None or not thin_archive),'invalid-thin-archive-context')
 require('Cross Reference Table' in text,'missing-cref')
 output=re.findall(r'^OUTPUT\(([^\s()]+)(?:[ \t]+[^)\r\n]*)?\)[ \t]*$',text,re.M)
 require(len(output)==1 and resolve(output[0],build)==binary,'map-output-mismatch')
 loads={resolve(x,build) for x in re.findall(r'^LOAD\s+(\S+)\s*$',text,re.M)}
 require((archive or provider) in loads,'map-load-missing')
 table=text.split('Cross Reference Table',1)[1]
 for api in apis:
  matches=re.findall(r'^'+re.escape(api)+r'\s+(\S+)\s*$',table,re.M)
  require(len(matches)==1,'ambiguous-api-provider')
  actual=matches[0]
  if archive:
   m=re.fullmatch(r'(.+)\(([^()]+)\)',actual)
   wrapped=m and resolve(m[1],build)==archive and (m[2]==member or resolve(m[2],build)==provider or resolve(m[2],archive.parent)==provider)
   # GNU thin-archive cref may name the exact member object directly. The caller
   # permits this only after thin magic, unique ar membership, and ar-p bytes
   # matching this compiled object have all been verified. Never basename-only.
   direct=thin_archive and not any(c in actual for c in '()') and resolve(actual,build)==provider
   require(wrapped or direct,'wrong-archive-api-provider')
  else:require(resolve(actual,build)==provider,'wrong-direct-api-provider')
def full_cases(test,manifest):
 cases=manifest.get('case_ids')
 require(manifest.get('registration')==FIXED and isinstance(cases,list) and len(cases)==107 and len(set(cases))==107,'wrong-full-test-manifest')
 registered=['/painter-canvas-ui/'+x for x in re.findall(r'\bADD\(([A-Za-z_][A-Za-z_0-9]*)\)\s*;',test)]
 require(all(isinstance(c,str) and registered.count(c)==1 for c in cases),'manifest-source-registration-mismatch')
 return cases
def check_row(records,binary,old=False,full=None):
 require(len(records)==1 and isinstance(records[0],dict),'one-registration-required');row=records[0]
 target=OLD if old else FIXED
 require(str(row.get('name','')).split(' / ')[-1]==target,'wrong-registration')
 require(type(row.get('returncode')) is int and row['returncode']==(-5 if old else 0) and row.get('result')==('EXPECTEDFAIL' if old else 'OK'),'wrong-native-result')
 require(row.get('command')==[str(binary)]+([] if full is not None else ['-p',CASE]),'wrong-selected-command')
 stdout,stderr=row.get('stdout'),row.get('stderr','');require(isinstance(stdout,str) and isinstance(stderr,str),'missing-raw-output')
 require(re.search(r'^TAP version [0-9]+$',stdout,re.M) and '# Start of painter-canvas-ui tests' in stdout,'missing-tap-hierarchy')
 text=stdout+'\n'+stderr
 require(not re.search(r'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:|Traceback|#\s*(?:SKIP|TODO)\b',text,re.I),'sanitizer-or-skip-or-exception')
 if old:
  expected='not ok '+CASE+' - '+CRITICAL
  require(stdout.splitlines().count(expected)==1 and stdout.splitlines().count('Bail out!')==1,'wrong-intended-assertion')
  require(not re.search(r'^ok\b|^1\.\.0\b',stdout,re.M),'old-passed-or-zero')
  plans=re.findall(r'^1\.\.(\d+)\s*$',stdout,re.M);require(plans in ([],['1']),'old-wrong-plan')
  for line in text.splitlines():
   if re.search(r'CRITICAL|WARNING|ERROR:|assertion|^not ok|^Bail out!',line,re.I):
    require(line in (expected,'Bail out!'),'unrelated-old-diagnostic')
 else:
  require(not re.search(r'CRITICAL|WARNING|ERROR:|Bail out!|^not ok',text,re.I|re.M),'candidate-diagnostic')
  cases=full if full is not None else [CASE]
  require(re.findall(r'^ok (\d+) (\S+)\s*$',stdout,re.M)==[(str(i+1),c) for i,c in enumerate(cases)],'wrong-pass-leaves')
  require(len(re.findall(r'^ok\b',stdout,re.M))==len(cases) and re.findall(r'^1\.\.(\d+)\s*$',stdout,re.M)==[str(len(cases))],'wrong-tap-plan')
 return row

def label(p,source,build):
 for prefix,root in (('source',source),('build',build)):
  if p.is_relative_to(root):return prefix+'/'+p.relative_to(root).as_posix()
 return 'external/'+str(p).lstrip('/')
def provider(source,build,mode):
 witness={}
 try:return _provider(source,build,mode,witness)
 except (OSError,ValueError,TypeError,KeyError,AttributeError,subprocess.SubprocessError) as exc:
  if witness:raise ProviderWitnessError(str(exc),witness,getattr(exc,'diagnostic',None)) from None
  raise

def _provider(source,build,mode,witness):
 require(mode in ('normal','sanitizer'),'unknown-mode')
 require(not os.environ.get('LD_PRELOAD') and not os.environ.get('LD_AUDIT'),'injected-loader')
 source,build=Path(source).resolve(),Path(build).resolve();evidence=set()
 for rel,h in PINNED.items():
  if rel==FIXTURE and mode=='sanitizer':continue
  p=source/rel;require(p.resolve()==p and sha(p)==h,'pinned-source-mismatch');evidence.add(p)
 test_cases=full_cases((source/TEST).read_text(),read_json(source/MANIFEST))
 ccpath=build/'compile_commands.json';optpath=build/'meson-info/intro-buildoptions.json';metapath=build/'meson-info/intro-tests.json'
 infopath=build/'meson-info/meson-info.json';check_meson_info(read_json(infopath),source,build)
 cc=read_json(ccpath);check_options(read_json(optpath),mode);metadata=read_json(metapath);evidence.update((ccpath,optpath,metapath,infopath))
 targets=[FIXED]+([OLD] if mode=='normal' else [])
 if mode=='sanitizer':require(not any(x.get('name')==OLD for x in metadata) and not any(resolve(x['file'],Path(x['directory']).resolve())==source/FIXTURE for x in cc),'old-control-in-sanitizer-build')
 specs={'production':(PRODUCTION,'libappwidgets.a.p')}
 for target in targets:
  specs[target+'-test']=(TEST,target+'.p');specs[target+'-support']=(SUPPORT,target+'.p')
 if mode=='normal':specs['old']=(FIXTURE,OLD+'.p')
 objects={};recipes={};compilers={}
 for key,(rel,parent) in specs.items():
  matches=[]
  for entry in cc:
   directory=Path(entry['directory']).resolve()
   if resolve(entry['file'],directory)!=source/rel:continue
   args=entry.get('arguments') or shlex.split(entry['command']);require(isinstance(args,list) and all(isinstance(x,str) for x in args),'invalid-compile-command')
   outputs=[args[i+1] for i,x in enumerate(args[:-1]) if x=='-o'];require(len(outputs)==1,'ambiguous-object')
   raw=Path(outputs[0]);raw=raw if raw.is_absolute() else directory/raw;obj=raw.resolve()
   if parent not in obj.parts:continue
   require(obj==raw.absolute() and obj.is_relative_to(build),'cross-build-object')
   require('-c' in args and sum(resolve(x,directory)==source/rel for x in args if not x.startswith('-'))==1,'wrong-direct-compile-input')
   check_compile_flags(args,mode)
   compiler=shutil.which(args[0]);require(compiler,'missing-compiler-identity');evidence.add(Path(compiler).resolve());compilers[key]={'path':str(Path(compiler).resolve()),'sha256':sha(Path(compiler).resolve())}
   matches.append(obj);recipes[key]=digest(json.dumps(args,separators=(',',':')).encode())
  require(len(matches)==1,'missing-or-duplicate-compile-entry');objects[key]=matches[0];evidence.add(matches[0])
 deps_hash={}
 for key,obj in objects.items():
  dep=run(['ninja','-C',str(build),'-t','deps',str(obj.relative_to(build))],stage='object-dependencies');heads=[x for x in dep.splitlines() if ': #deps ' in x]
  require(len(heads)==1 and heads[0].endswith('(VALID)') and resolve(heads[0].split(': #deps ',1)[0],build)==obj,'missing-stale-object-deps')
  paths={resolve(x.strip(),build) for x in dep.splitlines() if x.startswith('    ') and x.strip()}
  required={build/'config.h'}
  if key in ('old','production'):required.update(source/x for x in ('app/widgets/gimppainterlayertiles.h','app/widgets/gimplayertileview.h'))
  if key.endswith('-support'):required.add(source/'app/tests/tests.h')
  require(required<=paths,'missing-compiled-headers')
  for path in paths:
   if path.suffix in ('.h','.hpp'):require(path.is_file(),'missing-dependency');evidence.add(path)
  deps_hash[key]=digest(dep.encode())
 archive=build/'app/widgets/libappwidgets.a';member=objects['production'].name
 members=run(['ar','t',str(archive)],stage='archive-members').splitlines()
 with archive.open('rb') as stream:thin=stream.read(8)==b'!<thin>\n'
 matching=[x for x in members if ((resolve(x,build)==objects['production'] or resolve(x,archive.parent)==objects['production']) if thin else x==member)]
 require(len(matching)==1,'missing-or-duplicate-archive-member');member=matching[0]
 require(digest(run(['ar','p',str(archive),member],binary=True,stage='archive-member-bytes'))==sha(objects['production']),'archive-object-bytes-mismatch');evidence.add(archive)
 # Collect both target maps before any defining-provider guard. This witness
 # diagnoses serialization/identity differences; it never changes admission.
 witness.update(schema='canvas-mask-defining-provider-witness-v1',observational_only=True,
  acceptance_eligible=False,archive_kind='thin' if thin else 'regular',
  archive_sha256=sha(archive),compiled_fixed_object_sha256=sha(objects['production']),
  ar_selected_member_sha256=digest(member.encode()),ar_selected_member_sha256_scope='member-name-utf8',
  archive_member_bytes_match_compiled_object=True,targets={})
 for target in targets:
  expected=objects['old'] if target==OLD else objects['production']
  witness['targets'][target]=map_witness((build/SUBDIR/target).with_suffix('.map'),build,expected,archive,member,objects['production'])
  witness['targets'][target]['expected_object_sha256']=sha(expected)
  witness['targets'][target]['expected_defining_form']='direct-object' if target==OLD else ('archive-member-or-thin-direct-exact-object' if thin else 'archive-member')

 elfs={key:symbols(run(['readelf','-Ws',str(obj)],stage='object-symbols')) for key,obj in objects.items()}
 for key in ['production']+(['old'] if mode=='normal' else []):require(all(defined(elfs[key],a) for a in APIS),'missing-object-api-definitions')
 for target in targets:
  require(any(x[0]=='gimp_painter_layer_tiles_new' and not x[3] for x in elfs[target+'-test']),'test-does-not-reference-provider')
  require(not any(x[0] in APIS and x[3] for key in (target+'-test',target+'-support') for x in elfs[key]),'test-interposes-provider')
 binaries={};maps={};loaders={};prelibs={};pre_envhash={};declarations={}
 for target in targets:
  binary=build/SUBDIR/target;mapfile=binary.with_suffix('.map');text=read_bytes(mapfile,MAX_MAP).decode()
  table=symbols(run(['readelf','-Ws',str(binary)],stage='binary-symbols'));require(all(defined(table,a) for a in APIS),'missing-binary-api-definitions')
  if mode=='sanitizer':require(all(any(x[0].startswith(prefix) and not x[3] for x in table) for prefix in ('__asan_','__ubsan_')),'missing-sanitizer-elf-references')
  else:require(not any(x[0].startswith(('__asan_','__ubsan_','__lsan_')) for x in table),'normal-elf-instrumented')
  map_defs(text,build,binary,objects[target+'-test'],('main',))
  require(any(x[0]=='gimp_init_for_gui_testing' and not x[3] for x in elfs[target+'-test']),'test-missing-harness-reference')
  require(defined(elfs[target+'-support'],'gimp_init_for_gui_testing') and defined(table,'gimp_init_for_gui_testing'),'support-definition-missing')
  map_defs(text,build,binary,objects[target+'-support'],('gimp_init_for_gui_testing',))
  if target==OLD:
   map_defs(text,build,binary,objects['old'],APIS)
   require(not re.search(r'libappwidgets\.a\([^)]*gimppainterlayertiles[^)]*\)',text),'fixed-provider-extracted-in-old')
  else:map_defs(text,build,binary,objects['production'],APIS,archive,member,thin_archive=thin)
  dyn=run(['readelf','-dW',str(binary)],stage='binary-dynamic');require(not re.search(r'\((?:AUDIT|DEPAUDIT)\)',dyn),'elf-audit-injection')
  interps=re.findall(r'Requesting program interpreter:\s*(\S+)\]',run(['readelf','-lW',str(binary)],stage='binary-interpreter'));require(len(interps)==1,'missing-loader')
  loader=Path(interps[0]).resolve();check_loader(loader);evidence.add(loader)
  regs=[x for x in metadata if x.get('name')==target];require(len(regs)==1,'missing-registration')
  reg=regs[0];declarations[target]=check_registration(reg,target,binary,build)
  evidence.update((binary,mapfile));binaries[target]=str(binary);maps[target]=str(mapfile);loaders[target]=str(loader)
  predicted=predicted_registration_environment(dict(os.environ),reg,build)
  prelibs[target],pre_envhash[target]=replay({'build_root':str(build),'runtime_workdir':str(build),'loaders':loaders,'binaries':binaries,'mode':mode},{'env':predicted},target)
  for item in prelibs[target].values():evidence.add(Path(item['path']))
 tools={};tool_paths={}
 for name in ('readelf','ninja','ar'):
  path=shutil.which(name);require(path,'missing-inspection-tool');path=Path(path).resolve();evidence.add(path);tools[name]=sha(path);tool_paths[name]=str(path)
 for p in evidence:require(p.is_file() and p.resolve()==p,'changed-or-symlinked-evidence')
 r=base('provider');r.update(proof_passed=True,mode=mode,source_root=str(source),build_root=str(build),runtime_workdir=str(build),
  producer_sha256=sha(__file__),registration_declarations=declarations,meson_version=MESON_VERSION,expected_failure_semantics_verified=False,inputs={label(p,source,build):sha(p) for p in sorted(evidence)},objects={k:str(p.relative_to(build)) for k,p in objects.items()},
  compile_recipe_sha256=recipes,compilers=compilers,dependency_record_sha256=deps_hash,inspection_tools=tools,inspection_tool_paths=tool_paths,binaries=binaries,maps=maps,loaders=loaders,
  provider_witness=witness,api_definitions=list(APIS),full_cases=test_cases,runtime_binding_complete=False,pre_run_libraries=prelibs,
  pre_run_loader_environment_sha256=pre_envhash,pre_run_resolution_scope='pinned Meson1.7 build-library prepend applied to inherited environment; actual Meson environment must independently resolve identical providers after execution')
 return r

def check_loader(path):
 require(path.is_absolute() and path.resolve()==path and path.name=='ld-linux-x86-64.so.2' and path.parent in (Path('/usr/lib/x86_64-linux-gnu'),Path('/lib/x86_64-linux-gnu'),Path('/usr/lib64'),Path('/lib64')),'unapproved-loader')
def validate_receipt(r):
 require(r.get('schema')=='canvas-mask-provider-v1' and r.get('proof_passed') is True and r.get('errors')==[] and r.get('acceptance_eligible') is False and r.get('producer_sha256')==sha(__file__),'invalid-provider-receipt')
 mode=r.get('mode');require(mode in ('normal','sanitizer'),'unknown-mode');source=Path(r['source_root']);build=Path(r['build_root'])
 require(source.is_absolute() and build.is_absolute() and source.resolve()==source and build.resolve()==build and r.get('runtime_workdir')==str(build),'invalid-receipt-roots')
 targets={FIXED,OLD} if mode=='normal' else {FIXED}
 check_registration_receipt(r,targets)
 require(set(r.get('binaries',{}))==targets and set(r.get('maps',{}))==targets and set(r.get('loaders',{}))==targets,'incomplete-target-evidence')
 require(r.get('api_definitions')==list(APIS),'missing-api-proof')
 expected_objects={'production',FIXED+'-test',FIXED+'-support'}|({'old',OLD+'-test',OLD+'-support'} if mode=='normal' else set())
 require(set(r.get('objects',{}))==expected_objects and set(r.get('dependency_record_sha256',{}))==expected_objects and set(r.get('compile_recipe_sha256',{}))==expected_objects and set(r.get('inspection_tools',{}))=={'ar','ninja','readelf'},'incomplete-object-proof')
 inputs=r.get('inputs');require(isinstance(inputs,dict) and inputs,'missing-input-evidence')
 require(set(r.get('compilers',{}))==expected_objects,'missing-compiler-witness')
 for item in r['compilers'].values():
  path=Path(item['path']);require(path.is_absolute() and inputs.get(label(path,source,build))==item['sha256'],'missing-compiler-hash')
 require(set(r.get('inspection_tool_paths',{}))=={'ar','ninja','readelf'},'missing-inspection-tool-paths')
 for name,pathtext in r['inspection_tool_paths'].items():
  path=Path(pathtext);require(path.is_absolute() and inputs.get(label(path,source,build))==r['inspection_tools'][name],'missing-inspection-tool-hash')
 for rel,h in PINNED.items():
  if rel==FIXTURE and mode=='sanitizer':continue
  require(inputs.get('source/'+rel)==h,'missing-pinned-source-evidence')
 required={'build/meson-info/meson-info.json','build/compile_commands.json','build/meson-info/intro-buildoptions.json','build/meson-info/intro-tests.json','build/app/widgets/libappwidgets.a','build/config.h'}
 for obj in r['objects'].values():
  require(not Path(obj).is_absolute() and '..' not in Path(obj).parts,'escaping-object-label');required.add('build/'+obj)
 for target in targets:
  require(r['binaries'][target]==str(build/SUBDIR/target) and r['maps'][target]==str((build/SUBDIR/target).with_suffix('.map')),'wrong-receipt-target')
  required.update(('build/'+SUBDIR+'/'+target,'build/'+SUBDIR+'/'+target+'.map'))
  loader=Path(r['loaders'][target]);check_loader(loader);required.add('external/'+str(loader).lstrip('/'))
 require(required<=inputs.keys(),'missing-core-evidence')
 require(set(r.get('pre_run_libraries',{}))==targets and set(r.get('pre_run_loader_environment_sha256',{}))==targets,'missing-pre-run-resolution')
 for libs in r['pre_run_libraries'].values():
  require(isinstance(libs,dict) and libs,'empty-pre-run-libraries')
  for prefix in ('libgimpwidgets-3.0.so','libgimpbase-3.0.so'):
   hits=[x for name,x in libs.items() if name.startswith(prefix)];require(len(hits)==1 and Path(hits[0]['path']).is_relative_to(build),'missing-pre-run-project-provider')
  for item in libs.values():
   path=Path(item['path']);require(path.is_absolute() and inputs.get(label(path,source,build))==item['sha256'],'missing-pre-run-provider-hash')
 require(r.get('full_cases')==full_cases(read_bytes(source/TEST,MAX_LOG).decode(),read_json(source/MANIFEST)),'wrong-manifest')
def verify_files(r):
 require(not os.environ.get('LD_PRELOAD') and not os.environ.get('LD_AUDIT'),'injected-loader')
 roots={'source':Path(r['source_root']),'build':Path(r['build_root']),'external':Path('/')}
 for name,h in r['inputs'].items():
  prefix,rel=name.split('/',1);require(prefix in roots and not Path(rel).is_absolute() and '..' not in Path(rel).parts and re.fullmatch('[0-9a-f]{64}',h),'invalid-evidence-label')
  p=roots[prefix]/rel;require(p.resolve()==p and p.is_file() and sha(p)==h,'evidence-changed')
def runtime_settings(row,r):
 raw=row.get('env');require(isinstance(raw,dict) and all(isinstance(k,str) and isinstance(v,str) for k,v in raw.items()),'missing-full-meson-env')
 expected={'G_TEST_BUILDDIR':str(Path(r['build_root'])/'app/tests'),'GDK_BACKEND':'x11','GDK_SCALE':'1','GDK_DPI_SCALE':'1','UI_TEST':'yes',
           'GIMP_TEST_PAINTER_CANVAS_GROUP':'original','GIMP_TESTING_SESSIONRC_NAME':'sessionrc-native-oncanvas',
           'GIMP_TESTING_ABS_TOP_SRCDIR':r['source_root'],'GIMP_TESTING_ABS_TOP_BUILDDIR':r['build_root'],
           'GIMP_TESTING_PLUGINDIRS':str(Path(r['build_root'])/'plug-ins/common')}
 require(all(raw.get(k)==v for k,v in expected.items()),'runtime-settings-mismatch')
 require('LSAN_OPTIONS' not in raw,'lsan-override')
 require(not any(k in raw for k in FORBIDDEN_RUNTIME),'runtime-fixture-override')
 expected_sanitizer=dict(MESON_SANITIZER_DEFAULTS)
 if r['mode']=='sanitizer':expected_sanitizer.update(ASAN_OPTIONS=ASAN,UBSAN_OPTIONS=UBSAN)
 require(all(raw.get(k)==v for k,v in expected_sanitizer.items()),'sanitizer-settings-mismatch' if r['mode']=='sanitizer' else 'normal-sanitizer-override')
 return digest(json.dumps(expected,sort_keys=True).encode())

def replay(r,row,target):
 raw=row.get('env');require(isinstance(raw,dict) and all(isinstance(k,str) and isinstance(v,str) for k,v in raw.items()),'missing-full-meson-env')
 require(not any(v for k,v in raw.items() if k.startswith('LD_') and k not in LOADER_VARS),'injected-runtime-loader')
 env={k:raw[k] for k in LOADER_VARS if k in raw};loader=r['loaders'][target]
 listing=run([loader,'--list',r['binaries'][target]],cwd=r['runtime_workdir'],env=env,stage='runtime-loader-list')
 require('not found' not in listing,'unresolved-provider');libs={}
 for line in listing.splitlines():
  if not line.strip() or 'linux-vdso' in line:continue
  m=re.fullmatch(r'\s*(?:(\S+)\s+=>\s+)?(/\S+)\s+\(0x[0-9a-fA-F]+\)\s*',line);require(m,'unparsed-loader-output')
  name=m[1] or Path(m[2]).name;p=Path(m[2]).resolve();require(name not in libs,'duplicate-loaded-provider')
  if name.startswith('libgimp'):require(p.is_relative_to(Path(r['build_root'])),'foreign-project-library')
  require(p.is_file(),'missing-loaded-provider');libs[name]={'path':str(p),'sha256':sha(p)}
 require(libs and any(x.startswith('libgimpwidgets-3.0.so') for x in libs) and any(x.startswith('libgimpbase-3.0.so') for x in libs),'missing-project-runtime-providers')
 if r['mode']=='sanitizer':require(all(any(name.startswith(prefix) for name in libs) for prefix in ('libasan.so','libubsan.so')),'missing-sanitizer-runtime')
 else:require(not any(name.startswith(('libasan.so','libubsan.so','liblsan.so','libtsan.so')) for name in libs),'normal-sanitizer-runtime')
 return libs,digest(json.dumps({k:raw.get(k) for k in LOADER_VARS},sort_keys=True).encode())
def aggregate(receipt,candidate_log,old_log=None,full_log=None):
 r=read_json(receipt);validate_receipt(r);verify_files(r)
 normal=r['mode']=='normal';require(bool(old_log)==normal and bool(full_log)==normal,'wrong-mode-log-set')
 inputs=[('fixed-focused',candidate_log,FIXED,False,None)]
 if normal:inputs.extend((('old-focused',old_log,OLD,True,None),('fixed-full107',full_log,FIXED,False,r['full_cases'])))
 reports={};closure=None;envhash=None
 for kind,path,target,old,full in inputs:
  records,h=rows(path);row=check_row(records,r['binaries'][target],old,full);runtime_settings(row,r);libs,eh=replay(r,row,target)
  require(libs==r['pre_run_libraries'][target],'pre-post-runtime-provider-mismatch')
  if closure is None:closure=libs;envhash=eh
  else:require(libs==closure and eh==envhash,'paired-runtime-provider-or-loader-settings-mismatch')
  reports[kind]={'raw_log_sha256':h,'binary_sha256':sha(r['binaries'][target]),'case_count':107 if full else 1,'returncode':row['returncode'],'result':row['result']}
 verify_files(r)
 for item in closure.values():require(sha(item['path'])==item['sha256'],'runtime-provider-changed')
 for kind,path,*_ in inputs:require(sha(path)==reports[kind]['raw_log_sha256'],'raw-log-changed')
 out=base('aggregate');out.update(proof_passed=True,mode=r['mode'],provider_receipt_sha256=sha(receipt),reports=reports,
  expected_failure_semantics_verified=True,expected_failure_semantics_scope='exact native Meson result and returncode for every included target; normal mode requires old EXPECTEDFAIL SIGTRAP and fixed OK',
  runtime_binding_complete=True,runtime_binding_scope='ELF resolution replay under actual Meson env/cwd, not live module observation',
  loader_environment_sha256=envhash,loaded_libraries={name:item['sha256'] for name,item in closure.items()},
  fixed_full107_passed=normal,existing_common107_gate_not_replaced=True)
 return out

def native_failure_detail(stdout, stderr, source):
 # Diagnostic only: publish known source identities, never runtime prose/values.
 test=read_bytes(source/TEST,MAX_LOG).decode()
 cases=full_cases(test,read_json(source/MANIFEST)); allowed=set(cases)
 files={TEST:test}
 for name in re.findall(r'^#include "(test-painter-[A-Za-z0-9_-]+\.inc)"',test,re.M):
  relative='app/tests/'+name
  files[relative]=read_bytes(source/relative,MAX_LOG).decode()
 text=stdout+'\n'+stderr
 passed=[];failed=[];unknown=0
 for state,case in re.findall(r'^(ok|not ok) (?:[0-9]{1,3} )?(\S+)(?: .*)?$',stdout,re.M):
  if case not in allowed: unknown+=1;continue
  (passed if state=='ok' else failed).append(case)
 locations=[];unmatched=0
 # GLib ERROR source prefixes. Ignore arbitrary functions and assertion values.
 for name,number in re.findall(r'(?m)^.*?ERROR:[^\n]*?/(test-painter-[A-Za-z0-9_-]+\.(?:c|inc)):([0-9]{1,6}):',text):
  relative='app/tests/'+name;line=int(number);lines=files.get(relative,'').splitlines()
  if not (1<=line<=len(lines)): unmatched+=1;continue
  match=re.search(r'\b(g_assert(?:_[a-z_]+)?|g_test_trap_assert_[a-z_]+|g_test_assert_expected_messages)\s*\(',lines[line-1])
  if not match: unmatched+=1;continue
  item={'source':relative,'line':line,'assertion_kind':match.group(1)}
  if item not in locations and len(locations)<16:locations.append(item)
 return {'schema':'canvas-mask-native-detail-v1','diagnostic_only':True,
   'allowlist_sha256':digest('\n'.join(cases).encode()),
   'passed_leaf_count':min(len(passed),108),'last_passed_case':passed[-1] if passed else None,
   'failed_cases':list(dict.fromkeys(failed))[:107],'unknown_leaf_count':min(unknown,108),
   'source_assertions':locations,'unmatched_assertion_locations':min(unmatched,108),
   'bailout_seen':bool(re.search(r'^Bail out!',stdout,re.M)),
   'subprocess_failure_seen':bool(re.search(r'child process .*failed unexpectedly|child process .*killed by signal',text)),
   'stdout_sha256':digest(stdout.encode()),'stderr_sha256':digest(stderr.encode()),
   'raw_output_exported':False}


def observe(log,build,mode,target,selection,shell_exit):
 require(mode in ('normal','sanitizer') and target in (FIXED,OLD) and selection in ('focused','full107'),'invalid-observation-selector')
 require(type(shell_exit) is int and 0<=shell_exit<=255,'invalid-shell-exit')
 require(not (target==OLD and (mode!='normal' or selection!='focused')) and not (selection=='full107' and mode!='normal'),'invalid-observation-combination')
 build=Path(build).resolve();label='old-focused' if target==OLD else 'fixed-'+selection
 expected=build/'meson-logs'/('canvas-mask-'+mode+'-'+label+'.json')
 require(Path(log).absolute()==expected,'wrong-observation-log')
 out=base('run-observation');out.update(observational_only=True,observation_complete=False,mode=mode,target=target,selection=selection,
   meson_shell_exit=shell_exit,native_returncode=None,meson_result='unknown',raw_log_sha256=None,categories=[])
 try:
  data=read_bytes(expected,MAX_LOG);out['raw_log_sha256']=digest(data)
  records=[json.loads(x,object_pairs_hook=unique) for x in data.decode().splitlines() if x.strip()]
  require(len(records)==1 and isinstance(records[0],dict),'invalid-observation-registration')
  row=records[0];result=row.get('result');code=row.get('returncode')
  allowed=('OK','FAIL','EXPECTEDFAIL','UNEXPECTEDPASS','SKIP','TIMEOUT','INTERRUPT','ERROR')
  out['meson_result']=result if isinstance(result,str) and result in allowed else 'unknown'
  require(type(code) is int and -255<=code<=255,'invalid-observation-native-exit');out['native_returncode']=code
  stdout,stderr=row.get('stdout'),row.get('stderr','');require(isinstance(stdout,str) and isinstance(stderr,str),'invalid-observation-output')
  text=stdout+'\n'+stderr;categories=[]
  for category,pattern in (('address-sanitizer',r'AddressSanitizer'),('leak-sanitizer',r'LeakSanitizer'),
                           ('undefined-behavior',r'UndefinedBehaviorSanitizer|runtime error:'),
                           ('critical-or-assertion',r'CRITICAL|assertion'),('warning',r'WARNING'),
                           ('error',r'ERROR:'),('tap-failure',r'^not ok|^Bail out!'),('skip-or-todo',r'#\s*(?:SKIP|TODO)\b')):
   if re.search(pattern,text,re.I|re.M):categories.append(category)
  if code!=0:categories.append('nonzero-native-exit')
  if result=='TIMEOUT':categories.append('meson-timeout')
  if result=='SKIP' or code==77:categories.append('meson-skip')
  if row.get('command')!=[str(build/SUBDIR/target)]+([] if selection=='full107' else ['-p',CASE]):categories.append('unexpected-command')
  if str(row.get('name','')).split(' / ')[-1]!=target:categories.append('unexpected-registration')
  out['native_detail']=native_failure_detail(stdout,stderr,Path(__file__).resolve().parents[2])
  out.update(observation_complete=True,categories=categories)
 except (OSError,ValueError,TypeError,UnicodeError,KeyError):
  out['errors']=['missing-unsafe-or-malformed-private-log']
 return out


def main():
 p=argparse.ArgumentParser(description=__doc__);m=p.add_mutually_exclusive_group(required=True);m.add_argument('--provider',action='store_true');m.add_argument('--aggregate',action='store_true');m.add_argument('--observe-log',type=Path)
 p.add_argument('--target',choices=(OLD,FIXED));p.add_argument('--selection',choices=('focused','full107'));p.add_argument('--shell-exit',type=int)
 p.add_argument('--source',type=Path);p.add_argument('--build',type=Path);p.add_argument('--mode',choices=('normal','sanitizer'))
 p.add_argument('--provider-receipt',type=Path);p.add_argument('--candidate-log',type=Path);p.add_argument('--old-log',type=Path);p.add_argument('--full-log',type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 try:
  if a.observe_log:
   require(a.build and a.mode and a.target and a.selection and a.shell_exit is not None and not any((a.source,a.provider_receipt,a.candidate_log,a.old_log,a.full_log)),'invalid-observation-cli');r=observe(a.observe_log,a.build,a.mode,a.target,a.selection,a.shell_exit)
  elif a.provider:
   require(a.source and a.build and a.mode and not any((a.provider_receipt,a.candidate_log,a.old_log,a.full_log,a.target,a.selection,a.shell_exit is not None)),'invalid-provider-cli');r=provider(a.source,a.build,a.mode)
  else:
   require(a.provider_receipt and a.candidate_log and not any((a.source,a.build,a.mode,a.target,a.selection,a.shell_exit is not None)),'invalid-aggregate-cli');r=aggregate(a.provider_receipt,a.candidate_log,a.old_log,a.full_log)
 except ProviderWitnessError as exc:
  code=str(exc);r=base('error');r['errors']=[code if re.fullmatch('[a-z][a-z0-9-]{0,95}',code) else 'provider-or-runtime-proof-failed'];r['provider_witness']=exc.witness
  if exc.inspection is not None:r['inspection_diagnostic']=exc.inspection
 except InspectionError as exc:
  r=base('error');r['errors']=['inspection-failed'];r['inspection_diagnostic']=exc.diagnostic
 except ValueError as exc:
  code=str(exc);r=base('error');r['errors']=[code if re.fullmatch('[a-z][a-z0-9-]{0,95}',code) else 'invalid-serialized-evidence']
 except (OSError,TypeError,KeyError,AttributeError,subprocess.SubprocessError):
  r=base('error');r['errors']=['provider-or-runtime-proof-failed']
 a.output.write_text(json.dumps(r,indent=2,sort_keys=True)+'\n');return 0 if (r.get('observation_complete') if a.observe_log else r['proof_passed']) else 1
if __name__=='__main__':raise SystemExit(main())
