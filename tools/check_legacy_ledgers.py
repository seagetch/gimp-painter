"""Structural completion gates for the exhaustive legacy assignment/audit data."""
import re
from pathlib import Path
from assign_legacy_hunks import read_tsv, task_catalog, tsv
from audit_legacy_granularity import specification,validate,AUDIT_FIELDS
from audit_standard_paint import PATHS


def validate_ledgers(wbs,tasks_path,evidence_path):
    errors=[];inventory=evidence_path.parent/'inventory';catalog=task_catalog(tasks_path)
    def done(task):return wbs.get(task,(False,))[0]
    def read(name):
        path=inventory/name
        if not path.exists():raise ValueError('missing '+str(path))
        return read_tsv(path)
    def unique(rows,key,label):
        if len({r[key] for r in rows})!=len(rows):errors.append(label+': duplicate '+key)
    def task_refs(records,fields,label):
        for r in records:
            for field in fields:
                ids=r[field].split(',')
                if not ids or any(not re.fullmatch(r'\d{2}\.\d{3}(?:/[A-Za-z0-9_.-]+)?',x) or x not in catalog for x in ids):
                    errors.append(label+': non-concrete or missing WBS target at '+r.get('child_id',r.get('path','')))
    try:
        if not done('01.013'):return errors
        files={r['path']:r for r in read('changed-files.tsv')};hunks=read('changed-hunks.tsv')
        assignments=read('hunk-wbs.tsv');assets=read('asset-wbs.tsv');by_hunk={r['hunk_id']:r for r in assignments}
        unique(assignments,'hunk_id','01.013');unique(assignments,'child_id','01.013');unique(assets,'path','01.013')
        if len(assignments)!=2489 or set(by_hunk)!={h['child_id'] for h in hunks}:errors.append('01.013: hunk assignment set differs')
        for h in hunks:
            a=by_hunk.get(h['child_id'])
            if not a:continue
            if a['status']!='DONE' or a['path']!=h['path'] or a['old_new_lines']!=h['old_new_lines'] or a['source_blob']!=files[h['path']]['source_git_blob'] or not re.fullmatch('[a-f0-9]{64}',a['payload_sha256']):errors.append('01.013: source identity/completion mismatch '+h['child_id'])
            if h['disposition_task']!=a['implementation_tasks'] or h['feature_or_base']!=a['feature']:errors.append('01.013: original hunk assignment columns drift '+h['child_id'])
        expected_assets={p for p in files if p.startswith(('data/','themes/','menus/','etc/','po/')) or p.endswith('gimptitlebaricon-pixbuf.h')}
        if len(assets)!=402 or {r['path'] for r in assets}!=expected_assets or any(r['status']!='DONE' or r['source_blob']!=files[r['path']]['source_git_blob'] for r in assets):errors.append('01.013: asset assignment set differs')
        task_refs(assignments+assets,('implementation_tasks','verification_tasks'),'01.013')
        if done('01.014'):
            candidates=read('cleanup-candidate-review.tsv');unique(candidates,'path','01.014')
            if len(candidates)!=947 or {r['path'] for r in candidates}!=files.keys() or any(r['status']!='DONE' or not r['classification'] or not r['evidence'] or not r['disposition'] for r in candidates):errors.append('01.014: incomplete candidate classification')
            task_refs(candidates,('implementation_tasks','verification_tasks'),'01.014')
        if done('01.015'):
            scripts=read('auxiliary-script-review.tsv');relations=read('auxiliary-generator-relations.tsv');unique(scripts,'hunk_id','01.015')
            paths={r['path'] for r in scripts};expected={h['child_id'] for h in hunks if h['path'] in paths}
            if len(scripts)!=83 or len(paths)!=37 or {r['hunk_id'] for r in scripts}!=expected or len(relations)!=6 or any(r['status']!='DONE' or not r['target'] for r in scripts+relations):errors.append('01.015: script/generator coverage incomplete')
            task_refs(scripts,('implementation_tasks','verification_tasks'),'01.015');task_refs(relations,('followup',),'01.015')
        if done('01.016') or done('01.016/bucket-selection-source'):
            paint=read('standard-paint-hunk-review.tsv');unique(paint,'hunk_id','01.016');expected={h['child_id'] for h in hunks if h['path'] in PATHS}
            if len(paint)!=306 or {r['hunk_id'] for r in paint}!=expected or any(r['status']!='DONE' or r['equivalence_state']!='NOT_PROVEN' or not r['changed_evidence'] or not r['target_contract'] or r['payload_sha256']!=by_hunk[r['hunk_id']]['payload_sha256'] for r in paint):errors.append('01.016: exact source/target hunk checklist incomplete or overclaimed')
            task_refs(paint,('implementation_tasks','verification_tasks'),'01.016')
        if done('01.017'):
            expected,checks=specification(catalog,inventory);work=read('legacy-port-work-items.tsv')
            issues=validate(expected,work,done('38.016') or done('38.017'))
            errors.extend('01.017: '+e for e in issues[:20])
            if (inventory/'granularity-review.tsv').read_text()!=tsv(AUDIT_FIELDS,checks):errors.append('01.017: per-source granularity checklist drift')
    except (ValueError,KeyError,OSError) as e:errors.append('legacy inventory gate: '+str(e))
    return errors
