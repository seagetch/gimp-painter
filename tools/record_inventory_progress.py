#!/usr/bin/env python3
"""Update an inventory WBS row and its evidence together (no runtime claim)."""
import csv
from pathlib import Path

def done(task, artifact, check, result, limitation='Static source inventory; implementation and runtime tests remain in assigned functional WBS tasks'):
    p=Path('tasks.md'); text=p.read_text(); before=f'| {task} | [ ] |'; after=f'| {task} | [x] |'
    assert before in text or after in text, task
    p.write_text(text.replace(before,after))
    p=Path('migration/progress.tsv')
    with p.open() as f: r=csv.DictReader(f,delimiter='\t'); fields=r.fieldnames; rows=list(r)
    row=next((r for r in rows if r['id']==task),None)
    if row is None: row={'id':task}; rows.append(row)
    row.update(status='DONE',owner='Codex',commit_or_artifact=artifact,test=check,result=result,added_children=row.get('added_children','-'),limitations=limitation)
    with p.open('w') as f: w=csv.DictWriter(f,fields,delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('task');p.add_argument('artifact');p.add_argument('check');p.add_argument('result');a=p.parse_args();done(a.task,a.artifact,a.check,a.result)
