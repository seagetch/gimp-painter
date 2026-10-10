from pathlib import Path
import hashlib,json,subprocess,re,os
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree');out=Path(__file__).resolve().parent
report=json.loads((out/'sanitizer-build.json').read_text());assert report['build_status']=='passed' and not report['changed_during_build'] and report['production_artifacts_unchanged']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert report['tests']==['painter-perspective'];binary=Path(report['executables'][0]);assert sha(binary)==report['executables_sha256'][str(binary)]
for path,digest in report['sources_sha256'].items():assert sha(root/path)==digest,path
env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
r=subprocess.run([str(binary)],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=240)
cases=re.findall(r'^ok \d+ (.+)$',r.stdout,re.M);errors=re.findall(r'(?:ERROR: AddressSanitizer|runtime error:|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer))[^\n]*',r.stdout)
changed=[p for p,h in report['sources_sha256'].items() if sha(root/p)!=h]
d=dict(name='sanitizer-perspective-model-type',status='PASS' if r.returncode==0 and len(cases)==12 and not errors and not changed else 'FAIL',exit_code=r.returncode,cases=cases,sanitizer_errors=errors,argv=[str(binary)],binary_sha256=sha(binary),build_report_sha256=sha(out/'sanitizer-build.json'),changed_during_run=changed,sanitizer_options={k:env[k] for k in ('ASAN_OPTIONS','UBSAN_OPTIONS')},stdout=r.stdout,scope='Twelve model/core cases under focused ASan/UBSan source coverage; RTTI-only and uninstrumented dependencies enumerated in build report, LeakSanitizer disabled. No new UI or platform acceptance.')
(out/'sanitizer.json').write_text(json.dumps(d,indent=2)+'\n');(out/'sanitizer.log').write_text(r.stdout);print(json.dumps({k:d[k] for k in ['name','status','exit_code','cases','sanitizer_errors','changed_during_run']}));assert d['status']=='PASS',r.stdout[-5000:]
