#!/usr/bin/env python3
"""Source correspondence for dialect ambiguity/recovery definition only."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
checks={
 'app/xcf/painter-xcf-compat.cpp':['if (unresolved (a) || unresolved (b))','Selection::inconclusive','Selection::shared_layout : Selection::ambiguous','result.legacy = decode_candidate','result.standard = decode_candidate'],
 'app/xcf/painter-xcf-load.cpp':['image_properties.back ().tag == 0','requested == XCF_PAINTER_DIALECT_AUTO && declared_dialect == 1','legacy_extensions (bytes, result.legacy) == ExtensionEvidence::invalid','Legacy Painter recovery supports XCF versions 0 through 4','XCF dialect could not be selected safely','if (result.legacy.diagnostic.status == compat::Status::unsupported)'],
 'app/xcf/xcf.c':['gimp-xcf-load-standard','gimp-xcf-load-painter','suffix or MIME match and are therefore never chosen automatically'],
 'app/xcf/tests/test-painter-xcf-compat.cpp':['Selection::ambiguous','Selection::shared_layout','Selection::inconclusive','both layouts consume legal zero words'],
}
for p,needles in checks.items():
 source=(ROOT/p).read_text()
 for needle in needles:assert needle in source,(p,needle)
paths=list(checks)+['migration/contracts/xcf-dialect-recovery-definition.md']
print(json.dumps({'task':'10.016','result':'PASS source decision correspondence only','files_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2))
