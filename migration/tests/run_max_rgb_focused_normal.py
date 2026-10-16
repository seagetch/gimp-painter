import sys,os,json,subprocess,hashlib,re,time,importlib.util
from pathlib import Path
source=Path(sys.argv[1]).resolve();build=Path(sys.argv[2]).resolve();out=Path(sys.argv[3]).resolve();out.mkdir()
sys.path.insert(0,str(source/'migration/tests'))
from run_sharpen_context_sanitizers import tap_result
names={
'gimp-filter-layer':['integer_native_route','invalid_definitions_keep_cache','gray_unsupported','tiny_selection_routes','late_context_routes','component_masks','sealed_context_routes','outside_selection_no_merge','cancel_replace_lower_close','partial_import_lifecycle','native_profile_bytes','admission_preserves_cache','real_routes_unchanged','projection_input_bounds','actual_old_live','actual_old_projection_history'],
'painter-xcf-open':['synthetic_capsule_roundtrip','malformed_capsules','active_save_close_reopen','old_writer_roundtrip']}
registry={r['name']:r for r in json.loads((build/'meson-info/intro-tests.json').read_text())}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=source).decode().split('\0');files=sorted(set(n for n in files if n and (source/n).is_file()))
seals={n:digest(source/n) for n in files};exes={n:digest(build/'app/tests'/n) for n in names}
report={'scope':'Twenty explicit Max RGB owner/XCF tests in normal native build','source_before':seals,'executables_before':exes,'tests':[],'all_passed':False}
def save(): (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
save()
for target,tests in names.items():
 env=os.environ.copy()
 for k,v in registry[target]['env'].items():env[k]=v+(':'+env[k] if k=='LD_LIBRARY_PATH' and env.get(k) else '')
 for name in tests:
  path='/'+target+'/max_rgb_'+name;cmd=[str(build/'app/tests'/target),'--tap','-p',path];start=time.monotonic()
  try:
   run=subprocess.run(cmd,cwd=build,env=env,capture_output=True,text=True,timeout=300);stdout,stderr,code=run.stdout,run.stderr,run.returncode
  except subprocess.TimeoutExpired as e:stdout=(e.stdout or b'').decode();stderr=(e.stderr or b'').decode();code=None
  tap=tap_result(stdout,path);bad=bool(re.search(r'CRITICAL|AddressSanitizer|runtime error:|Bail out!|^not ok',stdout+'\n'+stderr,re.M))
  row={'path':path,'command':cmd,'exit_code':code,'seconds':time.monotonic()-start,'tap':tap,'passed':code==0 and tap['exact_positive_result'] and not bad,'stdout':stdout,'stderr':stderr};report['tests'].append(row);save();print(('PASS ' if row['passed'] else 'FAIL ')+path,flush=True)
  if not row['passed']:break
 if report['tests'] and not report['tests'][-1]['passed']:break
report['source_changed']=[n for n,h in seals.items() if not (source/n).is_file() or digest(source/n)!=h]
report['executables_after']={n:digest(build/'app/tests'/n) for n in names}
report['all_passed']=len(report['tests'])==20 and all(r['passed'] for r in report['tests']) and not report['source_changed'] and report['executables_before']==report['executables_after'];save();sys.exit(0 if report['all_passed'] else 1)
