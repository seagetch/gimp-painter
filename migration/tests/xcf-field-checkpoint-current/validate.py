#!/usr/bin/env python3
"""Validate actual full-suite receipts without promoting field coverage to parity."""
import argparse,csv,hashlib,io,json,re,tarfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
EXPECTED={'xcf':4,'painter-provenance':12,'painter-xcf-open':31,'painter-xcf-roundtrip':49}
def require(condition,message):
 if not condition:raise ValueError(message)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def tap(text,count):
 require(f'1..{count}' in text.splitlines(),'exact TAP plan')
 names=re.findall(r'^ok \d+ (\S+)',text,re.M)
 require(len(names)==count and len(set(names))==count,'exact successful cases')
 require(not re.search(r'(?im)^not ok |#\s*(SKIP|TODO)\b',text),'no skipped/failed cases')
 return set(names)
def validate(normal,asan,field_text):
 require(len(normal)==4,'four normal suites');seen=set();named=set()
 for row in normal:
  name=row['name'].split(' / ')[-1];require(name in EXPECTED and name not in named,'unique registered suite');named.add(name)
  require(row['result']=='OK' and row['returncode']==0,'normal pass')
  require(row['duration']<(180 if name in ('painter-xcf-open','painter-xcf-roundtrip') else 60),'original normal budget')
  seen|=tap(row['stdout'],EXPECTED[name])
 require(set(asan['results'])==set(EXPECTED),'four sanitizer suites')
 require(len(asan['instrumented_sources'])==41 and len(asan['rtti_compatibility_only_sources'])==40 and not(set(asan['instrumented_sources'])&set(asan['rtti_compatibility_only_sources'])),'bounded instrumentation identity')
 require('LSan disabled' in asan['scope'] and 'remaining host/dependencies uninstrumented' in asan['scope'],'instrumentation disclosure')
 require(not asan['changed_during_build'] and not asan['changed_during_run'],'no input drift')
 for name,row in asan['results'].items():
  budget=180 if name in ('painter-xcf-open','painter-xcf-roundtrip') else 60
  require(row['exit_code']==0 and row['timeout_seconds']==budget and row['duration_seconds']<budget,'sanitizer pass and original budget')
  require(tap(row['stdout'],EXPECTED[name])<=seen,'same native cases')
  require(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:',row['stderr']),'no sanitizer error')
  if name=='painter-xcf-roundtrip':require(row['command'][-2:]==['-m','slow'],'large case enabled')
  if name=='painter-xcf-open':require(any('run_filter_owner_context_fixture_test.py' in x for x in row['command']),'registered owner fixture')
 fields=list(csv.DictReader(io.StringIO(field_text),delimiter='\t'));require(len(fields)==185 and len({x['id']for x in fields})==185,'fixed field ledger')
 for row in fields:
  names=re.findall(r'/[A-Za-z0-9_/-]+',row['value_test']);require(bool(names) and set(names)<=seen,'actual field assertion '+row['id'])
 return seen
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
 n=json.loads((HERE/'normal.json').read_text());s=json.loads((HERE/'sanitizers.json').read_text())
 validate(n,s,(ROOT/'migration/inventory/xcf-field-acceptance.tsv').read_text())
 for name,h in json.loads((HERE/'artifact-sha256.json').read_text()).items():require(sha(HERE/name)==h,'artifact '+name)
 with tarfile.open(HERE/'tested-sources.tar.gz')as ar:
  for name,h in s['sources_sha256'].items():
   require(hashlib.sha256(ar.extractfile(name).read()).hexdigest()==h,'archived source '+name)
   if a.check_source:require(sha(ROOT/name)==h,'current source '+name)
 closure=json.loads((HERE/'dependency-closure.json').read_text())
 require(closure['unchanged_after_execution'] is True,'compiler inputs stable through run')
 with tarfile.open(HERE/'dependency-closure.tar.gz')as ar:
  for name,h in closure['source_sha256'].items():require(hashlib.sha256(ar.extractfile(name).read()).hexdigest()==h,'compiler closure '+name)
 print('PASS full96normal+96boundedASan;185field references; original budgets; remaining graph/platform duties not closed')
