#!/usr/bin/env python3
"""Replay bounded named-entry receipts; never executes native/held diagnostics."""
import argparse,copy,hashlib,json,subprocess,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent
R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def verify(receipt,sources,manifest,logs):
    assert receipt['normal']['cases']==receipt['asan']['cases']==3
    assert receipt['normal']['exit']==receipt['asan']['exit']==0
    assert receipt['asan']['instrumented_translation_units']==7
    assert receipt['compiled_negative_controls']==5
    assert receipt['current_HEAD_aggregate'] is False
    assert receipt['old_edit_active_session_parity'] is False
    assert receipt['held_diagnostics_run'] is False
    assert receipt['xvfb']=={'width':1080,'height':1920,'depth':24,'oncanvas':True,'input':'API-activated menu action'}
    assert set(sources)==set(manifest)
    for name,blob in sources.items():assert sha(blob)==manifest[name],name
    for path,pin in receipt['owned_sha256'].items():assert sha(sources['candidate/'+path])==pin,path
    names=['constructor','menu-save','menu-lifetime']
    for log in logs:
        ok=[line for line in log.splitlines() if line.startswith('ok ')]
        assert len(ok)==3 and 'not ok' not in log
        for name in names:assert any(line.endswith('/named-entry/'+name) for line in ok),name
    editor=sources['candidate/app/widgets/gimppaintermybrusheditor.cpp'].decode()
    action=sources['candidate/app/actions/mypaint-brush-editor-actions.c'].decode()
    for token in ['gimp_mypaint_brush_editor_new (GimpContext *context, GimpMenuFactory *factory)',
                  'gimp_editor_create_menu (GIMP_EDITOR (widget), factory, "<MypaintBrushEditor>"',
                  'auto factory = std::move (named_menu_factory);',
                  'if (group && group->user_data == owner) group->user_data = nullptr;',
                  'gimp_menu_factory_delete_manager (GIMP_MENU_FACTORY (factory.get ()),'
                  ]:assert token in editor,token
    assert 'g_signal_connect_object (gimp_action_group_get_action' in action
    assert '"activate", G_CALLBACK (save_command)' in action
    assert 'data_editor_edit_active_cmd_callback' not in action

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--commit');parser.add_argument('--negative-controls',action='store_true');args=parser.parse_args()
    artifacts=json.loads((P/'artifact-sha256.json').read_text())
    for name,pin in artifacts.items():assert sha((P/name).read_bytes())==pin,name
    with tarfile.open(P/'sources.tar.gz','r:gz') as archive:
        sources={member.name:archive.extractfile(member).read() for member in archive.getmembers() if member.isfile()}
    receipt=json.loads((P/'receipts.json').read_text());manifest=json.loads((P/'source-manifest.json').read_text());logs=[(P/name).read_text() for name in ['normal.log','asan.log']]
    verify(receipt,sources,manifest,logs)
    assert json.loads((P/'linked-before.json').read_text())==json.loads((P/'linked-after.json').read_text())
    for mutant in json.loads((P/'mutants.json').read_text()):
        assert mutant['returncode']!=0
        assert mutant['expected'] in (P/'mutants'/(mutant['name']+'.log')).read_text()
    headers=json.loads((P/'headers.json').read_text());assert headers['native_C_symbol'] is True
    assert [r['language'] for r in headers['checks']]==['c','cpp']
    assert all(r['returncode']==0 for r in headers['checks'])
    for path,pin in receipt['owned_sha256'].items():
        blob=subprocess.check_output(['git','show',args.commit+':'+path],cwd=R) if args.commit else (R/path).read_bytes()
        assert sha(blob)==pin,('current owned source drift',path)
    rejected=0
    if args.negative_controls:
        cases=[]
        bad=copy.deepcopy(receipt);bad['normal']['cases']=4;cases.append((bad,sources,manifest,logs))
        bad=dict(sources);key='candidate/app/widgets/gimppaintermybrusheditor.cpp';bad[key]=bad[key]+b'\n/* drift */\n';cases.append((receipt,bad,manifest,logs))
        bad=list(logs);bad[0]=bad[0].replace('ok 3 /painter-editor/named-entry/menu-lifetime','not ok 3 /painter-editor/named-entry/menu-lifetime');cases.append((receipt,sources,manifest,bad))
        for case in cases:
            try:verify(*case)
            except AssertionError:rejected+=1
        assert rejected==len(cases)
    print('PASS: native 3, scoped sanitizer 3 (7 units), 5 compiled negatives, C11/C++14; %d replay controls; predecessor-only integration'%rejected)
if __name__=='__main__':main()
