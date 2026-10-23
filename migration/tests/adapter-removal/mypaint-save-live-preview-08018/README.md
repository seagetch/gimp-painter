# Save the live brush preview, not the stale encoded cache

This closes one concrete row44 semantic gap left by the named save entry.
Two genuine regressions against the unchanged b2bc287087 provider fail for
both original named save and direct native save-vfunc stream: after mutating
pixels through public `ref_icon`, reload yields old red-channel52 instead of
current164. `expected-red.json` binds both failures to the unchanged provider,
fixture and binary. Failure is at the pixel assertion, not build/setup.

The one native serializer now takes an owning icon reference and encodes its
current pixels into a detached Resource snapshot before writing any bytes.
Both entry routes therefore share the fix. The source's settings/encoded
cache remain unchanged: serialization is not an editing operation. A common
PNG helper is shared with `set_icon` rather than adding a duplicate encoder.
The icon is retained across encoder callbacks; source-owned bytes cannot be
freed by losing another reference while the encoder is active.

Repeated two-color roundtrips cover both routes. A test-only GNU linker wrap
injects exactly one selected PNG failure, preserving all non-targeted varargs
options through the real vector encoder. File output must remain unchanged,
stream output must remain empty, source dirty/settings must remain intact and
retry must succeed. Production has no fault hook. Unsupported linkers do not
register these two fault cases; this Linux native run must include both, with
no skipped cases. No UI change or screenshot is claimed.

## Remaining row44 boundaries

Live-pixel serialization is addressed by this child. Native checked I/O versus
old ignored failures, clean/mtime versus unchanged dirty state, embedded PNG
versus a separate sidecar, and historical type/layout compatibility remain
explicitly PARTIAL in the canonical inventory. This child does not restore
those old semantics or close parent08.018. The wider row42/44 disposition is
in `../mypaint-file-save-08018/README.md`, whose live-pixel note is historical
and is superseded only by this focused regression proof.

## Verified results

The ordinary real C11 suite passes 42/42 in 1.72 s; bounded ASan/UBSan/float
instrumentation passes 42/42 in 2.39 s. Both keep the original 60 s budget,
include both encoder-failure controls, and skip no cases. 37 units are
instrumented and 40 additional units use RTTI-only compatibility; remaining
application/dependency objects are uninstrumented and LSan is disabled.
No sanitizer diagnostic occurred. Both ran through authenticated portrait
Xvfb (1080x1920x24), but these are core tests, not UI screenshots.

Source-only persistence checks follow the shared encoder and both callers:
201 MyPaint field assertions plus six semantic mutations rejected. Combined
config/MyPaint/XCF metadata gate passes 1,378 records, 873 dispositions,
16 routes and 46 rejected controls, without changing field populations or
defaults. Eight controls belong to the separate tool-group config repair.
The receipt excludes inherited environment dictionaries. Publication remains
subject to the parent approval gate.
