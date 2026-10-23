#!/usr/bin/env python3
"""Validate bounded navigation receipts; no task closure or runtime replay."""
import argparse,hashlib,json,pathlib,struct,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[3]
def sha(data): return hashlib.sha256(data).hexdigest()
def require(value,message):
    if not value: raise ValueError(message)
def check(root,evidence):
    m=json.loads((evidence/'manifest.json').read_text())
    require(m['base_commit']=='ce40ac4cffa5b6dba48cd9100303c3c94653a5bc','base identity')
    require(m['native_cases']==35 and m['new_cases']==4,'case count')
    require(m['legacy_motion_checks']==1920 and m['legacy_point_pairs']==5760,'oracle matrix count')
    require(m['xvfb']=={'width':1080,'height':1920,'depth':24,'authenticated':True},'portrait authentication')
    require(m['new_ui_oncanvas'] is True and m['original_render_environment_unchanged'] is True,'UI scope')
    require(m['build_policy']['b_ndebug'] is False and m['build_policy']['debug'] is False,'assertion policy')
    for p,h in m['source_sha256'].items(): require(sha((root/p).read_bytes())==h,'source hash '+p)
    for p,h in m['artifact_sha256'].items(): require(sha((evidence/p).read_bytes())==h,'artifact hash '+p)
    inventory='migration/inventory/additional-c-header-routes.json'
    built=json.loads((evidence/'build-header-routes.json').read_text())
    current=json.loads((root/inventory).read_text())
    route=lambda m:next(r for r in m['routes']if r['path']=='app/display/gimpdisplayshell-scroll.h')
    require(route(current)==route(built),'current navigation header route')
    baseline=json.loads(subprocess.check_output(['git','show',m['base_commit']+':'+inventory],cwd=ROOT))
    baseline['routes'].append(route(current))
    require(baseline==built,'isolated header closure must be pinned baseline plus navigation only')
    native=json.loads((evidence/'native.json').read_text())
    require(native['result']=='OK' and native['returncode']==0,'native result')
    cases=[x for x in native['stdout'].splitlines() if x.startswith('ok ')]
    require(len(cases)==35,'native TAP count')
    for name in ['inherited-fractional-pan','oncanvas-render','transitions','invalid-and-space']:
        require(any('/navigation-events/legacy/'+name in x for x in cases),'missing '+name)
    require(native['stdout'].count('# old pan ')==5760,'actual oracle point count')
    red=json.loads((evidence/'expected-red.json').read_text())[0]
    require(red['result']=='FAIL' and red['returncode']==-6,'expected-red status')
    require('actual 370,233' in red['stdout'] and 'expected center 371.00602758221567,233.98001204063144' in red['stdout'],'expected-red geometry')
    for p in ['portrait-oncanvas-pan-m0.png','portrait-oncanvas-pan-m1.png']:
        b=(evidence/p).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'screenshot dimensions')
    fixture=root/'migration/fixtures/legacy-pan';oracle=(fixture/'oracle.inc').read_text();fm=json.loads((fixture/'source-manifest.json').read_text())
    require(fm['commit']=='afa43fae3e920210146abed514f136fd49f671b5','old source identity')
    require(len(fm['pieces'])==5,'old immutable bodies count')
    for name,h in fm['pieces'].items():
        body=(fixture/(name+'.txt')).read_bytes();require(sha(body)==h,'old body hash '+name)
        require(body.decode() in oracle,'old body omitted from oracle '+name)
        path='app/display/gimpdisplayshell-rotate.c' if name in ['gimp_display_shell_set_cairo_rotate','gimp_display_shell_device_to_image_coords'] else 'app/display/gimpdisplayshell-scroll.c' if name=='gimp_display_shell_scroll_clamp_offsets' else 'app/display/gimpdisplayshell-tool-events.c'
        original=subprocess.check_output(['git','show',fm['commit']+':'+path],cwd=ROOT)
        require(body in original,'body differs from pinned Git source '+name)
    if m['sanitizer_status']=='passed':
        san=json.loads((evidence/'sanitizer.json').read_text())
        require(san['result']=='OK' and san['returncode']==0,'sanitizer result')
        require(len([x for x in san['stdout'].splitlines() if x.startswith('ok ')])==35,'sanitizer TAP count')
    else: require(m['sanitizer_status']=='pending','unrecognized sanitizer state')
    return {'status':'PASS','native_cases':35,'old_motion_checks':1920,'point_pairs':5760,'sanitizer':m['sanitizer_status'],'task_closure':False}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,default=ROOT);p.add_argument('--evidence',type=pathlib.Path);a=p.parse_args()
    print(json.dumps(check(a.root,a.evidence or a.root/'migration/tests/navigation-legacy-pan'),sort_keys=True))
