#!/usr/bin/env python3
import argparse,hashlib,json,subprocess,sys,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def verify(t,sources,logs):
    assert set(sources)==set(t['source_sha256'])
    for p,pin in t['source_sha256'].items():assert sha(sources[p])==pin,p
    assert t['normal']==t['sanitizer']==3 and t['instrumented_units']==7 and t['fresh_instrumented_units']==2
    assert not t['current_HEAD_aggregate'] and not t['readiness_runtime_verified'] and not t['held_diagnostics_run']
    for log in logs:
        assert len([x for x in log.splitlines() if x.startswith('ok ')])==3 and 'not ok' not in log
        assert 'outside notifies52 / inside1' in log and 'restores displayed6' in log
    cpp=sources[t['changed_path']].decode()
    for token in ['if(value<metadata.minimum||value>metadata.maximum)','auto resource=i.snapshot();resource.set_base_value(n,value);','gimp_painter_mybrush_options_set_json(model.get(),resource.encode().c_str(),&error)','else g_object_set(model.get(),key.c_str(),value,nullptr);','!i.closed&&i.ui_revision==revision&&i.options.get()==model.get()','SyncScope sync(i);gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget.get()),current);']:assert token in cpp

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--commit');ap.add_argument('--negative-controls',action='store_true');a=ap.parse_args()
    for p,pin in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==pin,p
    t=json.loads((P/'transition.json').read_text());prior=P.parent/'editor-type-alias'
    assert sha((prior/'artifact-sha256.json').read_bytes())==t['predecessor_manifest_sha256']
    subprocess.run([sys.executable,str(prior/'validate.py'),'--commit',t['base']],check=True)
    src={p:subprocess.check_output(['git','show',a.commit+':'+p],cwd=R) if a.commit else (R/p).read_bytes() for p in t['source_sha256']};logs=[(P/p).read_text() for p in ['green.log','asan.log']];verify(t,src,logs)
    m=t['independent_metadata_transition'];oldmeta=subprocess.check_output(['git','show',t['base']+':'+m['path']],cwd=R);newmeta=subprocess.check_output(['git','show',m['commit']+':'+m['path']],cwd=R)
    assert sha(oldmeta)==m['before_sha256'] and sha(newmeta)==m['after_sha256']
    assert newmeta==oldmeta+m['exact_append'].encode() and src[m['path']]==newmeta
    assert (P/'potfiles-before.txt').read_bytes()==oldmeta and (P/'potfiles-after.txt').read_bytes()==newmeta
    assert (P/'before.cpp').read_bytes()==subprocess.check_output(['git','show',t['base']+':'+t['changed_path']],cwd=R)
    assert (P/'after.cpp').read_bytes()==src[t['changed_path']]
    before=(P/'before.cpp').read_text();after=(P/'after.cpp').read_text();old='connect(i,w,"value-changed",G_CALLBACK(simple_cb),[n,key](EditorImpl&i){g_object_set(i.options.get(),key.c_str(),gtk_spin_button_get_value(GTK_SPIN_BUTTON(i.numeric[n])),nullptr);});}'
    left,right=before.split(old);assert after.startswith(left) and after.endswith(right)
    assert int((P/'numeric-red.exit').read_text())!=0 and 'displayed=5.9000000000000004 model=6' in (P/'numeric-red.log').read_text()
    assert int((P/'mutant.exit').read_text())!=0 and 'assertion failed (gtk_spin_button_get_value(w) == 6): (5.9 == 6)' in (P/'mutant.log').read_text()
    assert int((P/'green.exit').read_text())==int((P/'asan.exit').read_text())==0
    assert json.loads((P/'linked-before.json').read_text())==json.loads((P/'linked-after.json').read_text())
    with tarfile.open(P/'sources.tar.gz') as ar:
        for p,pin in json.loads((P/'dependency-sources.json').read_text()).items():assert sha(ar.extractfile(p.lstrip('/')).read())==pin
    n=0
    if a.negative_controls:
        for p in [t['changed_path'],'app/actions/mypaint-brush-editor-actions.c','app/widgets/gimppaintermybrusheditor.hpp','po/POTFILES.in']:
            bad=dict(src);bad[p]+=b'\n/* drift */'
            try:verify(t,bad,logs)
            except AssertionError:n+=1
        assert n==4
    print('PASS: native3/scoped sanitizer3, exact numeric source successor, red+compiled rollback mutant; controls',n)
if __name__=='__main__':main()
