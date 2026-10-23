#!/usr/bin/env python3
"""Verify pinned old image metadata presence definition; no application run."""
import argparse,csv,hashlib,json,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--legacy-source',required=True);args=p.parse_args()
manifest=json.loads((HERE/'source-hashes.json').read_text());sources={}
for name,digest in manifest['source_sha256'].items():
 raw=subprocess.check_output(['git','-C',args.legacy_source,'show',manifest['source_commit']+':'+name]);assert hashlib.sha256(raw).hexdigest()==digest;sources[name]=raw.decode()
save=sources['app/xcf/xcf-save.c'];start=save.index('xcf_save_image_props (XcfInfo    *info,');end=save.index('\nstatic gboolean\nxcf_save_layer_props',start);body=save[start:end]
actual=set(re.findall(r'xcf_save_prop \(info, image, (PROP_[A-Z_]+)',body))
rows=list(csv.DictReader((ROOT/'migration/inventory/legacy-image-metadata-presence.tsv').open(),delimiter='\t'))
recorded={r['wire_record'] for r in rows if r['scope']=='image'}
assert actual==recorded and len(actual)==12,(actual,recorded)
assert 'if (gimp_parasite_is_persistent (parasite))' in save
assert 'gimp_parasite_list_persistent_length (list) > 0' in save
assert '"icc-profile"' in sources['app/core/gimpimage.c']
assert '"gimp-comment"' in sources['app/core/gimpimage-new.c']
assert '"gimp-image-grid"' in sources['app/core/gimpgrid.c']
wire=list(csv.DictReader((ROOT/'migration/inventory/xcf-wire-records.tsv').open(),delimiter='\t'))
assert {r['field'] for r in wire if r['owner']=='image' and r['wire_id'].isdigit()}=={n.removeprefix('PROP_') for n in actual}
print('PASS pinned-source hashes and all12 image-property presence rows; no native runtime claim.')
