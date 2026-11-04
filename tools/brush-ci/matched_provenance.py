#!/usr/bin/env python3
"""C17 private matched-cell provenance. No acceptance authority; fail closed.
Inspection subprocesses are for the hosted diagnostic only, not local native tests.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import stat

COMMITS = {
 'baseline': ('e25a94e21efca6e6604ddac6f15570c049f421e7','8634eeef49c81aaa5c25192cb76d749ff680683f'),
 'current': ('b671825309326bdb55571652e43ab78cc83fd92d','4469df41e161e3f855918125932e377757a1c937')}
CELLS = tuple(a+'-'+b for a in COMMITS for b in ('normal','sanitizer'))
SUBMODULE = 'a31e5dea4f572fb5f33b06f5c4b86db5f30643dc'
OVERLAY = {
 'test-matched-preclick.c':'b363b43f8eb9d1eae6e37741c9690e8065eb40409e72b2c1eed84b0916ffdbd6',
 'matched-preclick-phases.inc':'f2fcebbacdca5da6111c1e17599f6dbb2ee3e3e2e2f84cda14b45dc0aedf2665',
 'meson.append':'668d95cb2700e567b1b9bdc39f4fb730242254af90a3e0b7abfc6cd47b1e9d65'}
CONFIGURATION_POLICY = {
 'file':'baseline-meson-compatibility.json',
 'sha256':'97a542bfdf237f7680b6e865ca728e4263eb244b729a8b2b0751f1f4d028c765',
 'baseline_before':'6cbbc986cae81fcbcd3234e70eac91d96ffa2a254ad8c1b0e56f64f19325959f',
 'baseline_after':'83d371852ebeeede92caee7e0280f35dfc28a2a05cc8af6d00a84b67177d5b0f',
 'current_unchanged':'74358058b211605bcd3fcb0e08df60722728196b93102799bc27ec0500329ea5'}

def configuration_overlay(arm):
 baseline = arm == 'baseline'
 return {'policy_sha256':CONFIGURATION_POLICY['sha256'], 'path':'meson.build',
         'applied':baseline, 'scope':'diagnostic-build-configuration-only',
         'before_sha256':CONFIGURATION_POLICY['baseline_before' if baseline else 'current_unchanged'],
         'after_sha256':CONFIGURATION_POLICY['baseline_after' if baseline else 'current_unchanged']}

COMMON = {
 'tools/prepare-linux-build-deps.py':'0884591ce6ee387e5dc7ee035033168d99ff304043a32d19cc8011cb641db750',
 'tools/linux-debian13-env.sh':'503c6ddb42e43b58ec617b0e91c433f7b84657b38b88cca12d570a05067bcc76',
 'tools/run-painter-frozen-aggregate.py':'4cdc8a36be383487c4cca06fc173f8d3c2b949370318b66fd15f22548eecfada',
 'app/tests/tests.c':'56303878e316e41fcb7c439b199cde5bd9fc4ac84b46b48efcbfbc79737747e7',
 'app/tests/tests.h':'5541a82208a639234d6d91b0f70d8dacefe6ebd59a3082f84487409db55de1fe',
 'app/tests/gimp-app-test-utils.c':'2d58f6f426f782cc2fca7207924d7d18f903b8b9519f1ef7b0c69b16c815929c',
 'app/tests/gimp-test-session-utils.c':'e6ac7958e5c33195f499b7a8e4a8709a3c6ec51827d0dabb9467333594af4863',
 'app/tests/gimpdir/sessionrc-native-oncanvas':'75f60bbb224293a5399f4d7bc043461ec1004bc8abbc3968f315bf40650b8e56',
 'migration/baseline/debian13-package-lock.json':'553f4051b91626bc2f5470373fd3e8d67c8d61ec62103b2addcab7a16492129f'}
MESON_SHA = 'c65567c7fa8f3e75dbcf24ab5986e8c697dc3fe1d52bf88384ca58817423849e'
PLUGINS = ('blinds','tile-small','contrast-retinex','convolution-matrix')
ALIASES = ('gimp30-gmo','gimp30-libgimp-gmo','gimp30-std-plug-ins-gmo','gimp30-python-gmo','gimp30-script-fu-gmo','org.gimp.extension.goat-exercises-gmo')
TARGETS = ('app/tests/matched-preclick', 'menus/image-menu.ui','menus/dockable-menu.ui') + tuple('plug-ins/common/'+x for x in PLUGINS) + ALIASES
WRAPPER_SHA='992b0337655c156cc7a78f0de1575ecbade0af180be656918f132bc9708d2815'
ORDER=('baseline-normal','current-normal','baseline-sanitizer','current-sanitizer','current-sanitizer','baseline-sanitizer','current-normal','baseline-normal','baseline-normal','current-normal','baseline-sanitizer','current-sanitizer','current-normal','current-sanitizer')
MAX_BYTES = 256*1024*1024
class EvidenceError(ValueError): pass

def bounded_object_path(value):
 # Only already-contained source/build-relative paths, never absolute paths.
 if not isinstance(value,str) or len(value)>240 or not re.fullmatch(r'[A-Za-z0-9_.+/-]+',value): return 'unavailable'
 if value.startswith('/') or any(x in ('','.', '..') for x in value.split('/')): return 'unavailable'
 return value

def sanitizer_argument_labels(args):
 labels=[]
 forms={'address':'address-only','undefined':'undefined-only',
        'address,undefined':'address-undefined','undefined,address':'undefined-address',
        'all':'all','vptr':'vptr','none':'none'}
 for arg in args:
  for prefix,category in (('-fsanitize=','enable'),('-fno-sanitize=','disable'),
                          ('-fno-sanitize-recover=','disable-recovery'),('-fsanitize-recover=','enable-recovery'),
                          ('-fsanitize-trap=','enable-trap'),('-fno-sanitize-trap=','disable-trap')):
   if arg.startswith(prefix):
    labels.append(category+'-'+forms.get(arg[len(prefix):],'other')); break
 return {'labels':labels[:16], 'truncated':len(labels)>16}

class ObjectSanitizerError(EvidenceError):
 def __init__(self,args,recipe,cell,obj,source,row_index,row_count):
  super().__init__('object-sanitizer-mismatch')
  self.diagnostic={'schema':'matched-object-sanitizer-mismatch-v1',
   'cell':cell if cell in CELLS else 'unavailable',
   'object':bounded_object_path(obj),'source':bounded_object_path(source),
   'recipe_sha256':hashlib.sha256(recipe.encode()).hexdigest(),
   'compile_row_index':row_index if type(row_index) is int and 0<=row_index<10**7 else None,
   'compile_row_count':row_count if type(row_count) is int and 0<=row_count<=10**7 else None,
   'sanitizer_arguments':sanitizer_argument_labels(args),
   'target_metadata':'not-bound-to-object',
   'scope':'recipe-observation-not-instrumentation-proof'}

def validate_object_sanitizer(args,recipe,cell,obj,source,row_index,row_count):
 # Keep the original predicate verbatim; this patch adds evidence only.
 valid=(('-fsanitize=address,undefined' in recipe or '-fsanitize=undefined,address' in recipe) if cell.endswith('sanitizer') else '-fsanitize=' not in recipe)
 if not valid: raise ObjectSanitizerError(args,recipe,cell,obj,source,row_index,row_count)

def need(ok, code):
 if not ok: raise EvidenceError(code)
# Only fixed stage/category/code labels leave the private runner. Exception
# messages, paths, subprocess output, and environment values are never emitted.
RUNTIME_STAGES = ('source-identity','raw-log','build-identity','postflight-cache',
                  'preflight','actual-environment','wrapper-command','sanitizer-options',
                  'schema-search','font-list','font-config','catalog-aliases',
                  'resource-inventory','loader-replay','final-recheck')
RESOURCE_FAILURE_CODES = ('catalog-alias-unproven', 'catalog-content-mismatch', 'catalog-extension-unproven', 'catalog-helper-mismatch', 'catalog-inventory-mismatch', 'catalog-metadata-invalid', 'catalog-metadata-mismatch', 'catalog-source-mismatch', 'catalog-target-unproven', 'data-cell-prefix-provider-unapproved', 'data-directory-cycle', 'data-directory-depth-limit', 'data-provider-invalid', 'data-provider-unapproved', 'data-search-path-unproven', 'font-resolution-inventory-invalid', 'font-schema-evidence-invalid', 'font-schema-evidence-missing', 'font-schema-path-invalid', 'font-schema-path-unapproved', 'fontconfig-inventory-invalid', 'plugin-cache-archive-binding-mismatch', 'plugin-cache-freshness-unproven', 'plugin-cache-invalid', 'plugin-cache-limit', 'plugin-executable-inventory', 'plugin-registration-duplicate', 'plugin-registration-invalid', 'plugin-registration-inventory', 'plugin-registration-provider', 'resource-directory-missing', 'resource-evidence-invalid', 'resource-fallback-override', 'resource-file-invalid', 'resource-inventory-empty', 'resource-inventory-limit', 'resource-json-duplicate', 'resource-json-invalid', 'resource-path-escape', 'resource-read-failed', 'resource-relative-name-invalid', 'resource-runtime-binding-mismatch', 'resource-session-mismatch', 'resource-special-file', 'resource-symlink-invalid', 'schema-compiled-missing', 'schema-search-path-unproven')
class RuntimeInspectionError(EvidenceError):
 def __init__(self,error,stage):
  super().__init__(str(error) if isinstance(error,EvidenceError) else 'inspection-unavailable')
  category=('evidence-rejected' if isinstance(error,EvidenceError) else
            'os-error' if isinstance(error,OSError) else
            'subprocess-error' if isinstance(error,subprocess.SubprocessError) else
            'key-error' if isinstance(error,KeyError) else
            'type-error' if isinstance(error,TypeError) else
            'stop-iteration' if isinstance(error,StopIteration) else 'value-error')
  self.diagnostic={'stage':stage,'category':category,'details_exported':False}
  # Import only our bundled class. Even a forged message of arbitrary length
  # cannot escape: membership is against this finite reviewed literal set.
  from matched_resources import ResourceError
  if isinstance(error,ResourceError) and str(error) in RESOURCE_FAILURE_CODES:
   self.diagnostic['resource_code']=str(error)
   if getattr(error,'substage',None)=='catalog-alias-inputs':
    self.diagnostic['resource_substage']='catalog-alias-inputs'

@contextmanager
def runtime_stage(stage):
 if stage not in RUNTIME_STAGES:raise EvidenceError('unknown-runtime-stage')
 try:yield
 except (EvidenceError,OSError,ValueError,KeyError,TypeError,StopIteration,subprocess.SubprocessError) as error:
  raise RuntimeInspectionError(error,stage) from None

def digest(data): return hashlib.sha256(data).hexdigest()
def sha(path):
 p=Path(path); need(p.is_file(), 'missing-file')
 with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def encoded(value): return json.dumps(value,sort_keys=True,separators=(',',':')).encode()
def hashed(value): return digest(encoded(value))
def pairs(items):
 d={}
 for k,v in items: need(k not in d,'duplicate-json-key'); d[k]=v
 return d
def read(path):
 p=Path(path); need(p.stat().st_size<=MAX_BYTES,'oversized-json')
 return json.loads(p.read_text(),object_pairs_hook=pairs)
def run(args,cwd=None,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90,check=False)
 need(p.returncode==0,'inspection-command-failed'); need(len(p.stdout)<=MAX_BYTES,'oversized-inspection')
 return p.stdout.decode('utf-8',errors='strict')
def git(src,*args): return run(['git','-C',str(src),*args])
def base(kind): return {'schema':'matched-preclick-'+kind+'-v1','acceptance_eligible':False,'errors':[],'verified':False,'producer_sha256':sha(__file__)}
def inside(path,root): return Path(path).resolve().is_relative_to(Path(root).resolve())
def regular(path): need(not Path(path).is_symlink() and Path(path).is_file(),'nonregular-input'); return sha(path)
def source_identity(source, inputs=None):
 s=Path(source).resolve(); commit=git(s,'rev-parse','HEAD').strip(); tree=git(s,'rev-parse','HEAD^{tree}').strip()
 arm=next((a for a,v in COMMITS.items() if v==(commit,tree)),None); need(arm is not None,'unreviewed-source')
 need(git(s,'diff','--cached','--name-only')=='','staged-source-change')
 changed=git(s,'diff','--name-only').splitlines()
 need(changed==(['app/tests/meson.build','meson.build'] if arm=='baseline' else ['app/tests/meson.build']),'unexpected-tracked-change')
 compatibility=configuration_overlay(arm)
 need(digest(git(s,'show','HEAD:meson.build').encode())==compatibility['before_sha256'],'configuration-base-mismatch')
 need(regular(s/'meson.build')==compatibility['after_sha256'],'configuration-overlay-mismatch')
 if inputs is not None: need(regular(Path(inputs)/CONFIGURATION_POLICY['file'])==CONFIGURATION_POLICY['sha256'],'configuration-policy-mismatch')
 untracked=git(s,'ls-files','--others','--exclude-standard').splitlines()
 need(set(untracked)=={'app/tests/'+x for x in OVERLAY if x!='meson.append'},'unexpected-untracked-source')
 # Ignored files also matter: fresh sources may not smuggle alternate headers.
 need(not git(s,'ls-files','--others','--ignored','--exclude-standard').strip(),'ignored-source-input')
 old=git(s,'show','HEAD:app/tests/meson.build').encode(); need(digest(old)==MESON_SHA,'protected-meson-mismatch')
 current=(s/'app/tests/meson.build').read_bytes(); need(current.startswith(old),'protected-meson-edited')
 append=current[len(old):]; need(digest(append)==OVERLAY['meson.append'],'meson-append-mismatch')
 for p,h in COMMON.items(): need(regular(s/p)==h,'common-input-mismatch')
 for name,h in OVERLAY.items():
  if name!='meson.append': need(regular(s/'app/tests'/name)==h,'observer-mismatch')
  if inputs is not None: need(regular(Path(inputs)/name)==h,'overlay-input-mismatch')
 need(git(s/'gimp-data','rev-parse','HEAD').strip()==SUBMODULE,'submodule-commit-mismatch')
 need(not git(s/'gimp-data','status','--porcelain','--untracked-files=all').strip(),'submodule-dirty')
 need(git(s,'ls-tree','HEAD','gimp-data').split()[2]==SUBMODULE,'submodule-gitlink-mismatch')
 return {'source_commit':commit,'source_tree':tree,'arm':arm,'submodule_commit':SUBMODULE,'overlay':OVERLAY,'configuration_overlay':compatibility,'common_inputs':COMMON,'source_root_identity':digest(str(s).encode())}

def normalize(text, roots):
 # Never export command text: normalized command hashes are sufficient. Reject
 # unknown private roots (including another cell), rather than redacting proof.
 for label,path in sorted(roots.items(),key=lambda x:-len(str(x[1]))): text=text.replace(str(Path(path).resolve()),'@'+label+'@')
 need(not re.search(r'/(?:workspace|home|tmp|private|opt|mnt)/',text),'foreign-private-path')
 return text

def validate_compile_paths(args,cwd,roots):
 allowed=list(roots.values())+[Path('/usr'),Path('/lib'),Path('/lib64'),Path('/bin')]
 for n,arg in enumerate(args):
  value=None
  if arg in ('-I','-isystem','-iquote','-L'):
   need(n+1<len(args),'missing-path-option'); value=args[n+1]
  elif arg.startswith(('-I','-L')) and len(arg)>2: value=arg[2:]
  elif arg.startswith('-isystem') and len(arg)>8: value=arg[8:]
  elif arg.startswith('-iquote') and len(arg)>7: value=arg[7:]
  if value is not None:
   path=resolve(value,cwd); need(any(inside(path,r) for r in allowed),'foreign-compile-search-path')

def resolve(p,cwd): return (Path(p) if Path(p).is_absolute() else Path(cwd)/p).resolve()
def no_loader_injection(env):
 need(not any(v for k,v in env.items() if k.startswith('LD_') and k not in ('LD_LIBRARY_PATH','LD_BIND_NOW')),'unsupported-loader-environment')
 need('LSAN_OPTIONS' not in env,'lsan-override')

def build_environment(roots,cwd):
 keys=('PATH','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','COMPILER_PATH','GCC_EXEC_PREFIX','PKG_CONFIG_PATH','PKG_CONFIG_LIBDIR','CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS')
 witness={}
 for key in keys:
  value=os.environ.get(key)
  if value is not None:
   if key in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','COMPILER_PATH','PKG_CONFIG_PATH','PKG_CONFIG_LIBDIR'):
    for entry in value.split(':'): validate_compile_paths(['cc','-I',entry or '.'],cwd,roots)
   for label,path in sorted(roots.items(),key=lambda x:-len(str(x[1]))): value=value.replace(str(path),'@'+label+'@')
  witness[key]=value
 return hashed(witness)

def options(build,roots,cell):
 raw=read(build/'meson-info/intro-buildoptions.json'); opts={x['name']:x['value'] for x in raw}; need(len(opts)==len(raw),'duplicate-meson-option')
 need(opts.get('buildtype')=='debugoptimized','buildtype-mismatch')
 for key in ('libunwind','libbacktrace','enable-console-bin','print'): need(opts.get(key) is False,'meson-option-mismatch')
 for key in ('auto_features','headless-tests','painter-http','gi-docgen','appdata-test','javascript','vala'): need(opts.get(key)=='disabled','meson-option-mismatch')
 need(opts.get('check-update')=='no','meson-option-mismatch')
 san=opts.get('b_sanitize'); san=','.join(san) if isinstance(san,list) else san
 need(san in (('address,undefined','undefined,address') if cell.endswith('sanitizer') else ('none','')),'sanitizer-option-mismatch')
 normalized={k:json.loads(normalize(json.dumps(v),roots)) for k,v in opts.items()}; normalized['b_sanitize']='MODE'
 selected={k:normalized[k] for k in ('buildtype','auto_features','libunwind','libbacktrace','headless-tests','painter-http','enable-console-bin','gi-docgen','appdata-test','javascript','vala','print','check-update','b_sanitize')}
 return {'selected':selected,'all_options_normalized_sha256':hashed(normalized)}

def target_paths(targets,build,source):
 found={}
 for t in targets:
  for filename in t.get('filename',[]):
   p=resolve(filename,build)
   if inside(p,build): found[p.relative_to(build).as_posix()]=t
 for name in TARGETS:
  if name in ALIASES:
   query=run(['ninja','-C',str(build),'-t','query',name],cwd=build)
   need(query.startswith(name+':\n') and 'input:' in query,'missing-catalog-alias')
  else:
   need(name in found,'missing-required-target')
   need(found[name].get('type')==('custom' if name.startswith('menus/') else 'executable'),'wrong-target-type')
   need((build/name).is_file(),'unbuilt-required-target')
 return found

GENERATOR = {
 'object':'gimp-data/tools/colorsvg2png.p/colorsvg2png.c.o',
 'source':'gimp-data/tools/colorsvg2png.c',
 'declaration':'gimp-data/tools/meson.build',
 'executable':'gimp-data/tools/colorsvg2png',
 'source_sha256':'216cf0dd5c5636b9d8c598f78d629fa9c446d5022406951c8290218dd524fcd7',
 'declaration_sha256':'077055d4bc9b6399b180a1f9e888bfa9d73879bf79341fae20a928ad9caa9109',
 'recipe_sha256':'d11a29bff05e0ff2d04194e4d50462ad5c5471aca6687ae72f8b78eea8242227'}
GENERATOR_RESOURCES=('gimp-data/icons/Default/64/dialog-question.png',
 'gimp-data/icons/Default/64/gimp-wilber-eek.png',
 'gimp-data/icons/Default/gimp-core-pixbufs.c',
 'gimp-data/icons/Default/gimp-icon-pixbufs.c')

def query_outputs(text,target):
 lines=text.splitlines()
 need(lines and lines[0]==target+':' and lines.count('  outputs:')==1,'generator-query-shape')
 n=lines.index('  outputs:'); need(n>1 and lines[1].startswith('  input: '),'generator-query-shape')
 out=lines[n+1:];need(all(x.startswith('    ') and x.strip() for x in out),'generator-query-shape')
 return [x[4:] for x in out]

def query_inputs(text,target):
 query_outputs(text,target)
 lines=text.splitlines(); rows=lines[2:lines.index('  outputs:')]
 need(all(x.startswith('    ') and x.strip() for x in rows),'generator-query-shape')
 values=[x[4:] for x in rows]
 return [{'kind':'order-only' if x.startswith('|| ') else 'implicit' if x.startswith('| ') else 'explicit',
          'path':x[3:] if x.startswith('|| ') else x[2:] if x.startswith('| ') else x} for x in values]

class GeneratorSubmoduleError(EvidenceError):
 def __init__(self,output):
  super().__init__('generator-submodule-mismatch')
  normalized=output.removesuffix('\n')
  self.diagnostic={'schema':'matched-generator-submodule-mismatch-v1',
   'expected_commit':SUBMODULE,
   'observed_commit':normalized if re.fullmatch('[0-9a-f]{40}',normalized) else None,
   'output_shape':'one-lf-hash' if re.fullmatch('[0-9a-f]{40}\n',output) else ('bare-hash' if re.fullmatch('[0-9a-f]{40}',output) else 'other'),
   'output_sha256':digest(output.encode()),'output_bytes_capped':min(len(output.encode()),4096),
   'scope':'git-output-observation-not-source-content-proof'}

def validate_generator_submodule(output):
 # Git rev-parse emits one LF; remove only that delimiter, not arbitrary whitespace.
 if output.removesuffix('\n')!=SUBMODULE:raise GeneratorSubmoduleError(output)

def generator_role(s,b,targets,roots):
 # This is the sole reviewed upstream b_sanitize=none build-tool role.
 need(isinstance(targets,list) and all(isinstance(t,dict) for t in targets),'generator-target-mismatch')
 for target in targets:
  need(isinstance(target.get('filename',[]),list) and all(isinstance(x,str) for x in target.get('filename',[])),'generator-target-mismatch')
  need(isinstance(target.get('target_sources',[]),list) and all(isinstance(x,dict) for x in target.get('target_sources',[])),'generator-target-mismatch')
 validate_generator_submodule(git(s/'gimp-data','rev-parse','HEAD'))
 for key in ('source','declaration'):
  need(regular(s/GENERATOR[key])==GENERATOR[key+'_sha256'],'generator-source-mismatch')
 exe=b/GENERATOR['executable'];obj=b/GENERATOR['object'];src=s/GENERATOR['source']
 matches=[t for t in targets if t.get('filename')==[str(exe)]]
 need(len(matches)==1,'generator-target-ambiguous');t=matches[0]
 need(t.get('name')=='colorsvg2png' and t.get('type')=='executable' and t.get('installed') is False and
      t.get('defined_in')==str(s/GENERATOR['declaration']),'generator-target-mismatch')
 blocks=t.get('target_sources');need(isinstance(blocks,list),'generator-target-mismatch')
 c=[x for x in blocks if x.get('language')=='c'];links=[x for x in blocks if 'linker' in x]
 need(len(c)==1 and len(links)==1 and len(blocks)==2,'generator-target-mismatch')
 need(c[0].get('machine')=='build' and c[0].get('sources')==[str(src)] and
      not c[0].get('generated_sources') and not c[0].get('unity_sources'),'generator-machine-source-mismatch')
 for block in blocks:
  params=block.get('parameters');need(isinstance(params,list) and all(isinstance(a,str) for a in params),'generator-parameters-invalid')
  need(not any(a.startswith('@') or a.startswith(('-fsanitize','-fno-sanitize')) for a in params),'generator-parameters-invalid')
 object_query=run(['ninja','-C',str(b),'-t','query',GENERATOR['object']],cwd=b)
 need(query_outputs(object_query,GENERATOR['object'])==[GENERATOR['executable']],'generator-object-owner-mismatch')
 # Only custom PNG outputs and Ninja's all phony node may depend on the tool.
 custom_outputs={str(resolve(f,b).relative_to(b)) for x in targets if x.get('type')=='custom'
                 for f in x.get('filename',[]) if inside(resolve(f,b),b) and f.endswith('.png')}
 exe_query=run(['ninja','-C',str(b),'-t','query',GENERATOR['executable']],cwd=b)
 consumers=query_outputs(exe_query,GENERATOR['executable'])
 need(set(GENERATOR_RESOURCES[:2])<=set(consumers) and all(x=='all' or x in custom_outputs for x in consumers),'generator-host-consumer')
 resource_queries={}
 for png,csource in zip(GENERATOR_RESOURCES[:2],GENERATOR_RESOURCES[2:]):
  owners=[x for x in targets if str(b/csource) in x.get('filename',[])]
  need(len(owners)==1 and owners[0].get('type')=='custom','generator-resource-target-mismatch')
  query=run(['ninja','-C',str(b),'-t','query',csource],cwd=b)
  edges=[x for x in query_inputs(query,csource) if x['path']==png]
  need(len(edges)==1,'generator-resource-edge-missing')
  resource_queries[csource]={'query_sha256':digest(normalize(query,roots).encode()),'png_edge_kind':edges[0]['kind']}
 for other in targets:
  if other is t:continue
  for block in other.get('target_sources',[]):
   if 'linker' not in block:continue
   args=block.get('parameters',[])
   need(isinstance(args,list),'opaque-link-input')
   need(all(isinstance(a,str) and not a.startswith('@') for a in args),'opaque-link-input')
   need(not any('colorsvg2png' in a for a in args),'generator-host-link')
 commands=run(['ninja','-C',str(b),'-t','commands',GENERATOR['executable']],cwd=b)
 parsed=[shlex.split(line) for line in commands.splitlines() if line.strip()]
 need(len(parsed)==2 and all(not any(a.startswith('@') or any(c in a for c in (';','&','|','`','<','>','\n','\r','$(')) for a in args) for args in parsed),'generator-link-shape')
 compile_args=[args for args in parsed if '-c' in args]
 need(len(compile_args)==1 and digest(normalize('\0'.join(compile_args[0]),roots).encode())==GENERATOR['recipe_sha256'],'generator-command-recipe-mismatch')
 ca=compile_args[0]
 need(ca.count('-c')==1 and ca.count('-o')==1 and resolve(ca[ca.index('-c')+1],b)==src and resolve(ca[ca.index('-o')+1],b)==obj,'generator-compile-binding')
 link=[args for args in parsed if '-c' not in args]
 need(len(link)==1 and link[0].count('-o')==1 and link[0][0]==ca[0],'generator-link-shape');args=link[0]
 need(resolve(args[args.index('-o')+1],b)==exe,'generator-link-output')
 need([resolve(a,b) for a in args if a.endswith('.o')]==[obj],'generator-link-object')
 need(not any(a.startswith(('-fsanitize','-fno-sanitize')) for a in args),'generator-link-sanitizer')
 return {'role':'pinned-native-build-generator','policy':GENERATOR,
  'target_machine':'build','installed':False,'executable_sha256':regular(exe),
  'link_ownership_scope':'declared-ninja-graph-and-command-not-live-execution',
  'link_recipe_sha256':digest(normalize('\0'.join(args),roots).encode()),
  'object_query_sha256':digest(normalize(object_query,roots).encode()),
  'executable_query_sha256':digest(normalize(exe_query,roots).encode()),
  'resource_graph_sha256':resource_queries,
  'generated_resources':{p:regular(b/p) for p in GENERATOR_RESOURCES}}

def build_identity(source,build,cell,deps):
 need(cell in CELLS,'unknown-cell'); s=Path(source).resolve(); b=Path(build).resolve(); d=Path(deps).resolve()
 need(len({s,b,d})==3 and not inside(b,s) and not inside(s,b),'nonisolated-build')
 r=base('build-identity'); r.update(source_identity(s)); need(cell.startswith(r['arm']+'-'),'cell-source-mismatch'); r['cell']=cell
 optrows=read(b/'meson-info/intro-buildoptions.json'); prefix=next(Path(x['value']).resolve() for x in optrows if x['name']=='prefix')
 roots={'source':s,'build':b,'deps':d,'prefix':prefix}
 r['options']=options(b,roots,cell); r['build_environment_sha256']=build_environment(roots,b)
 targets=read(b/'meson-info/intro-targets.json'); target_paths(targets,b,s)
 r['generator_role']=generator_role(s,b,targets,roots)
 r['resources_helper_sha256']=sha(Path(__file__).with_name('matched_resources.py'))
 r['metadata']={name:sha(b/'meson-info'/name) for name in ('intro-buildoptions.json','intro-targets.json','intro-tests.json')}
 r['normalized_targets_sha256']=digest(normalize(json.dumps(targets,sort_keys=True),roots).encode())
 tests=read(b/'meson-info/intro-tests.json'); match=[x for x in tests if x['name']=='matched-preclick']; need(len(match)==1,'missing-observer-registration')
 need(match[0]['cmd']==[str(b/'app/tests/matched-preclick')] and match[0].get('workdir') in (None,str(b)),'observer-command-mismatch')
 lock=read(s/'migration/baseline/debian13-package-lock.json'); packages={}
 for p in lock['packages']:
  need(regular(d/'apt/archives'/p['filename'])==p['sha256'],'dependency-archive-mismatch'); packages[p['filename']]=p['sha256']
 r['dependency_archives']=packages
 # Hash extracted regular-file contents and reviewed public external files.
 # External directory links are metadata only: their target contents are NOT
 # traversed or proved (especially /etc/ssl/private); no full-host closure claim.
 r['dependency_external_files']={}
 r['dependency_root_sha256']=tree_hash(d/'root',packages,r['dependency_external_files'])
 commands=read(b/'compile_commands.json'); compiled={}; compilers={}
 for row_index,row in enumerate(commands):
  cwd=Path(row['directory']).resolve(); need(cwd==b,'compile-cwd-mismatch')
  args=row.get('arguments') or shlex.split(row['command']); need(isinstance(args,list) and args,'invalid-compile-command')
  need('-o' in args and '-c' in args,'indirect-compile-command'); obj=resolve(args[args.index('-o')+1],cwd)
  need(inside(obj,b),'foreign-object-output')
  if not obj.is_file(): continue # Meson includes unrequested targets; do not claim built.
  src=resolve(row['file'],cwd); need(inside(src,s) or inside(src,b),'foreign-compile-source')
  need(not any(a.startswith('@') or a.startswith('-include') or a.startswith('-imacros') for a in args),'forced-compile-input')
  key=obj.relative_to(b).as_posix(); need(key not in compiled,'duplicate-built-object')
  compiler=Path(shutil.which(args[0]) or args[0]).resolve(); compilers[compiler.name]=sha(compiler)
  validate_compile_paths(args,cwd,roots)
  recipe=normalize('\0'.join(args),roots)
  source_key=('source/' + src.relative_to(s).as_posix()) if inside(src,s) else ('build/'+src.relative_to(b).as_posix())
  if key==GENERATOR['object']:
   need(source_key=='source/'+GENERATOR['source'] and digest(recipe.encode())==GENERATOR['recipe_sha256'],'generator-object-recipe-mismatch')
  else:validate_object_sanitizer(args,recipe,cell,key,source_key,row_index,len(commands))
  compiled[key]={'source':('source/' + src.relative_to(s).as_posix()) if inside(src,s) else ('build/'+src.relative_to(b).as_posix()),'source_sha256':sha(src),'object_sha256':sha(obj),'recipe_sha256':digest(recipe.encode())}
 need(any(x['source']=='source/app/tests/test-matched-preclick.c' for x in compiled.values()),'observer-not-compiled')
 for plugin in PLUGINS: need(any(x['source']=='source/plug-ins/common/'+plugin+'.c' for x in compiled.values()),'plugin-not-compiled')
 need(GENERATOR['object'] in compiled,'generator-object-missing')
 for resource in GENERATOR_RESOURCES[2:]:
  need(any(x['source']=='build/'+resource and x['source_sha256']==r['generator_role']['generated_resources'][resource] for x in compiled.values()),'generator-resource-not-compiled')
 r['compiled']=compiled; r['compilers']=compilers
 closure=run(['ninja','-C',str(b),'-t','commands',*TARGETS],cwd=b)
 normalized=normalize(closure,roots); r['commands_sha256']=digest(normalized.encode())
 maparg='-Wl,--cref,-Map,@build@/libgimpwidgets/frame-helper-provider.map'
 maps=re.findall(r'\S*(?:--cref|-Map)[^\s]*',normalized)
 need(maps==([maparg] if r['arm']=='current' else []),'unexpected-link-map-recipe')
 r['allowed_recipe_exception']=maparg if maps else None
 r['link_policy']=link_policy(normalized.replace(maparg,'' ))
 r['binaries']={x:sha(b/x) for x in TARGETS if x not in ALIASES and not x.startswith('menus/')}
 if r['arm']=='current': r['binaries']['app/tests/painter-compact-options']=sha(b/'app/tests/painter-compact-options')
 r['project_libraries']={p.relative_to(b).as_posix():sha(p) for p in sorted(b.rglob('*')) if p.is_file() and not p.is_symlink() and (p.name.endswith('.a') or '.so' in p.name) and 'gimpdir-output' not in p.parts}
 need(r['project_libraries'],'missing-project-library-inventory')
 r['build_root_identity']=digest(str(b).encode()); r['prefix_root_identity']=digest(str(prefix).encode())
 r['inspection_tools']={x:sha(Path(shutil.which(x)).resolve()) for x in ('ninja','readelf','ld')}
 r['verified']=True; return r

# Exact metadata from the already hash-verified locked Debian archives. This is
# not a general /etc allowlist. No directory target is traversed or enumerated.
PYTHON_ARCHIVE=('libpython3.13-minimal_3.13.5-2+deb13u5_amd64.deb','a4aa6a9a8c77f87bfe545e1d14ec42c001d189ace90e3aa75deec882b39dee49')
SSL_ARCHIVE=('openssl_3.5.7-1~deb13u2_amd64.deb','4ff006f431d8c2e69fa736885a3b6e779f2448535f69dbde821d9f0ff87ad8ea')
X11_ARCHIVE=('x11-common_1%3a7.7+24+deb13u1_all.deb','57a981938506b26dc552f19cafeaa8c04e9b59dc7508509151f8507a9d4e5f24')
PUBLIC_DEPENDENCY_LINKS={
 'usr/share/X11/rgb.txt':('/etc/X11/rgb.txt',X11_ARCHIVE,'2c8ab5acc9eb072f4cc88696834188100d05e50af5d1425501d993700aaa3164'),
 'usr/lib/python3.13/sitecustomize.py':('/etc/python3.13/sitecustomize.py',PYTHON_ARCHIVE,'43d81125d92376b1a69d53a71126a041cc9a18d8080e92dea0a2ae23be138b1e'),
 'usr/lib/ssl/openssl.cnf':('/etc/ssl/openssl.cnf',SSL_ARCHIVE,'a65a2cb9f4ee8ffdc7ef4f0ac600c0bdafb95b7b1ab457188ac610a62f5ad6b3'),
 'usr/lib/ssl/cert.pem':('/etc/ssl/certs/ca-certificates.crt',SSL_ARCHIVE,None),
}
METADATA_ONLY_DEPENDENCY_LINKS={
 'usr/lib/ssl/certs':('/etc/ssl/certs',SSL_ARCHIVE),
 'usr/lib/ssl/private':('/etc/ssl/private',SSL_ARCHIVE),
}
class InventoryLinkError(EvidenceError):
 def __init__(self,relative,category):
  super().__init__('dependency-link-policy-failure')
  # Only package-relative labels, never an arbitrary external link target.
  self.diagnostic={'category':category,'dependency_relative_path':relative if len(relative)<=240 and re.fullmatch(r'[A-Za-z0-9_.+/-]+',relative) and '..' not in Path(relative).parts else None,'dependency_relative_path_sha256':digest(relative.encode())}

MAX_PUBLIC_CONFIG_BYTES=16*1024*1024

def safe_regular_sha(path):
 """Open every component without following links; final descriptor regular only."""
 path=Path(path); need(path.is_absolute(),'noncanonical-public-dependency-file')
 fd=os.open('/',os.O_RDONLY|os.O_DIRECTORY)
 try:
  for part in path.parts[1:-1]:
   nxt=os.open(part,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=fd);os.close(fd);fd=nxt
  final=os.open(path.name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=fd)
  try:
   before=os.fstat(final);need(stat.S_ISREG(before.st_mode),'nonregular-public-dependency-file')
   need(before.st_size<=MAX_PUBLIC_CONFIG_BYTES,'oversized-public-dependency-file')
   h=hashlib.sha256();total=0
   while True:
    data=os.read(final,min(1024*1024,MAX_PUBLIC_CONFIG_BYTES-total+1))
    if not data:break
    total+=len(data);need(total<=MAX_PUBLIC_CONFIG_BYTES,'oversized-public-dependency-file');h.update(data)
   after=os.fstat(final)
   need(total==before.st_size==after.st_size and (before.st_mtime_ns,before.st_ctime_ns)==(after.st_mtime_ns,after.st_ctime_ns),'changed-public-dependency-file')
   return h.hexdigest()
  finally:os.close(final)
 finally:os.close(fd)

def tree_hash(root,locked_archives=None,external_files=None):
 root=Path(root).resolve(); need(root.is_dir(),'missing-inventory-root'); items={}; seen=set()
 locked_archives=locked_archives or {}; external_files=external_files if external_files is not None else {}
 for p in sorted(root.rglob('*')):
  relative=p.relative_to(root).as_posix()
  try:
   if relative in PUBLIC_DEPENDENCY_LINKS:
    target,archive,counterpart=PUBLIC_DEPENDENCY_LINKS[relative]
    need(locked_archives.get(archive[0])==archive[1],'archive-proof-missing')
    need(p.is_symlink() and os.readlink(p)==target,'literal-link-mismatch')
    host_sha=safe_regular_sha(target)
    if counterpart is not None:
     package_sha=safe_regular_sha(root/target.lstrip('/'))
     need(package_sha==counterpart and host_sha==package_sha,'package-counterpart-mismatch')
    record={'kind':'verified-public-host-file-link','literal_target_sha256':digest(target.encode()),'archive_sha256':archive[1],'host_file_sha256':host_sha,'package_counterpart_sha256':counterpart,'scope':'package-identical-public-file' if counterpart else 'generated-public-host-ca-bundle-no-package-counterpart'}
    items[relative]=record;external_files[relative]=record;seen.add(relative);continue
   if relative in METADATA_ONLY_DEPENDENCY_LINKS:
    target,archive=METADATA_ONLY_DEPENDENCY_LINKS[relative]
    need(locked_archives.get(archive[0])==archive[1],'archive-proof-missing')
    need(p.is_symlink() and os.readlink(p)==target,'literal-link-mismatch')
    items[relative]={'kind':'directory-link-metadata-only-not-followed','literal_target_sha256':digest(target.encode()),'archive_sha256':archive[1]};continue
   if p.is_symlink():
    q=p.resolve();need(inside(q,root) or str(q).startswith(('/usr/','/lib/')),'escaping-inventory-link')
   if p.is_file():
    q=p.resolve();need(inside(q,root) or str(q).startswith(('/usr/','/lib/')),'escaping-inventory-link');items[relative]=sha(q)
  except (EvidenceError,OSError,RuntimeError) as error:
   category=str(error) if isinstance(error,EvidenceError) else 'missing-noncanonical-or-unreadable-target'
   raise InventoryLinkError(relative,category) from None
 for relative,(_,archive,_) in PUBLIC_DEPENDENCY_LINKS.items():
  if locked_archives.get(archive[0])==archive[1] and relative not in seen:raise InventoryLinkError(relative,'missing-expected-archive-link')
 need(items,'empty-inventory'); return hashed(items)

def link_policy(commands):
 # The pinned trees intentionally have different object/source graphs. Compare
 # only link policy tokens, retain entire graph/commands hashes separately.
 flags={}
 for line in commands.splitlines():
  args=shlex.split(line)
  if '-c' in args or '-o' not in args: continue
  selected=[a for a in args if a.startswith(('-Wl,','-fsanitize=','-fno-sanitize','-flto','-O','-g','-static','-shared','-pie','-fuse-ld='))]
  output=args[args.index('-o')+1]; need(output not in flags,'duplicate-link-output'); flags[output]=selected
 need(flags,'missing-link-commands'); return flags

def runtime_settings(env):
 expected={'LANG':'C.UTF-8','LC_ALL':'C.UTF-8','GDK_BACKEND':'x11','GDK_SCALE':'1','GDK_DPI_SCALE':'1','GIMP_TESTING_SESSIONRC_NAME':'sessionrc-native-oncanvas','UI_TEST':'yes'}
 need(all(env.get(k)==v for k,v in expected.items()),'runtime-setting-mismatch')
 allowed={'GIMP_TESTING_ABS_TOP_SRCDIR','GIMP_TESTING_ABS_TOP_BUILDDIR','GIMP_TESTING_PLUGINDIRS','GIMP_TESTING_SESSIONRC_NAME'}
 need(not any(k not in allowed for k in env if k.startswith(('GIMP_TEST','GIMP_BRUSH_'))),'unexpected-test-control')
 render=('GSK_RENDERER','LIBGL_ALWAYS_SOFTWARE','GALLIUM_DRIVER','MESA_LOADER_DRIVER_OVERRIDE','GDK_DEBUG','GTK_DEBUG','GTK_THEME','GDK_SYNCHRONIZE','G_MESSAGES_DEBUG','G_SLICE','G_DEBUG','PANGOCAIRO_BACKEND')
 return {'selected':expected,'render_controls_sha256':hashed({k:env.get(k) for k in render}),'loader_controls_sha256':hashed({k:env.get(k) for k in ('LD_BIND_NOW','GLIBC_TUNABLES')})}

def verify_sanitizer_runtime_options(env,cell):
 # Pinned Meson 1.7.0 mtest.py injects these defaults for every test, including
 # unsanitized executables. Accept exact defaults only, never arbitrary options.
 expected={'ASAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1',
           'UBSAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1:print_stacktrace=1',
           'MSAN_OPTIONS':'halt_on_error=1:abort_on_error=1:print_summary=1:print_stacktrace=1'}
 need(cell in CELLS,'unknown-cell')
 if cell.endswith('sanitizer'):
  expected.update(ASAN_OPTIONS='halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 need(all(env.get(k)==v for k,v in expected.items()) and 'LSAN_OPTIONS' not in env,'sanitizer-runtime-option-mismatch')

def actual_environment(row):
 env=row.get('env'); need(isinstance(env,dict) and all(isinstance(k,str) and isinstance(v,str) for k,v in env.items()),'missing-actual-environment')
 no_loader_injection(env); return env

def loader_providers(binary,build,deps,env):
 b=Path(build).resolve(); d=Path(deps).resolve(); binary=Path(binary).resolve()
 text=run(['readelf','-lW',str(binary)]); matches=re.findall(r'Requesting program interpreter: ([^\]]+)',text)
 need(len(matches)==1,'missing-elf-interpreter'); loader=Path(matches[0]).resolve()
 need(loader.name=='ld-linux-x86-64.so.2' and str(loader).startswith(('/usr/lib/','/lib/','/lib64/')),'unapproved-elf-interpreter')
 replay={k:v for k,v in os.environ.items() if not k.startswith('LD_') and k!='GLIBC_TUNABLES'}
 for k in ('LD_LIBRARY_PATH','LD_BIND_NOW','GLIBC_TUNABLES'):
  if k in env: replay[k]=env[k]
 before=sha(binary); out=run([str(loader),'--list',str(binary)],cwd=b,env=replay); providers={}
 for line in out.splitlines():
  if 'linux-vdso' in line: continue
  m=re.search(r'(?:=>\s*)?(/\S+)\s+\(0x[0-9a-f]+\)',line); need(m is not None,'unparsed-loader-record'); p=Path(m[1]).resolve()
  label=('build/'+p.relative_to(b).as_posix()) if inside(p,b) else ('deps/'+p.relative_to(d).as_posix()) if inside(p,d) else 'system/'+p.as_posix().lstrip('/') if str(p).startswith(('/usr/','/lib/')) else None
  need(label is not None,'cross-cell-library-provider')
  need(not p.name.startswith('libgimp') or label.startswith('build/'),'foreign-project-library')
  need(label not in providers,'duplicate-library-provider'); providers[label]=sha(p)
 need(any('libgimpwidgets' in x for x in providers),'missing-project-provider'); need(sha(binary)==before,'binary-changed-during-replay')
 return {'binary_sha256':before,'loader_sha256':sha(loader),'providers':providers,'loader_environment_sha256':hashed({k:env.get(k) for k in ('LD_LIBRARY_PATH','LD_BIND_NOW','GLIBC_TUNABLES')}),'resolution_replay_not_live_modules':True}

def runtime_preflight(build,cell,order):
 need(cell in CELLS and 1<=order<=14 and ORDER[order-1]==cell,'unknown-invocation'); b=Path(build).resolve()
 need((b/'build.ninja').is_file(),'missing-build'); need(not (b/'app/tests/gimpdir-output/pluginrc').exists(),'stale-plugin-cache')
 r=base('runtime-preflight'); r.update(verified=True,cell=cell,order=order,build_root_identity=digest(str(b).encode()),plugin_cache_absent=True); return r

def verify_runtime_identity(identity,build,cell):
 need(identity.get('schema')=='matched-preclick-build-identity-v1' and identity.get('verified') is True and identity.get('producer_sha256')==sha(__file__) and identity.get('cell')==cell and identity.get('build_root_identity')==digest(str(build).encode()),'unproven-runtime-identity')
 need(identity.get('binaries') and identity.get('project_libraries'),'empty-runtime-identity')
 for group in ('binaries','project_libraries'):
  for relative,h in identity[group].items():
   p=build/relative; need(inside(p,build) and not Path(relative).is_absolute(),'escaping-identity-path'); need(sha(p)==h,'changed-runtime-provider')

def runtime_postflight(build,cell,order,log):
 need(cell in CELLS and 1<=order<=14 and ORDER[order-1]==cell,'unknown-invocation'); b=Path(build).resolve()
 r=base('runtime-postflight'); r.update(verified=True,cell=cell,order=order,build_root_identity=digest(str(b).encode()),raw_log_sha256=sha(log),plugin_cache_sha256=regular(b/'app/tests/gimpdir-output/pluginrc')); return r

def runtime_inputs(source,build,cell,deps,profile,catalogs,log,order,preflight,wrapper,identity,postflight,cache_directory):
 from matched_resources import runtime_resources
 with runtime_stage('source-identity'):
  need(cell in CELLS and 1<=order<=14 and ORDER[order-1]==cell,'unknown-invocation'); s=Path(source).resolve(); b=Path(build).resolve(); r=base('runtime-inputs'); r.update(source_identity(s)); r.update(cell=cell,order=order)
 with runtime_stage('raw-log'):
  with Path(log).open('rb') as stream: raw_bytes=stream.read(MAX_BYTES+1)
  need(len(raw_bytes)<=MAX_BYTES,'oversized-raw-log'); raw_digest=digest(raw_bytes)
  rows=[json.loads(x,object_pairs_hook=pairs) for x in raw_bytes.decode('utf-8').splitlines() if x.strip()]; need(len(rows)==1,'raw-registration-count')
 with runtime_stage('build-identity'):
  ident=read(identity); verify_runtime_identity(ident,b,cell); r['identity_sha256']=sha(identity)
  need(all(ident.get(k)==r.get(k) for k in ('source_commit','source_tree','source_root_identity','configuration_overlay')),'runtime-source-identity-mismatch')
 with runtime_stage('postflight-cache'):
  post=read(postflight); cache=Path(cache_directory).resolve(); need(cache==Path(profile).resolve()/'native-output','unexpected-archived-cache-directory')
  need(post.get('schema')=='matched-preclick-runtime-postflight-v1' and post.get('verified') is True and post.get('producer_sha256')==sha(__file__) and post.get('cell')==cell and post.get('order')==order and post.get('build_root_identity')==digest(str(b).encode()) and post.get('raw_log_sha256')==raw_digest and post.get('plugin_cache_sha256')==regular(cache/'pluginrc'),'invalid-runtime-postflight')
  r['raw_log_sha256']=raw_digest; r['postflight_sha256']=sha(postflight)
 with runtime_stage('preflight'):
  pre=read(preflight); need(pre.get('schema')=='matched-preclick-runtime-preflight-v1' and pre.get('verified') is True and pre.get('plugin_cache_absent') is True and pre.get('producer_sha256')==sha(__file__) and pre.get('cell')==cell and pre.get('order')==order and pre.get('build_root_identity')==digest(str(b).encode()),'invalid-runtime-preflight')
 with runtime_stage('actual-environment'):
  row=rows[0]; env=actual_environment(row); r['runtime_settings']=runtime_settings(env); r['profile_root_identity']=digest(str(Path(profile).resolve()).encode()); cmd=row.get('command') or row.get('cmd'); expected='app/tests/matched-preclick' if order<=12 else 'app/tests/painter-compact-options'
 with runtime_stage('wrapper-command'):
  need(isinstance(cmd,list) and len(cmd)==5 and re.fullmatch(r'python3(?:\.\d+)?',Path(cmd[0]).name) and Path(cmd[1]).resolve()==Path(wrapper).resolve() and Path(cmd[2]).resolve()==b/expected,'runtime-binary-mismatch')
  need(cmd[3:]==['-p','/matched-preclick/primary' if order<=12 else '/brush-thumbnail-native-popup/private-target-release'],'runtime-selector-mismatch')
  r['wrapper_sha256']=regular(wrapper); need(r['wrapper_sha256']==WRAPPER_SHA,'wrapper-mismatch'); r['preflight_sha256']=sha(preflight)
 with runtime_stage('sanitizer-options'):
  verify_sanitizer_runtime_options(env,cell)
  need(env.get('GIMP_TESTING_ABS_TOP_SRCDIR')==str(s) and env.get('GIMP_TESTING_ABS_TOP_BUILDDIR')==str(b),'runtime-source-build-mismatch')
 with runtime_stage('schema-search'):
  schema_dirs=[]
  if env.get('GSETTINGS_SCHEMA_DIR'): schema_dirs.append(env['GSETTINGS_SCHEMA_DIR'])
  for root in env.get('XDG_DATA_DIRS','/usr/local/share:/usr/share').split(':'):
   candidate=Path(root)/'glib-2.0/schemas'
   if candidate.is_dir(): schema_dirs.append(str(candidate))
 evidence={'cache_absent_preflight':True,'schema_dirs':sorted(set(schema_dirs))}
 with runtime_stage('font-list'):
  evidence['fc_list_stdout']=run(['fc-list','--format=%{file}\n'],cwd=b,env=env)
 with runtime_stage('font-config'):
  evidence['fc_conflist_stdout']=run(['fc-conflist'],cwd=b,env=env)
 with runtime_stage('catalog-aliases'):
  evidence['ninja_alias_queries']={a:run(['ninja','-C',str(b),'-t','query',a],cwd=b) for a in ALIASES}
 with runtime_stage('resource-inventory'):
  r['resources_helper_sha256']=sha(Path(__file__).with_name('matched_resources.py'))
  r['resources']=runtime_resources(s,b,Path(profile),Path(catalogs),env,evidence,cache_directory=cache)
 with runtime_stage('loader-replay'):
  r['loader']=loader_providers(b/expected,b,Path(deps),env)
 with runtime_stage('final-recheck'):
  verify_runtime_identity(ident,b,cell); need(sha(log)==raw_digest,'raw-log-changed-during-inspection')
 r['verified']=True; return r

def compare(identities,runtimes):
 r=base('comparison-identity'); r.update(comparison_eligible=False,closure_comparable=False,cells={},runs=[]); errors=r['errors']
 try:
  ids={x['cell']:(p,x) for p,x in identities}; need(len(ids)==len(identities)==4 and set(ids)==set(CELLS),'identity-cell-set')
  need(len(runtimes)==14 and {x['order'] for _,x in runtimes}==set(range(1,15)),'runtime-order-set')
  need(len({x['profile_root_identity'] for _,x in runtimes})==14 and len({x['raw_log_sha256'] for _,x in runtimes})==14,'reused-runtime-evidence')
  need(all(x['runtime_settings']==runtimes[0][1]['runtime_settings'] for _,x in runtimes),'runtime-setting-mismatch')
  need(len({x['source_root_identity'] for _,x in identities})==4 and len({x['build_root_identity'] for _,x in identities})==4,'reused-cell-directory')
  for cell,(path,x) in ids.items():
   arm,mode=cell.split('-'); need(x.get('schema')=='matched-preclick-build-identity-v1' and x.get('verified') is True and not x.get('errors'),'unverified-build-identity')
   need(x.get('producer_sha256')==sha(__file__) and x.get('resources_helper_sha256')==sha(Path(__file__).with_name('matched_resources.py')),'producer-mismatch')
   need(x.get('common_inputs')==COMMON and x.get('overlay')==OVERLAY and all(x.get(k) for k in ('dependency_archives','dependency_root_sha256','compiled','compilers','inspection_tools','binaries','project_libraries','options','link_policy')),'incomplete-build-evidence')
   need((x['source_commit'],x['source_tree'])==COMMITS[arm],'identity-source-mismatch')
   need(x.get('configuration_overlay')==configuration_overlay(arm),'configuration-identity-mismatch')
   runs=[(p,y) for p,y in runtimes if y['cell']==cell]; need(len(runs)==(4 if arm=='current' else 3),'cell-runtime-count')
   for _,y in runs:
    need(y.get('configuration_overlay')==x['configuration_overlay'],'runtime-configuration-mismatch')
    need(y.get('producer_sha256')==sha(__file__) and y.get('resources_helper_sha256')==x['resources_helper_sha256'],'producer-mismatch')
    need(y.get('identity_sha256')==sha(path) and y.get('wrapper_sha256')==WRAPPER_SHA and ORDER[y['order']-1]==cell,'runtime-identity-binding-mismatch')
    executable='app/tests/matched-preclick' if y['order']<=12 else 'app/tests/painter-compact-options'
    need(y['loader']['binary_sha256']==x['binaries'][executable],'runtime-binary-hash-mismatch')
    need(y['loader'].get('providers') and y['loader'].get('resolution_replay_not_live_modules') is True,'missing-runtime-provider-closure')
    for label,h in y['loader']['providers'].items():
     if label.startswith('build/'): need(x['project_libraries'].get(label[6:])==h,'runtime-library-hash-mismatch')
    need(y.get('schema')=='matched-preclick-runtime-inputs-v1' and y.get('verified') is True and not y.get('errors'),'unverified-runtime-inputs')
    need(y['resources'].get('comparison_eligible') is True,'unproven-resource-inventory')
   for p,y in runs:
    r['runs'].append({'order':y['order'],'cell':cell,'kind':'primary' if y['order']<=12 else 'fidelity','repetition':(sum(1 for c in ORDER[:y['order']] if c==cell) if y['order']<=12 else 1),'raw_log_sha256':y['raw_log_sha256'],'runtime_inventory_sha256':sha(p)})
   r['cells'][cell]={'source_commit':x['source_commit'],'source_tree':x['source_tree'],'closure_comparable':True,'configuration_overlay':x['configuration_overlay'],'identity_sha256':sha(path),'runtime_inventory_sha256':sorted(sha(p) for p,_ in runs)}
  first=identities[0][1]
  for _,x in identities:
   role=x.get('generator_role',{})
   need(role.get('policy')==GENERATOR and role.get('target_machine')=='build' and role.get('installed') is False,'generator-role-unverified')
   need(role.get('generated_resources')==first['generator_role']['generated_resources'] and set(role['generated_resources'])==set(GENERATOR_RESOURCES),'generator-resource-mismatch')
   need(role.get('link_recipe_sha256')==first['generator_role'].get('link_recipe_sha256') and isinstance(role.get('link_recipe_sha256'),str),'generator-link-recipe-mismatch')
   need(x['compiled'][GENERATOR['object']]['recipe_sha256']==GENERATOR['recipe_sha256'] and x['compiled'][GENERATOR['object']]['source_sha256']==GENERATOR['source_sha256'],'generator-object-recipe-mismatch')
  for _,x in identities[1:]:
   for key in ('options','build_environment_sha256','dependency_archives','dependency_root_sha256','dependency_external_files','compilers','inspection_tools','common_inputs','overlay'):
    need(x[key]==first[key],'common-build-input-mismatch')
  for mode in ('normal','sanitizer'):
   a=ids['baseline-'+mode][1]; z=ids['current-'+mode][1]; 
   for output in set(a['link_policy']) & set(z['link_policy']): need(a['link_policy'][output]==z['link_policy'][output],'link-policy-mismatch')
   for obj in set(a['compiled']) & set(z['compiled']):
    aa=a['compiled'][obj]; zz=z['compiled'][obj]
    need(aa['source']==zz['source'] and aa['recipe_sha256']==zz['recipe_sha256'],'common-object-recipe-mismatch')
  for mode in ('normal','sanitizer'):
   selected=[x for _,x in runtimes if x['cell'].endswith(mode)]
   external=[{k:v for k,v in x['loader']['providers'].items() if not k.startswith('build/')} for x in selected]
   need(external[0] and all(x==external[0] for x in external),'external-runtime-provider-mismatch')
  r['runs'].sort(key=lambda x:x['order'])
  resource=hashed(runtimes[0][1]['resources']['comparison_inventory'])
  need(all(hashed(x['resources']['comparison_inventory'])==resource for _,x in runtimes),'runtime-resource-mismatch')
  r.update(verified=True,comparison_eligible=True,closure_comparable=True)
 except (EvidenceError,KeyError,TypeError,ValueError) as e:
  errors.append(str(e) if isinstance(e,EvidenceError) else 'malformed-identity')
 return r

def main():
 p=argparse.ArgumentParser(); subs=p.add_subparsers(dest='operation',required=True)
 for op in ('overlay','build-identity','runtime-preflight','runtime-postflight','runtime-inputs','compare'):
  q=subs.add_parser(op); q.add_argument('--output',required=True,type=Path)
  if op not in ('compare','runtime-preflight','runtime-postflight'): q.add_argument('--source',required=True,type=Path)
  if op=='overlay': q.add_argument('--inputs',required=True,type=Path)
  if op in ('build-identity','runtime-inputs'):
   for name in ('build','deps'): q.add_argument('--'+name,required=True,type=Path)
   q.add_argument('--cell',choices=CELLS,required=True)
  if op in ('runtime-preflight','runtime-postflight'):
   q.add_argument('--build',required=True,type=Path); q.add_argument('--cell',choices=CELLS,required=True); q.add_argument('--order',type=int,required=True)
   if op=='runtime-postflight': q.add_argument('--log',required=True,type=Path)
  if op=='runtime-inputs':
   for name in ('profile','catalogs','log','preflight','wrapper','identity','postflight','cache-directory'): q.add_argument('--'+name,required=True,type=Path)
   q.add_argument('--order',type=int,required=True)
  if op=='compare':
   q.add_argument('--identities',nargs=4,required=True,type=Path); q.add_argument('--runtime-inputs',nargs=14,required=True,type=Path)
 a=p.parse_args(); r=base(a.operation)
 try:
  if a.operation=='overlay': r.update(source_identity(a.source,a.inputs)); r['verified']=True
  elif a.operation=='build-identity': r=build_identity(a.source,a.build,a.cell,a.deps)
  elif a.operation=='runtime-preflight': r=runtime_preflight(a.build,a.cell,a.order)
  elif a.operation=='runtime-postflight': r=runtime_postflight(a.build,a.cell,a.order,a.log)
  elif a.operation=='runtime-inputs': r=runtime_inputs(a.source,a.build,a.cell,a.deps,a.profile,a.catalogs,a.log,a.order,a.preflight,a.wrapper,a.identity,a.postflight,a.cache_directory)
  else: r=compare([(x,read(x)) for x in a.identities],[(x,read(x)) for x in a.runtime_inputs])
 except (EvidenceError,OSError,ValueError,KeyError,TypeError,StopIteration,subprocess.SubprocessError) as e:
  r['errors'].append(str(e) if isinstance(e,EvidenceError) else 'inspection-unavailable')
  if isinstance(e,RuntimeInspectionError):r['runtime_failure']=e.diagnostic
  if isinstance(e,InventoryLinkError):r['inventory_link_failure']=e.diagnostic
  if isinstance(e,ObjectSanitizerError):r['object_sanitizer_failure']=e.diagnostic
  if isinstance(e,GeneratorSubmoduleError):r['generator_submodule_failure']=e.diagnostic
 a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(r,sort_keys=True,indent=2)+'\n')
 return 0 if r['verified'] else 1
if __name__=='__main__': raise SystemExit(main())
