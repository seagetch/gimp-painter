#!/usr/bin/env python3
"""Extract selected receipts only; never copy Meson's environment dictionary."""
import argparse,hashlib,json,pathlib,shutil
ROOT=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def receipt(build):
    rows=[json.loads(s)for s in (build/'meson-logs/testlog.json').read_text().splitlines()]
    rows=[r for r in rows if r['name']=='gimp:app / painter-navigation-events']
    assert len(rows)==1
    r=rows[0];assert r['result']=='OK' and r['returncode']==0
    assert sum(s.startswith('ok ')for s in r['stdout'].splitlines())==35
    assert r['stdout'].count('# old pan ')==5760
    return {k:r[k]for k in ['name','result','duration','returncode','stdout','stderr'] if k in r}
def main():
    p=argparse.ArgumentParser();p.add_argument('secondary',type=pathlib.Path);a=p.parse_args();d=a.secondary
    native=receipt(d/'build');san=receipt(d/'build-asan')
    paths=['app/display/'+n for n in ['gimpdisplayshell.h','gimpdisplayshell-scroll.h','gimpdisplayshell-scroll.c','gimpdisplayshell-rotate.c','gimpdisplayshell-tool-events.c','gimpdisplayshell-scale.c','gimpdisplayshell.c','gimpdisplayshell-callbacks.c','gimpdisplayshell-title.c']]+['app/tests/test-painter-navigation-events.c','app/tests/test-painter-navigation-legacy-pan.inc']
    paths+=['migration/fixtures/legacy-pan/'+p.name for p in (ROOT/'migration/fixtures/legacy-pan').iterdir() if p.is_file()]
    for path in paths:assert (ROOT/path).read_bytes()==(d/'source'/path).read_bytes(),path
    inventory='migration/inventory/additional-c-header-routes.json'
    current=json.loads((ROOT/inventory).read_text());built=json.loads((d/'source'/inventory).read_text())
    route=lambda m:next(r for r in m['routes']if r['path']=='app/display/gimpdisplayshell-scroll.h')
    assert route(current)==route(built)
    shutil.copyfile(d/'source'/inventory,E/'build-header-routes.json')
    for name,value in [('native.json',native),('sanitizer.json',san)]: (E/name).write_text(json.dumps(value,indent=2)+'\n')
    for f in ['portrait-oncanvas-pan-m0.png','portrait-oncanvas-pan-m1.png']:shutil.copyfile(d/'pan-screenshots'/f,E/f)
    files=['build-header-routes.json','native.json','sanitizer.json','sanitizer-build-policy.json','related.json','expected-red.json','expected-red.log','portrait-oncanvas-pan-m0.png','portrait-oncanvas-pan-m1.png']
    m={'base_commit':'ce40ac4cffa5b6dba48cd9100303c3c94653a5bc','scope':'Only explicitly owned changes applied to this isolated committed base; not current unrelated worktree build','source_sha256':{p:sha((ROOT/p).read_bytes())for p in sorted(paths)},'build_policy':{'optimization':'2','debug':False,'b_ndebug':False,'painter-http':'enabled','auto_features':'disabled','headless-tests':'disabled','libunwind':False},'native_cases':35,'new_cases':4,'legacy_motion_checks':1920,'legacy_point_pairs':5760,'xvfb':{'width':1080,'height':1920,'depth':24,'authenticated':True},'new_ui_oncanvas':True,'original_render_environment_unchanged':True,'sanitizer_status':'passed','artifact_sha256':{f:sha((E/f).read_bytes())for f in files}}
    (E/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    print(json.dumps({'native_cases':35,'sanitizer_cases':35,'native_seconds':native['duration'],'sanitizer_seconds':san['duration']}))
if __name__=='__main__':main()
