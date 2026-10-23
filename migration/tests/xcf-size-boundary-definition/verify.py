#!/usr/bin/env python3
"""Source consistency for documented limits/decoder boundary, not fuzz execution."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
checks={
 'app/xcf/painter-xcf-compat.hpp':['max_records = 100000','max_objects = 16384','max_string_bytes = 1048576','max_property_bytes = 67108864','max_scanned_bytes = 268435456','Metadata-only structural candidates, NOT proof of pixel decodability'],
 'app/xcf/painter-xcf-compat.cpp':['size > end_ - pos_','referenced offset is outside the input'],
 'app/xcf/painter-xcf-load.cpp':['limits.bytes = G_MAXINT64'],
 'app/xcf/painter-xcf-storage.hpp':['guint64 (65536) * 1048576','values = G_MAXSIZE','depth = G_MAXUINT'],
 'app/xcf/painter-xcf-multipart.hpp':['multipart_block_size = 65536','multipart_max_chunks = 1048576'],
 'app/xcf/painter-xcf-preserve.cpp':['256u * 1024u * 1024u - 4096u','count > 65536','!permit_multipart && size > max_blob'],
 'app/xcf/xcf-load.c':['xcf_load_tile_rle','xcf_load_tile_zlib','fractal compression unimplemented','A following table entry is another absolute address, not an end'],
}
for name,needles in checks.items():
 source=(ROOT/name).read_text()
 for needle in needles:assert needle in source,(name,needle)
p=ROOT/'app/xcf/xcf-private.h'
for name,value in [('NONE',0),('RLE',1),('ZLIB',2),('FRACTAL',3)]:assert re.search(r'COMPRESS_'+name+r'\s*=\s*'+str(value)+r'\b',p.read_text())
paths=list(checks)+['app/xcf/xcf-private.h','migration/contracts/xcf-size-boundary-definition.md']
print(json.dumps({'task':'10.015','result':'PASS source boundary consistency only','files_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2))
