#!/usr/bin/env python3
"""Extract immutable Clone-only old model expectations; never execute old app."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[3]
source=root/'migration/fixtures/legacy-layer-operations/clone/capture.log'
rows=[];stages=[]
for line in source.read_text().splitlines():
 if line.startswith('STATE step='):stages.append(line.split()[1].split('=',1)[1])
 if not line.startswith('OBJECT '):continue
 fields=dict(re.findall(r'(\w+)=([^ ]+)',line));xy=fields.get('xy','0,0').split(',');wh=fields.get('wh','0,0').split(',')
 rows.append([fields['step'],fields['id'],fields['alive'],fields.get('attached','0'),fields.get('parent','NULL'),fields.get('index','-1'),*xy,*wh,fields.get('source','-')])
out=Path(__file__).parent
(out/'old-model.tsv').write_text('\n'.join('\t'.join(row) for row in rows)+'\n')
(out/'old-stages.txt').write_text('\n'.join(stages)+'\n')
assert len(stages)==30
files=[source,root/'migration/fixtures/legacy-layer-operations/capture-layer-operations.c']
files+=list((root/'migration/fixtures/legacy-layer-operations/clone').glob('*.pixels'))
(out/'provenance.json').write_text(json.dumps({'scope':'Clone-only old live operation capture, not old reader projection equivalence','stage_count':len(stages),'model_rows':len(rows),'sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},'old_runtime_executed':False},indent=2)+'\n')
print(len(stages),'stages',len(rows),'model rows')
