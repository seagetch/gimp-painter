#!/usr/bin/env python3
"""Exact 09.014 placement criterion, without absorbing later precision duties."""
import hashlib,json,pathlib
from check_drawable_buffer_read_resolution import load,require
ROOT=pathlib.Path(__file__).resolve().parents[1];MANIFEST=ROOT/'migration/contracts/color-placement.json'
def validate(m,args=None):
 _,inventory,tasks,progress=load()if args is None else args
 require(m['task']=='09.014' and m['criterion_status']=='DONE','Wrong criterion')
 require(m['original_action']=='色変換の配置を実装する' and m['original_acceptance']=='採色と合成で意図しない二重変換がない','Original criterion changed')
 require(m['dependencies']==['09.013'] and progress['09.013']['status']=='DONE','Missing dependency')
 require(tasks['09.014']==['[x]',m['original_action'],'09.013',m['original_acceptance']] and progress['09.014']['status']=='DONE','Ledger mismatch')
 require(m['duties']==[] and not any(r['wbs_task']=='09.014'for r in inventory.values()),'Unaccounted source duties')
 require(m['whole_legacy_feature_complete']is False,'Premature feature completion')
 expected={'09.015','09.016','09.017'};require(set(m['deferred_criteria'])==expected,'Lost later criteria')
 for k,v in m['deferred_criteria'].items():
  require(v==tasks[k][1]+'; '+tasks[k][3],'Later criterion changed')
  require('color-placement.json#'+k in progress[k]['commit_or_artifact'],'Missing reverse link')
 require(set(m['compatibility_policies'])=={'migration/contracts/mypaint-gray-surface.md','migration/contracts/mypaint-native-precision.md'},'Missing policy')
 for n,h in m['compatibility_policies'].items():require(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,'Changed byte/native color policy')
 require(m['evidence']==['migration/tests/color-placement/report.json'] and all((ROOT/p).is_file()for p in m['evidence']),'Missing evidence')
 return 0
if __name__=='__main__':print(f'{validate(json.loads(MANIFEST.read_text()))} direct source duties; original 09.014 conversion criterion verified')
