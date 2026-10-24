#!/usr/bin/env python3
"""Exact getter successor; immutable predecessor replay, no runtime diagnostics."""
import argparse,hashlib,json,subprocess,sys,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent; R=P.parents[2]; OLD=P.parent/'named-editor-entry'
def sha(b):return hashlib.sha256(b).hexdigest()
def validate_sources(old,current,pins):
    assert set(old)==set(current)
    changed={p for p in old if old[p]!=current[p]}
    assert changed==set(pins)
    for p in changed:assert sha(current[p])==pins[p],p
    p='app/widgets/gimppaintermybrusheditor.cpp'
    insert=b'GType gimp_mypaint_brush_editor_get_type (void)\n{\n  return gimp_painter_mybrush_editor_get_type ();\n}\n'
    assert current[p]==old[p].replace(b'namespace {\nconstexpr int preview_width',insert+b'namespace {\nconstexpr int preview_width')
    p='app/widgets/gimppaintermybrusheditor.hpp'
    assert current[p]==old[p].replace(b'return GIMP_TYPE_PAINTER_MYBRUSH_EDITOR;',b'return gimp_mypaint_brush_editor_get_type ();')
    p='app/widgets/gimpmypaintbrusheditor.hpp'
    assert current[p]==old[p].replace(b'#include "gimppaintermybrusheditor.hpp"',b'#include "gimpmypaintbrusheditor.h"\n#include "gimppaintermybrusheditor.hpp"')
    p='app/widgets/gimppaintermybrusheditor.h'
    anchor=b'GType gimp_painter_mybrush_editor_get_type (void) G_GNUC_CONST;'
    assert current[p]==old[p].replace(anchor,anchor+b'\n/* Named compatibility getter: the same native runtime type, never a second registration. */\nGType gimp_mypaint_brush_editor_get_type (void) G_GNUC_CONST;')
    compat=current['app/widgets/gimpmypaintbrusheditor.h']
    assert b'typedef struct _GimpPainterMybrushEditor GimpMypaintBrushEditor;' in compat
    assert b'Class' not in compat and b'GIMP_DATA_EDITOR' not in compat and b'gimppaintermybrusheditor.h' not in compat

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--commit');ap.add_argument('--negative-controls',action='store_true');a=ap.parse_args()
    artifacts=json.loads((P/'artifact-sha256.json').read_text())
    for p,pin in artifacts.items():assert sha((P/p).read_bytes())==pin,p
    t=json.loads((P/'transition.json').read_text())
    assert sha((OLD/'artifact-sha256.json').read_bytes())==t['predecessor_manifest_sha256']
    subprocess.run([sys.executable,str(OLD/'validate.py'),'--commit',t['base']],check=True)
    receipt=json.loads((OLD/'receipts.json').read_text());old={}
    with tarfile.open(OLD/'sources.tar.gz') as ar:
        for p in receipt['owned_sha256']:old[p]=ar.extractfile('candidate/'+p).read()
    current={p:subprocess.check_output(['git','show',a.commit+':'+p],cwd=R) if a.commit else (R/p).read_bytes() for p in old}
    for p,pin in t['predecessor_source_sha256'].items():assert sha(old[p])==pin
    validate_sources(old,current,t['source_sha256'])
    build=json.loads((P/'build.json').read_text());assert build['unmangled_symbol']
    assert [x['returncode']==0 for x in build['compile']]==[True,True,True,False,False]
    log=(P/'run.log').read_text();assert 'C11 alias-first PASS' in log and 'C++14 native-first alias TypeTraits PASS' in log
    assert len([x for x in log.splitlines() if x.startswith('ok ')])==3 and 'not ok' not in log
    mutant=json.loads((P/'mutant.json').read_text());assert mutant['returncode']!=0 and 'a == b' in mutant['stdout']+mutant['stderr']
    assert json.loads((P/'linked-before.json').read_text())==json.loads((P/'linked-after.json').read_text())
    n=0
    if a.negative_controls:
        for p in ['app/widgets/gimppaintermybrusheditor.cpp','app/actions/mypaint-brush-editor-actions.c','app/widgets/gimpmypaintbrusheditor.h']:
            bad=dict(current);bad[p]+=b'\n/* unadmitted drift */\n'
            try:validate_sources(old,bad,t['source_sha256'])
            except AssertionError:n+=1
        assert n==3
    print('PASS: sole native GType, two initialization orders, C11/C++14, ordinary native3, compile negatives2, runtime mutant1; successor controls',n)
if __name__=='__main__':main()
