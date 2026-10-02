# Genuine legacy bounded-search/grow oracle

`capture.c` links the pinned old application core and `tests.c` through its
existing libtool test link recipe. `runtime.json` records the source/archive/
binary hashes and commands. `stimuli.h` defines only synthetic pixels and case
parameters; it contains no shared search implementation. Four native formats,
four mask/ROI shapes, four thresholds and two antialias settings yield128 cases.
`masks.tsv.gz` contains256 full130x70 masks and a completion record. Compressed
`runtime-capture.log.gz` preserves the original two startup debug lines too.

Run `migration/tests/capture_bounded_fill.py` only with the prepared legacy
`env.sh` and shared build lock. The captured run used the inherited legacy test
profile defaults and attempted an unwritable pluginrc save; diagnostics are
retained, exit0, no profile file was written at that missing path. Existing old
build compatibility overlays are documented in `migration/baseline/legacy/`.
The finalized gzip-producing runner was syntax-checked after the original
capture; no second full recapture is claimed.

The pinned algorithm is GPL; these synthetic stimuli and capture harness are
GPL-3.0-or-later, created for this migration. No private artwork is included.
