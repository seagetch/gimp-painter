#!/usr/bin/env python3
"""Original precision acceptance with unchanged inherited full scalar duties."""
import hashlib,json,pathlib,copy
from check_drawable_buffer_read_resolution import load,require
ROOT=pathlib.Path(__file__).resolve().parents[1];MANIFEST=ROOT/'migration/contracts/precision-refusal.json'
def validate(m,args=None):
 _,inventory,tasks,progress=load()if args is None else args
 require(m['task']=='09.015' and m['criterion_status']=='DONE','Wrong criterion')
 require(m['original_action']=='高精度・色空間変換の扱いを実装する' and m['original_acceptance']=='unsupported 時も情報を黙って欠落させない','Original criterion changed')
 require(m['dependencies']==['09.014'] and progress['09.014']['status']=='DONE','Missing dependency')
 require(tasks['09.015']==['[x]',m['original_action'],'09.014',m['original_acceptance']] and progress['09.015']['status']=='DONE','Ledger mismatch')
 require(m['direct_duties']==[] and not any(r['wbs_task']=='09.015'for r in inventory.values()),'Unaccounted direct duties')
 require(m['whole_legacy_feature_complete']is False,'Premature feature completion')
 expected=[]
 for name in ['drawable-buffer-write','tempbuf-conversion']:
  a=json.loads((ROOT/f'migration/contracts/{name}.json').read_text())
  for row in a['duties']:
   if '09.015'in row.get('successor_obligations',{}):
    d=copy.deepcopy(row);d['ancestor_contract']=name;d['inherited_09_015_obligation']=d['successor_obligations'].pop('09.015');d['whole_hunk_checkpoint_status']='TODO';expected.append(d)
 require(len(expected)==2 and m['inherited_duties']==expected,'Lost or changed inherited scalar duty')
 for d in m['inherited_duties']:
  require('31.017'in d['successor_obligations'] and d['whole_hunk_checkpoint_status']=='TODO','Whole scalar replacement not proven')
  require('precision-refusal.json#31.017'in progress['31.017']['commit_or_artifact'],'Missing reverse owner link')
  if inventory[d['work_id']]['status']=='DONE':require(progress['31.017']['status']=='DONE' and tasks['31.017'][0]=='[x]','Premature inventory completion')
 require(set(m['compatibility_policies'])=={'migration/contracts/mypaint-gray-surface.md','migration/contracts/mypaint-native-precision.md'},'Missing policy')
 for n,h in m['compatibility_policies'].items():require(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,'Changed color policy')
 require(m['evidence']==['migration/tests/precision-refusal/report.json'] and all((ROOT/p).is_file()for p in m['evidence']),'Missing evidence')
 return 2
if __name__=='__main__':print(f'{validate(json.loads(MANIFEST.read_text()))} inherited full scalar duties preserved; original09.015 verified')
