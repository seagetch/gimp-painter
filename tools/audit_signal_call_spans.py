#!/usr/bin/env python3
"""Find legacy signal/source deltas across complete calls, including changed arguments."""
import csv
import re
import subprocess
from pathlib import Path
from inventory_call_boundaries import without_comments
from audit_c_reference_added_lines import added_lines
ROOT=Path('migration/inventory')
CALL=re.compile(r'\b(g_signal_connect\w*|g_idle_add\w*|g_timeout_add\w*|g_source_set_callback|g_source_remove|gimp_container_add_handler|gimp_container_remove_handler)\s*\(')
def end_call(code, start):
    depth=1; quote=None; i=start
    while i<len(code):
        ch=code[i]
        if quote:
            if ch=='\\': i+=2; continue
            if ch==quote: quote=None
        elif ch in '\"\'': quote=ch
        elif ch=='(': depth+=1
        elif ch==')':
            depth-=1
            if not depth: return i+1
        i+=1
    raise ValueError('unclosed call')
def main():
    with (ROOT/'changed-files.tsv').open() as f: files=list(csv.DictReader(f,delimiter='\t'))
    rows=[]
    for info in files:
        path=info['path']
        if not path.startswith('app/') or not path.endswith(('.c','.cpp','.hpp','.h')) or not info['source_git_blob']: continue
        raw=subprocess.check_output(['git','show',info['source_git_blob']],text=True,errors='replace')
        code=without_comments(raw)
        matches=list(CALL.finditer(code))
        if not matches: continue
        adds=added_lines(info['base_git_blob'],info['source_git_blob'],len(raw.splitlines()))
        for m in matches:
            end=end_call(code,m.end()); first=code.count('\n',0,m.start())+1; last=code.count('\n',0,end)+1
            changed=sorted(adds.intersection(range(first,last+1)))
            role='DELTA' if changed else 'UNCHANGED_CALL_CONTEXT'
            rows.append((f'{path}:{first}',m[1],last,role,','.join(map(str,changed)) or '-',re.sub(r'\s+',' ',code[m.start():end]).strip(),'DONE'))
    with (ROOT/'signal-call-spans.tsv').open('w') as f:
        w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(('legacy_site','operation','end_line','delta_role','added_lines','complete_call','status'));w.writerows(rows)
    print(f'{len(rows)} complete calls; {sum(r[3]=="DELTA" for r in rows)} changed including argument-only changes')
if __name__=='__main__': main()
