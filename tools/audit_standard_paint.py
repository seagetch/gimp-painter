#!/usr/bin/env python3
"""Audit every standard paint/context/dynamics/bucket delta against pinned GIMP 3."""
import argparse
import json
import re
from collections import Counter
from pathlib import Path
from assign_legacy_hunks import ROOT,INV,git,read_tsv,tsv,patch_sections,sha,task_catalog

PATHS=set('''app/actions/context-commands.c
app/core/core-enums.c app/core/core-enums.h
app/tools/gimpsmudgetool.c
app/tools/gimpdynamicsoptions-gui.c app/tools/gimpdynamicsoptions-gui.h
app/tools/gimpbrushoptions-gui.c app/tools/gimpbrushoptions-gui.h
app/core/gimpcontext.c app/core/gimpcontext.h
app/core/gimpdrawable-bucket-fill.c app/core/gimpimage-contiguous-region.c app/core/gimpimage-contiguous-region.h
app/core/gimpdynamics.c app/core/gimpdynamics.h app/core/gimpdynamicsoutput.h
app/paint/gimpbrushcore.c app/paint/gimpbrushcore.h
app/paint/gimppaintcore.c app/paint/gimppaintcore.h
app/paint/gimppaintoptions.c app/paint/gimppaintoptions.h
app/paint/gimpsmudge.c app/paint/gimpsmudge.h app/paint/gimpsmudgeoptions.c app/paint/gimpsmudgeoptions.h
app/paint/gimpink.c app/paint/gimpink.h
app/tools/gimppainttool.c app/tools/gimppaintbrushtool.c
app/tools/gimppaintoptions-gui.c app/tools/gimppaintoptions-gui.h
app/widgets/gimpdynamicsoutputeditor.c app/widgets/gimpdeviceinfo-coords.c app/widgets/gimpdevicemanager.c
app/display/gimpmotionbuffer.c'''.split())
FIELDS=('child_id','status','hunk_id','path','source_blob','source_range','payload_sha256','source_scope','changed_evidence','feature','implementation_tasks','verification_tasks','target_commit','target_path','target_blob','target_contract','equivalence_state')
TARGETS={
 'app/core/gimpimage-contiguous-region.c':('app/core/gimppickable-contiguous-region.cc','Current GEGL seed traversal has no source-mask input; add bounded sampling and old penalty semantics, preserving transparency/offset rules'),
 'app/core/gimpimage-contiguous-region.h':('app/core/gimppickable-contiguous-region.h','Current seed API lacks explicit fixed start color, source-mask buffer and rectangle inputs'),
 'app/core/gimpdrawable-bucket-fill.c':('app/core/gimpdrawable-bucket-fill.c','Current seed traversal occurs before selection bounds/data intersection; old source_mask affects traversal inside selection bounds'),
 'app/core/gimpdynamics.c':('app/core/gimpdynamics.c','Old blending-output is the independent Smudge extension; do not equate it with current Flow/Rate or paper settings'),
 'app/core/gimpcontext.c':('app/core/gimpcontext.c','Current mybrush resource path must retain extended custom brush state/name and thaw/removal/copy semantics; template rename is C++ glue'),
 'app/widgets/gimpdevicemanager.c':('app/widgets/gimpdevicemanager.c','Device container unique-name policy affects restored device identity; compare duplicate names and saved state'),
}

def render():
    baseline=json.loads((ROOT/'migration/baseline/baseline.json').read_text());source=baseline['source']['commit'];base=baseline['source_diff_base']['commit'];target=baseline['initial_port']['upstream_commit']
    sections=patch_sections(base,source);routes={r['hunk_id']:r for r in read_tsv(INV/'hunk-wbs.tsv')}
    tree={}
    for e in git('ls-tree','-rz',target).split(b'\0'):
        if e:m,p=e.split(b'\t',1);tree[p.decode()]=m.split()[2].decode()
    result=[]
    for h in read_tsv(INV/'changed-hunks.tsv'):
        p=h['path']
        if p not in PATHS:continue
        sec=sections[(p,h['index'])];r=routes[h['child_id']]
        changed=[('+'+l.strip()) for l in sec['added'] if l.strip()]+[('-'+l.strip()) for l in sec['removed'] if l.strip()]
        evidence=' | '.join(changed)
        # Keep complete evidence for every changed line, including inactive
        # branches, wrappers and whitespace-only hunks (explicit marker).
        evidence=evidence or 'Whitespace-only source delta; no active token change'
        tp,contract=TARGETS.get(p,(p,'Use the named current source with this separately assigned legacy contract; same path/feature name is not equivalence evidence'))
        if tp not in tree:
            contract='New/recreated component required at '+tp+'; port into the shared GTK3/core contracts named by the implementation tasks'
        result.append(dict(child_id=h['child_id'].replace('01.002/','01.016/hunk-'),status='DONE',hunk_id=h['child_id'],path=p,source_blob=r['source_blob'],source_range=h['old_new_lines'],payload_sha256=r['payload_sha256'],source_scope=sec['scope'],changed_evidence=evidence,feature=r['feature'],implementation_tasks=r['implementation_tasks'],verification_tasks=r['verification_tasks'],target_commit=target,target_path=tp,target_blob=tree.get(tp,'PATH_ABSENT'),target_contract=contract,equivalence_state='NOT_PROVEN'))
    actual={r['path'] for r in result}
    if actual!=PATHS:raise ValueError('Missing standard-paint paths: '+str(PATHS-actual))
    if len({r['hunk_id'] for r in result})!=len(result):raise ValueError('Duplicate hunk')
    # The traversal input was added in the caller and removed from its old
    # final-mask-only location. Verify source witnesses before publishing.
    old=git('show',source+':app/core/gimpdrawable-bucket-fill.c').decode()
    assert 'gimp_image_contiguous_region_by_seed_ext (image, drawable, src_mask,' in old
    assert '#if 0\n      if (selection)' in old
    new=git('show',target+':app/core/gimpdrawable-bucket-fill.c').decode()
    assert new.index('new_mask = gimp_pickable_contiguous_region_by_seed') < new.index('If there is a selection, intersect the region bounds')
    search=git('show',target+':app/core/gimppickable-contiguous-region.cc').decode()
    signature=search.split('gimp_pickable_contiguous_region_by_seed (',1)[1].split('{',1)[0]
    assert 'source_mask' not in signature and 'src_mask' not in signature
    return tsv(FIELDS,result),len(result)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--check',action='store_true');a=ap.parse_args();text,count=render();p=INV/'standard-paint-hunk-review.tsv'
    if a.check:
        if not p.exists() or p.read_text()!=text:raise SystemExit('Standard-paint audit drift')
    else:p.write_text(text)
    print(f'{count} exact hunk checks across {len(PATHS)} standard paint/context/input paths; no behavioral equivalence asserted')
if __name__=='__main__':main()
