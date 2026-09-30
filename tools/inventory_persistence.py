#!/usr/bin/env python3
"""Inventory persistence syntax in legacy changed sources and validate paired routes."""
import csv,re,subprocess
from pathlib import Path
from inventory_call_boundaries import without_comments
ROOT=Path('migration/inventory')
PERSIST=re.compile(r'\b(?:xcf_(?:load|save|read|write)\w*|json_(?:parser_load|generator_to|builder_set_member_name|reader_read_member)\w*|gimp_config_(?:serialize|deserialize)\w*|gimp_(?:image_|item_)?parasite_\w*|GIMP_CONFIG_INSTALL_PROP_\w+)\s*\(')
def main():
 with (ROOT/'changed-files.tsv').open() as f:files=list(csv.DictReader(f,delimiter='\t'))
 rows=[]
 for info in files:
  path=info['path']
  if not path.endswith(('.c','.cpp','.h','.hpp')) or not info['source_git_blob']:continue
  raw=subprocess.check_output(['git','show',info['source_git_blob']],text=True,errors='replace');code=without_comments(raw)
  for m in PERSIST.finditer(code):
   line=code.count('\n',0,m.start())+1;rows.append((f'{path}:{line}',m[0].split('(')[0].strip(),raw.splitlines()[line-1].strip(),'SOURCE_CANDIDATE'))
 with (ROOT/'persistence-sites.tsv').open('w') as f:
  w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(('legacy_site','operation','source_line','role'));w.writerows(rows)
 with (ROOT/'persistence-routes.tsv').open() as f:routes=list(csv.DictReader(f,delimiter='\t'))
 cache={}
 for r in routes:
  for col in ('reader_anchor','writer_anchor'):
   for anchor in r[col].split(';'):
    path,token=anchor.split('#',1)
    if path not in cache:cache[path]=subprocess.check_output(['git','show',f'afa43fae:{path}'],text=True)
    assert token in cache[path],anchor
  assert r['followup'] and r['payload'] and r['storage']
 assert {r['storage'] for r in routes}>={'XCF','myb','JSON','settings','parasite'}
 print(f'{len(rows)} persistence syntax sites, {len(routes)} reader/writer routes verified at legacy revision')
if __name__=='__main__':main()
