#!/usr/bin/env python3
"""Documentation/source trace only; no acquisition or native diagnostics."""
import argparse,copy,csv,hashlib,io,json,subprocess
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(t,original,native):
    assert len(t['duties'])==18 and t['status_changes'] is False
    assert len({x['work_id'] for x in t['duties']})==18
    assert {x['work_id'] for x in t['duties']}==set(original)
    for row in t['duties']:
        assert {k:v for k,v in row.items() if k!='mapping'}==original[row['work_id']]
        assert row['mapping']
        blob=(P/'legacy-blobs'/row['source_blob']).read_bytes()
        assert hashlib.sha1(b'blob '+str(len(blob)).encode()+b'\0'+blob).hexdigest()==row['source_blob']
    assert native['runtime_executed'] is False
    for p,pin in native['sha256'].items():assert sha((P/'native'/p).read_bytes())==pin
    editor=(P/'native/app/widgets/gimppaintermybrusheditor.cpp').read_text()
    for token in ['GimpContext *ctx () const { return options ? GIMP_CONTEXT (options.get ()) : nullptr; }','gimp_painter_mybrush_options_ref_for_context(context,&failure)','ObjectRef<GObject>::retain(G_OBJECT(context))','ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(GIMP_CONTEXT(model.get())))','if(c)gimp_painter_mybrush_editor_set_context(editor,c,nullptr);','g_object_force_floating(G_OBJECT(e))']:assert token in editor,token
    typed=(P/'native/app/widgets/gimppaintermybrusheditor.hpp').read_text();assert 'ObjectRef<GimpPainterMybrushEditor>::sink' in typed

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--negative-controls',action='store_true');a=ap.parse_args()
    for p,pin in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==pin,p
    t=json.loads((P/'source-duty-trace.json').read_text());n=json.loads((P/'native-source.json').read_text())
    data=subprocess.check_output(['git','show',t['commit']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R,text=True);orig={x['work_id']:x for x in csv.DictReader(io.StringIO(data),delimiter='\t') if x['wbs_task']=='24.001/editor-context-contract'}
    check(t,orig,n)
    spec=(R/'migration/contracts/mypaint-editor-context.md').read_bytes();assert sha(spec)==json.loads((P/'spec.json').read_text())['sha256']
    count=0
    if a.negative_controls:
        bad=copy.deepcopy(t);bad['duties'].pop();cases=[(bad,n)]
        bad=copy.deepcopy(t);bad['duties'][0]['acceptance']='weakened';cases.append((bad,n))
        bad=copy.deepcopy(n);bad['runtime_executed']=True;cases.append((t,bad))
        for x,y in cases:
            try:check(x,orig,y)
            except AssertionError:count+=1
        assert count==3
    print('PASS: specification trace, 18 unchanged original duties, committed source anchors; no runtime claim; controls',count)
if __name__=='__main__':main()
