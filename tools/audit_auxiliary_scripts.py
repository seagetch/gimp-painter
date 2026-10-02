#!/usr/bin/env python3
"""Audit all changed non-C++ scripts/build inputs and referenced generators."""
import argparse
import json
import re
import subprocess
import tempfile
from collections import Counter
from pathlib import Path
from assign_legacy_hunks import ROOT,INV,git,read_tsv,tsv,sha,task_catalog

FIELDS=('child_id','status','path','source_blob','hunk_id','range','classification','input_output_contract','target','implementation_tasks','verification_tasks','runtime_state')
REL_FIELDS=('child_id','status','source_path','source_blob','role','inputs','outputs','target','followup','finding')
SPECIAL={
 'data/mypaint-brushes/Makefile.am.skel':('ruby-generator','Glob each child directory for .myb and _prev.png, sort basenames, generate its Makefile.am','Current Meson/install manifest','30.017/brush-manifest-generator','34.013,34.014'),
 'data/mypaint-brushes/label-brush-mypaint.sh':('bash-imagemagick','For each PNG derive label, convert underscores, caption/border/resize/sharpen/flatten and overwrite that same PNG','Isolated asset-label generation preserving ImageMagick semantics','30.017/brush-preview-generator','34.013,34.014'),
 'tools/pdbgen/enums.pl':('generated-perl-enums','Adds legacy seven-mode export values to enum metadata, including scalar/wire values','pdb/enumgen.pl and pdb/meson.build','04.013,10.005,30.013','34.013,36.004'),
 'tools/pdbgen/groups.pl':('generated-perl-groups','Adds mypaint_brush_select group consumed by pdbgen','pdb/groups and pdb/meson.build','30.012,30.013','34.013'),
 'tools/pdbgen/pdb/mypaint_brush_select.pdb':('perl-pdb-source','Declares popup/close_popup/set_popup, argument nullability and app/lib export bodies','pdb/groups/*.pdb current internal/public PDB generator','30.012,30.013','24.017,34.013'),
 'plug-ins/script-fu/scheme-wrapper.c':('scheme-runtime-constant-table','Adds ERASE, REPLACE, ANTI-ERASE, SRC-IN, DST-IN, SRC-OUT, DST-OUT interpreter bindings','Current Script-Fu enum/constants registration with explicit meaning conversion','30.012/scheme-mode-constants','13.013,34.013,36.004'),
}
REFERENCES={
 'app/composite/make-installer.py':('unchanged-python-generator','Object-file function namespace via ns.py; ordered composite_modes','gimp-composite-*-installer.c','GEGL operation registration, preserving actual checked-in old dispatch','13.001/gegl-operation-route','The generator composite_modes omits SRC_IN/DST_IN/SRC_OUT/DST_OUT while checked-in generic installer has all four; do not regenerate away hand-maintained additions'),
 'tools/gimp-mkenums':('unchanged-perl-generator','Enum source headers and invocation templates','Registered GType value/name/description C files','tools/meson-mkenums.py and current header/template targets','04.013','Regenerate metadata from preserved authoritative declarations; do not port generated bodies as independent source'),
 'tools/pdbgen/enumgen.pl':('unchanged-perl-generator','enum_headers in legacy tools/pdbgen/Makefile.am','tools/pdbgen/enums.pl','pdb/enumgen.pl via pdb/meson-enumgen.py','04.013,30.013','The changed enums.pl is generated, but the mode meanings/numbers still require explicit preservation'),
 'tools/pdbgen/enumcode.pl':('unchanged-perl-generator','Generated enums.pl metadata','libgimp/gimpenums.h and enum Python constants','pdb/enumcode.pl via pdb/meson-enumcode.py','04.013,30.013','Preserve public semantic values without blindly retaining colliding integers'),
 'tools/pdbgen/pdbgen.pl':('unchanged-perl-generator','stddefs, pdb.pl/util.pl, groups.pl, app/lib code emitters and *.pdb group sources','app/pdb/*-cmds.c, internal-procs.*, libgimp/*_pdb.*, gimp_pdb_headers.h','pdb/pdbgen.pl via pdb/meson-pdbgen.py','30.012,30.013','Three custom MyPaint selection procedures must be registered and generated through the current source-of-truth'),
 'generate.py':('missing-named-generator','Unknown in pinned tree; mentioned by mypaintbrush-enum-settings.h','103 old setting constants and related setting tables','Restore an authoritative setting schema and generator','04.013/brush-setting-generator','No generate.py exists anywhere in the pinned source tree; absence is tracked, not interpreted as unused settings'),
}

def is_script(p,data):
    n=Path(p).name
    return n.startswith('Makefile.am') or Path(p).suffix in ('.sh','.pl','.pdb','.py','.rb','.scm','.ac','.yml','.patch') or p in SPECIAL or data.startswith(b'#!')

def render():
    baseline=json.loads((ROOT/'migration/baseline/baseline.json').read_text());source=baseline['source']['commit']
    files=read_tsv(INV/'changed-files.tsv');hunks=read_tsv(INV/'hunk-wbs.tsv');by={}
    for h in hunks:by.setdefault(h['path'],[]).append(h)
    selected=[];out=[]
    for f in files:
        p=f['path'];data=git('show',f['source_git_blob'])
        if not is_script(p,data):continue
        selected.append(p)
        if p in SPECIAL:kind,contract,target,tasks,tests=SPECIAL[p]
        elif p.endswith('.yml') or p.endswith('.patch'):
            kind='packaging-recipe-or-patch';contract='Retain pinned dependencies, build commands and patch applicability as reproducibility evidence';target='Current per-OS package recipe';tasks='32.001,33.001,34.005,34.005/license-manifest';tests='32.012,33.009,34.002'
        elif p=='configure.ac':
            kind='autoconf-feature-input';contract='C++ compiler, standard, optional HTTP/web dependencies and source registration';target='Meson feature/compiler/dependency configuration';tasks='04.002,04.008,04.010,31.003';tests='04.015,31.009'
        elif Path(p).name=='Makefile.am':
            kind='automake-build-or-install-input';contract='Each changed source/library/generated target and install-list entry';target='Corresponding current Meson module/install target';tasks=','.join(sorted({t for h in by[p] for t in h['implementation_tasks'].split(',')}));tests=','.join(sorted({t for h in by[p] for t in h['verification_tasks'].split(',')}))
        else:raise ValueError('Unclassified auxiliary executable/input '+p)
        for h in by[p]:out.append(dict(child_id=h['hunk_id'].replace('01.002/','01.015/hunk-'),status='DONE',path=p,source_blob=f['source_git_blob'],hunk_id=h['hunk_id'],range=h['old_new_lines'],classification=kind,input_output_contract=contract,target=target,implementation_tasks=tasks,verification_tasks=tests,runtime_state='NOT_PORTED'))
    tree={}
    for e in git('ls-tree','-rz',source).split(b'\0'):
        if e:m,p=e.split(b'\t',1);tree[p.decode()]=m.split()[2].decode()
    relations=[]
    for p,info in REFERENCES.items():
        kind,inputs,outputs,target,follow,finding=info
        if p=='generate.py':
            if any(Path(n).name=='generate.py' for n in tree):raise ValueError('Missing-generator assumption changed')
            blob='ABSENT_FROM_PINNED_TREE'
        else:
            blob=tree[p]
            if p=='app/composite/make-installer.py':
                generator=git('show',blob).decode();installer=git('show',tree['app/composite/gimp-composite-generic-installer.c']).decode()
                for key in ('SRC_IN','DST_IN','SRC_OUT','DST_OUT'):
                    assert 'GIMP_COMPOSITE_'+key not in generator
                    assert 'GIMP_COMPOSITE_'+key in installer
        relations.append(dict(child_id=f'01.015/generator-{len(relations)+1:02d}',status='DONE',source_path=p,source_blob=blob,role=kind,inputs=inputs,outputs=outputs,target=target,followup=follow,finding=finding))
    assert len(selected)==37,selected
    assert not any(p.endswith(('.py','.scm')) for p in selected),'Revisit source-language scope'
    catalog=task_catalog()
    for r in out:
        if set((r['implementation_tasks']+','+r['verification_tasks']).split(','))-catalog.keys():raise ValueError('Undefined WBS route')
    return tsv(FIELDS,out),tsv(REL_FIELDS,relations),len(out)

def syntax_checks():
    source=json.loads((ROOT/'migration/baseline/baseline.json').read_text())['source']['commit']
    with tempfile.TemporaryDirectory(prefix='painter-script-syntax-') as tmp:
        for p in ('data/mypaint-brushes/label-brush-mypaint.sh','tools/pdbgen/enums.pl','tools/pdbgen/groups.pl','tools/pdbgen/pdb/mypaint_brush_select.pdb'):
            f=Path(tmp)/Path(p).name;f.write_bytes(git('show',source+':'+p))
            cmd=['bash','-n',str(f)] if p.endswith('.sh') else ['perl','-c',str(f)]
            subprocess.run(cmd,check=True,capture_output=True)
    print('Bash syntax and three Perl source/generated-input syntax checks: PASS (no asset mutation)')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--check',action='store_true');ap.add_argument('--syntax',action='store_true');a=ap.parse_args()
    first,second,count=render()
    for name,content in [('auxiliary-script-review.tsv',first),('auxiliary-generator-relations.tsv',second)]:
        p=INV/name
        if a.check:
            if not p.exists() or p.read_text()!=content:raise SystemExit('Auxiliary inventory drift: '+name)
        else:p.write_text(content)
    if a.syntax:syntax_checks()
    print(f'37 changed script/build-input paths; {count} hunks; 6 generator relationships; all assigned')
if __name__=='__main__':main()
