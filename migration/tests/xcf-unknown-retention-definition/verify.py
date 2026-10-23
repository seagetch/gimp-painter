#!/usr/bin/env python3
"""Verify source correspondence for retention policy, not runtime preservation."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
checks={
 'app/xcf/painter-xcf-preserve.cpp':['Duplicate Painter metadata key; original capsule is retained and cannot be safely rewritten','explicit Dictionary (GVariant *base = nullptr)','g_variant_dict_init (&dict, base)','Unknown origin archive version cannot be overwritten','Unknown Painter extension version, kind or invalid semantic state cannot be overwritten with edited custom semantics','"opaque-record-sequences"','"original-properties"','"original-argument-model"','"original-argument-procedure"','gimp_filter_layer_ref_opaque_arguments','original capsule remains inert'],
 'app/xcf/painter-xcf-transport.cpp':['gimp_painter_provenance_set_transport','captured.push_back','Invalid retained parasite framing'],
 'app/xcf/painter-xcf-multipart.cpp':['multipart_protected_records','multipart_inert_markers','inert_magic','reserved_inert_name'],
 'app/core/gimp-painter-provenance.cpp':['gimp_painter_provenance_ref_records','gimp_painter_provenance_set_records'],
}
for p,needles in checks.items():
 source=(ROOT/p).read_text()
 for needle in needles:assert needle in source,(p,needle)
paths=list(checks)+['migration/contracts/xcf-unknown-retention-definition.md']
print(json.dumps({'task':'10.014','result':'PASS source policy consistency only','files_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2))
