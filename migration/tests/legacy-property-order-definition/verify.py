#!/usr/bin/env python3
"""Check exhaustive pinned-source ordering rule inventory, not runtime permutations."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--legacy-source',required=True);a=p.parse_args()
data=json.loads((HERE/'rules.json').read_text());sources={}
for name,digest in data['source_sha256'].items():
 raw=subprocess.check_output(['git','-C',a.legacy_source,'show',data['source_commit']+':'+name]);assert hashlib.sha256(raw).hexdigest()==digest;sources[name]=raw.decode()
load=sources['app/xcf/xcf-load.c'];bounds={'image':('xcf_load_image_props (XcfInfo','xcf_load_layer_props (XcfInfo'),'layer':('xcf_load_layer_props (XcfInfo','xcf_load_channel_props (XcfInfo'),'channel':('xcf_load_channel_props (XcfInfo','xcf_load_filter_specs(XcfInfo')}
for owner,(start,end) in bounds.items():
 s=load.index('\n'+start);e=load.index('\n'+end,s+1)
 tags=re.findall(r'case (PROP_[A-Z_]+):',load[s:e]);rows=[r for r in data['rules'] if r['owner']==owner]
 assert set(tags)=={r['property'] for r in rows} and len(tags)==len(rows)
assert 'private->guides = g_list_reverse (private->guides)' in load
assert 'private->guides = g_list_prepend' in sources['app/core/gimpimage-guides.c']
assert 'private->sample_points = g_list_append' in sources['app/core/gimpimage-sample-points.c']
assert 'gimp_parasite_list_remove (list, parasite->name)' in sources['app/core/gimpparasitelist.c']
assert 'xcf_skip_unknown_prop (info, prop_size)' in load
assert 'ordered, duplicates and END retained' in (ROOT/'app/xcf/painter-xcf-compat.hpp').read_text()
print(f"PASS {len(data['rules'])} pinned reader cases and helper rules; no native permutation claim.")
