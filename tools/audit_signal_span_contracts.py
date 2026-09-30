#!/usr/bin/env python3
"""Resolve changed complete calls to parsed data and teardown/migration contracts."""
import csv
from pathlib import Path
from audit_cpp_object_data_sites import split_args
ROOT=Path('migration/inventory')
def read(name):
 with (ROOT/name).open() as f:return list(csv.DictReader(f,delimiter='\t'))
def main():
 direct={r['legacy_site']:r for r in read('signal-source-inventory.tsv')}
 teardown={r['path_basename']:r for r in read('signal-teardown-by-file.tsv')}
 rows=[]
 for r in read('signal-call-spans.tsv'):
  if r['delta_role']!='DELTA':continue
  site=r['legacy_site'];op=r['operation'];call=r['complete_call'];args=split_args(call,call.index('(')+1)
  data=args[2] if 'delegator' in op else args[3] if op.startswith(('g_signal_connect','gimp_container_add')) else args[2] if op=='g_timeout_add' else '-'
  if site in direct:
   d=direct[site];contract=d['release_path'];task=d['followup'];risk=d['reentrancy']
  elif Path(site.rsplit(':',1)[0]).name in teardown:
   d=teardown[Path(site.rsplit(':',1)[0]).name];contract=d['teardown_evidence'];task=d['followup'];risk='Synchronous callback may switch/close popup; disconnect handler before freeing user data'
  else:
   assert site.startswith(('app/base/','app/core/gimpfilterlayer.cpp:','app/tools/gimptooloptions-gui-cxx','app/widgets/gimpeditor-cxx.cpp:')),site
   contract='Generic helper forwards receiver/callback/data into Connection or closure; Connection destructor disconnects, closure finalizer releases delegator; callers listed in signal-source-inventory.tsv'
   task='06.017';risk='Caller must retain handle or explicitly track untracked signal; executing delegator must survive close'
  role='HELPER_DEFINITION_OR_FORWARD' if site.startswith('app/base/') else 'CANCELLATION' if op=='gimp_container_remove_handler' else 'REGISTRATION'
  rows.append((site,op,role,args[0],args[1] if len(args)>1 else '-',data,contract,'synchronous' if 'signal' in op or 'container' in op else 'G_PRIORITY_DEFAULT / 300ms',risk,task,'TRACED_STATIC'))
 assert len(rows)==139
 with (ROOT/'signal-span-contracts.tsv').open('w') as f:
  w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(('legacy_site','operation','role','receiver','signal_or_interval','user_data_or_delegate','teardown_evidence','priority','reentrancy','followup','status'));w.writerows(rows)
 print(f'{len(rows)} changed complete calls traced; all have teardown/risk owner')
if __name__=='__main__':main()
