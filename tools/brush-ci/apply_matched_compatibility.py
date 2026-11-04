#!/usr/bin/env python3
"""Exact diagnostic-owned baseline build-metadata overlay; no native execution."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
POLICY_SHA='97a542bfdf237f7680b6e865ca728e4263eb244b729a8b2b0751f1f4d028c765'
CELLS=('baseline-normal','baseline-sanitizer','current-normal','current-sanitizer')
def digest(raw):return hashlib.sha256(raw).hexdigest()
def policy(path):
 p=Path(path).absolute()
 if p.resolve()!=p or not p.is_file() or p.stat().st_size>16384:raise ValueError('policy-path')
 raw=p.read_bytes()
 if digest(raw)!=POLICY_SHA:raise ValueError('policy-hash')
 return json.loads(raw)
def transform(raw,cell,p):
 if cell not in CELLS:raise ValueError('cell')
 if cell.startswith('current-'):
  if digest(raw)!=p['current_unchanged_sha256']:raise ValueError('current-meson-hash')
  return raw
 if digest(raw)!=p['baseline_before_sha256']:raise ValueError('baseline-meson-hash')
 result=raw
 for edit in p['edits']:
  before=edit['before'].encode();after=edit['after'].encode()
  if result.count(before)!=1:raise ValueError('hunk-count')
  result=result.replace(before,after,1)
 if digest(result)!=p['baseline_after_sha256']:raise ValueError('result-hash')
 return result

def apply(source,cell,inputs):
 from matched_driver_state import owned_run
 if os.environ.get('GITHUB_ACTIONS')!='true' or cell not in CELLS:raise ValueError('hosted-cell')
 s=Path(source).absolute()
 if len(s.parents)<3:raise ValueError('owned-source')
 run=owned_run(s.parents[2])
 if s.resolve()!=s or s!=run/'cells'/cell/'source':raise ValueError('owned-source')
 p=policy(Path(inputs)/'baseline-meson-compatibility.json')
 head=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],timeout=30).decode().strip()
 if head!=p['baseline_commit' if cell.startswith('baseline-') else 'current_commit']:raise ValueError('source-commit')
 f=s/'meson.build'
 if f.is_symlink() or not f.is_file():raise ValueError('meson-file')
 raw=f.read_bytes();updated=transform(raw,cell,p)
 if updated!=raw:
  # Only a new owned worktree file is replaced, never a linked original inode.
  temp=s/'.matched-meson-compatibility.tmp'
  with temp.open('xb') as stream:stream.write(updated)
  temp.chmod(f.stat().st_mode & 0o777)
  os.replace(temp,f)
 return updated!=raw
class SafeParser(argparse.ArgumentParser):
 def error(self,message):self.exit(2,'matched-compatibility: invalid-cli\n')
def main():
 p=SafeParser();p.add_argument('--source',required=True);p.add_argument('--cell',required=True,choices=CELLS);p.add_argument('--inputs',required=True);a=p.parse_args()
 try:apply(a.source,a.cell,a.inputs)
 except (OSError,ValueError,KeyError,subprocess.SubprocessError):print('matched-compatibility: exact-overlay-check-failed');return 2
 return 0
if __name__=='__main__':raise SystemExit(main())
