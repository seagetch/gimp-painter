"""Generate a human-readable ordering summary from exported raw-log records."""
import json,sys
from pathlib import Path
d=json.loads(Path(sys.argv[1]).read_text());out=['# Measured GUI timeline summary','','Elapsed microseconds relative to run-enter. Core IDs and observed ordinals identify events within one process. These are instrumented samples, not performance guarantees. Full nesting and repeat measurements remain in timeline.json.','']
keep=['run-enter','run-return-running','stop-request-running','cancel-signal','explicit-cancel-request','late-cancel-request','shadow-merge-applied','progress-end-callback','propagation-begin','propagation-end','stack-unblocked-requeue','detach-request']
for scene in ['async','stack','cancel','latecancel','detach','builtin']:
 r=d['captures/'+scene];out+=['## '+scene,'']
 for start in [x for x in r['events'] if x['event']=='run-enter']:
  rows=[x for x in r['events'] if x['name']==start['name'] and x['ordinal']==start['ordinal'] and x['event'] in keep]
  out.append(f"- {start['name']}, core ID {start['id']}, ordinal {start['ordinal']}: "+'; '.join(f"{x['event']} +{x['time_us']-start['time_us']}us (attached={x['attached']})" for x in rows))
 out+=['- Final states: '+str(r['states'][-(2 if scene=='stack' else 1):]),'']
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
