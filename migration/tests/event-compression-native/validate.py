from pathlib import Path
import hashlib,json,gzip,tarfile,struct,re,os
E=Path(__file__).resolve().parent;R=Path(os.environ.get('GIMP_EVENT_PROOF_REPO',E.parents[2]))
sha=lambda b:hashlib.sha256(b).hexdigest()
read=lambda n:json.loads((E/n).read_text())
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
s=read('scope.json');assert s['base']=='55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94' and s['status']=='DOING'
assert s['original_criterion']=='旧compress_motionとnext_event経路を現行入力へ接続し、pen up・key・crossing・device切替が欠落せずイベント所有が一回解放される'
assert s['normal_passed']==s['focused_sanitizer_passed']==14 and not s['hardware_windows_history_proof']
for p,h in {**s['providers'],**s['fixture']}.items():assert sha((R/p).read_bytes())==h,p
with tarfile.open(E/'source-capture.tar.gz') as t:
 candidate={p:t.extractfile('candidate/'+p).read() for p in s['providers']}
 for p,h in s['providers'].items():assert sha(candidate[p])==h
 fixture=t.extractfile('fixture/test-event-order.c').read();assert sha(fixture)==s['fixture']['app/tests/test-event-compression-order.c']
for key in ['gimp_painter_canvas_ui_set_visible(shell,TRUE)','g_assert_true(gimp_painter_canvas_ui_get_visible(shell))','==,1080','==,1920']:assert key in fixture.decode()
expected={'queued-terminals','exact-reset','exact-tool-switch','exact-image-switch','exact-guide-change','prior-false-idempotent','aborted-begin','recreated-window','nested-new-stroke','halted-begin','rejected-begin','device-boundary','mode-change','other-shell-dispose'}
for mode in ['normal','sanitizer']:
 r=read('final-'+mode+'.json');b=read('final-'+mode+'-build.json');assert r['returncode']==0 and r['runtime_inputs_unchanged'];assert b['unchanged'] and b['source_unchanged']
 rows=re.findall(r'^ok \d+ /event-order/(\S+)(.*)$',r['stdout'],re.M)
 assert {name for name,tail in rows if 'SKIP' not in tail}==expected and len(rows)==15
 assert [(name,tail) for name,tail in rows if 'SKIP' in tail][0][0]=='close-active-image'
 assert not any(x in r['stderr'] for x in ['AddressSanitizer:','runtime error:'])
 assert b['binary_sha256'] in r['binary_and_runtime_inputs'].values()
 for p,h in b['source_inputs'].items():
  if '/overlay/app/display/' in p:assert h==s['providers']['app/display/'+Path(p).name]
  elif p.endswith('/test-event-order.c'):assert h==sha(fixture)
 if mode=='sanitizer':
  assert '-fsanitize=address,undefined' in b['compile'] and len(b['provider_compile'])==3
  assert all('-fsanitize=address,undefined' in c for c in b['provider_compile'])
 links=json.loads(gzip.decompress((E/('final-'+mode+'-link.json.gz')).read_bytes()));assert len(links)==b['inputs']
for n in ['baseline-tool-final.json','baseline-image-final.json']:
 r=read(n);assert r['returncode']!=0 and r['runtime_inputs_unchanged'] and "'gdk_window_get_event_compression(w)' should be TRUE" in r['stderr']
r=read('generation-mutant.json');assert r['returncode']!=0 and "'shell->painter_compression_window==w' should be TRUE" in r['stderr']
original=candidate['app/display/gimpdisplayshell-tool-events.c'].decode();guard='if (! active &&\n      compression_generation == shell->painter_compression_generation)';assert original.count(guard)==1
assert (E/'generation-mutant-provider.c').read_text()==original.replace(guard,'if (! active)')
r=read('candidate-complete.json');assert r['returncode']!=0 and "'shell->grab_seat' should be NULL" in r['stderr']
h=read('headers.json');assert {r['language'] for r in h['results']}=={'c','cpp'}
for r in h['results']:assert ' U gimp_display_shell_restore_motion_compression\n' in r['unresolved']
png=(E/'oncanvas-event-order-portrait.png').read_bytes();assert png[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',png[16:24])==(1080,1920)
print('PASS compression:14 normal +14 focused sanitizer,2 baseline reds,causal generation mutant,portrait oncanvas; empty-shell grab gap and original23.014 remain open')
