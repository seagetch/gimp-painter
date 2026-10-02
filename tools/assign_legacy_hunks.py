#!/usr/bin/env python3
"""Reproduce and validate legacy hunk/asset -> concrete WBS assignments.

The DONE state is assignment work only. Implementation stays TODO in the
separate work-item checklist; no source match or assignment proves equivalence.
"""
import argparse
import csv
import hashlib
import io
import json
import re
import subprocess
from collections import Counter
from pathlib import Path
from legacy_assignment_rules import route, path_profile, PROFILES

ROOT = Path(__file__).resolve().parents[1]
INV = ROOT / 'migration/inventory'
TASK_ID = re.compile(r'\d{2}\.\d{3}(?:/[A-Za-z0-9_.-]+)?')
HUNK = re.compile(r'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$')
ASSIGN_FIELDS = ('child_id','status','hunk_id','path','source_blob','base_blob',
                 'old_new_lines','payload_sha256','profile','feature',
                 'implementation_tasks','verification_tasks','disposition',
                 'source_scope','reason','implementation_state')
ASSET_FIELDS = ('child_id','status','path','source_blob','base_blob','profile',
                'implementation_tasks','verification_tasks','preservation','implementation_state')

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def read_tsv(path):
    with path.open(newline='',encoding='utf-8') as f:
        return list(csv.DictReader(f,delimiter='\t'))

def tsv(fields, rows):
    out=io.StringIO(); w=csv.DictWriter(out,fields,delimiter='\t',lineterminator='\n')
    w.writeheader(); w.writerows(rows); return out.getvalue()

def task_catalog():
    result={}
    for line in (ROOT/'tasks.md').read_text().splitlines():
        if not re.match(r'^\| \d{2}\.\d{3}',line):continue
        cells=[c.strip() for c in line.split('|')[1:-1]]
        if len(cells)==5: result[cells[0]]=dict(title=cells[2], acceptance=cells[4])
    return result

def patch_sections(base,source):
    patch=git('-c','core.quotePath=false','diff','--no-renames','--no-ext-diff',
              '--no-textconv','--no-color','--unified=0',base,source)
    sections={}; path=None; number=0; section=None; meta=[]
    def end_file():
        if path and not number:
            sections[(path,'1')]=dict(header='no textual hunk',kind='metadata-or-binary',
                added=[],removed=[],payload='\n'.join(meta).encode(),scope='file metadata/binary section')
    for raw in patch.splitlines(keepends=True):
        line=raw.decode('utf-8','surrogateescape').rstrip('\n')
        if line.startswith('diff --git a/'):
            end_file(); path=line[13:].split(' b/',1)[0];number=0;section=None;meta=[line]
        elif line.startswith('@@ '):
            m=HUNK.fullmatch(line)
            if not m:raise ValueError('Malformed hunk '+line)
            number+=1
            old=f'-{m[1]}'+(','+m[2] if m[2] is not None else '')
            new=m[3]+(','+m[4] if m[4] is not None else '')
            section=dict(header=old+' -> '+new,kind='text',added=[],removed=[],payload=b'',scope=m[5].strip() or 'file-level declarations/content')
            sections[(path,str(number))]=section
        elif section is not None and line[:1] in ('+','-') and not line.startswith(('+++','---')):
            section['added' if line[0]=='+' else 'removed'].append(line[1:])
            section['payload']+=raw
        elif section is None: meta.append(line)
    end_file(); return sections

def build_outputs(validate_tasks=True):
    baseline=json.loads((ROOT/'migration/baseline/baseline.json').read_text())
    source=baseline['source']['commit'];base=baseline['source_diff_base']['commit']
    paths=read_tsv(INV/'changed-files.tsv');files={r['path']:r for r in paths}
    hunks=read_tsv(INV/'changed-hunks.tsv');sections=patch_sections(base,source)
    for role, revision in [('source', source), ('base', base)]:
        tree={}
        for entry in git('ls-tree','-rz',revision).split(b'\0'):
            if entry:
                meta,name=entry.split(b'\t',1); tree[name.decode()]=meta.split()[2].decode()
        for f in paths:
            if f[role+'_git_blob'] != tree.get(f['path'],''):
                raise ValueError('Pinned '+role+' blob drift: '+f['path'])
    expected={(r['path'],r['index']) for r in hunks}
    if set(sections)!=expected or len(hunks)!=2489 or len(files)!=947:
        raise ValueError('Fixed 2489 hunk/947 path inventory no longer equals source diff')
    assignments=[]; updated=[]
    catalog=task_catalog()
    for h in hunks:
        sec=sections[(h['path'],h['index'])]
        if h['old_new_lines']!=sec['header'] or h['kind']!=sec['kind']:
            raise ValueError('Source range/kind drift at '+h['child_id'])
        r=route(h,sec['added'],sec['removed']); f=files[h['path']]
        if not r['tasks'] or not r['tests']:raise ValueError('Empty route '+h['child_id'])
        if validate_tasks:
            missing=(set(r['tasks'])|set(r['tests']))-catalog.keys()
            if missing:raise ValueError('Undefined WBS action(s): '+','.join(sorted(missing)))
        assignments.append(dict(child_id=h['child_id'].replace('01.002/','01.013/hunk-'),
            status='DONE',hunk_id=h['child_id'],path=h['path'],source_blob=f['source_git_blob'],
            base_blob=f['base_git_blob'],old_new_lines=h['old_new_lines'],payload_sha256=sha(sec['payload']),
            profile=r['profile'],feature=r['feature'],implementation_tasks=','.join(r['tasks']),
            verification_tasks=','.join(r['tests']),disposition='retain-contract-and-port-or-prove-replacement',
            source_scope=sec['scope'],reason=r['reason'],implementation_state='TODO'))
        h=dict(h);h['feature_or_base']=r['feature'];h['disposition_task']=','.join(r['tasks'])
        h['result']='Enumerated and assigned; see hunk-wbs.tsv; implementation and behavioral verification remain TODO'
        updated.append(h)
    assets=[]
    for f in paths:
        p=f['path']
        if not (p.startswith(('data/','themes/','menus/','etc/','po/')) or p.endswith('gimptitlebaricon-pixbuf.h')):continue
        key=path_profile(p);r=PROFILES[key]
        assets.append(dict(child_id=f'01.013/asset-{len(assets)+1:04d}',status='DONE',path=p,
            source_blob=f['source_git_blob'],base_blob=f['base_git_blob'],profile=key,
            implementation_tasks=','.join(r['tasks']),verification_tasks=','.join(r['tests']),
            preservation=r['reason'],implementation_state='TODO'))
    if {r['path'] for r in assignments}!=files.keys():raise ValueError('Path coverage incomplete')
    hunk_text=tsv(tuple(hunks[0]),updated)
    summary=dict(source=source,base=base,hunks=len(assignments),changed_paths=len(files),
        assets_and_manifests=len(assets),unassigned_hunks=0,unassigned_assets=0,
        assignment_state='assigned; implementation and behavioral verification remain TODO',
        profiles=dict(sorted(Counter(r['profile'] for r in assignments).items())),
        hunk_ledger_sha256=sha(tsv(ASSIGN_FIELDS,assignments).encode()),
        asset_ledger_sha256=sha(tsv(ASSET_FIELDS,assets).encode()))
    return {
        INV/'changed-hunks.tsv':hunk_text,
        INV/'changed-hunks.json':json.dumps(dict(hunks_or_binary_sections=len(hunks),changed_paths=len(files),
            inventory_sha256=sha(hunk_text.encode()),assignment_state=summary['assignment_state']),indent=2)+'\n',
        INV/'hunk-wbs.tsv':tsv(ASSIGN_FIELDS,assignments),
        INV/'asset-wbs.tsv':tsv(ASSET_FIELDS,assets),
        INV/'wbs-assignment-summary.json':json.dumps(summary,indent=2)+'\n',
    }

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args();outputs=build_outputs()
    for path,content in outputs.items():
        if args.check:
            if not path.exists() or path.read_text()!=content:raise SystemExit('Assignment drift: '+str(path.relative_to(ROOT)))
        else:path.write_text(content)
    summary=json.loads(outputs[INV/'wbs-assignment-summary.json'])
    print(f"{summary['hunks']} hunks, {summary['changed_paths']} paths, {summary['assets_and_manifests']} assets/manifests; zero unassigned; implementation remains TODO")
if __name__=='__main__':main()
