# Genuine Value Invert capture tooling

Status: all fresh genuine old-runtime captures passed on 2026-10-16 and their
archives are explicitly sealed. The recovered original runtime was built in a
separate source/build tree. These observations are old-runtime evidence only;
native owner/XCF acceptance and full-port acceptance remain separate gates.
See `migration/tests/value-invert-capture-evidence.json` for exact reports/seals.

The first diagnostic writer smoke and all three complete batches passed; no
failed runtime attempt was discarded. Each run retained two inherited GValue
object assertions and one missing tag-cache startup message, with no unexpected
runtime diagnostics. This is not a warning-free old runtime claim. The harnesses
are headless; using authenticated portrait Xvfb does not establish visible GUI
behavior and they do not produce a meaningful GUI screenshot.

## Oracle and bounded scenes

The pinned original revision is `afa43fae3e920210146abed514f136fd49f671b5`.
The complete `plug-ins/common/value-invert.c` SHA-256 is
`bf7c46cac7f62c13ec73590d53c12ba3dd85fd28def5f74451aaf43b969fef20`.
Capture source validation checks the pinned commit and unchanged source files,
then records before/after executable and source hashes. Only private observation
copies of the old core receive hooks. The original plug-in, raw callback, merge,
projection copy, and XCF writer remain unchanged. Gray/indexed are outside this
RGB/RGBA capture task, including the separate indexed colormap procedure path.

The captured base matrix contains 140 scenes: 50 synchronous PDB and 90 live
FilterLayer scenes. Two live standard scenes save genuine old-writer XCF files
and execute a changed-source rerun. Two additional 1031x1021 scenes exercise
observed mid-work changes and unchanged lower notifications. Sixteen projection
scenes cover RGB/RGBA lower sources, top/group parents, positive/negative overflow,
and fresh/seeded prior cache. Each projection scene executes three times using
byte-identical lower notifications; only the owner moves on run three.

The base matrix includes all component masks, soft/hard/outside selections,
RGB/RGBA and hidden RGB, tiny/tile-crossing bounds, signed offsets, own alpha lock,
actual merge-entry and strictly mid-work edits, idle context changes, and
changed/unchanged lower notifications. The Value Invert pixel-region loop reports
its first processed region and every sixteen regions. Captures require the
actual PDB progress hook; a final progress value or image size never establishes
mid-work mutation. The 96x96 and 2x2 crossings require the observed first .25.

## Artifact interface

`captures.tsv` has Max RGB's 30 columns with the operation `mode` removed:
29 fields, listed explicitly in `tools/check_value_invert_evidence.py`. Case IDs
likewise omit the `-m` segment. The two writer scene IDs are
`live-c3-17x13-s0-f-1-a15-l0` and `live-c4-17x13-s0-f-1-a15-l0`.

Projection `captures.tsv` and `copies.tsv` retain the 14 and 20 fields from the
Max RGB projection schema. `options.tsv` records `id run run_mode`, reading the
retained context slot zero at each actual start. All three observed values are
noninteractive run mode 1. Value Invert has no operation-specific argument.

The actual old writer emitted a 56-byte capsule, literal `plug-in-vinvert` plus NUL,
record tags 2/1/10/0 and payload sizes 4/0/0/0. Both RGB/RGBA writer observations
matched the prior source prediction. The validator independently inspects bytes
emitted by the actual writer and compares `.definition.raw` exactly. Pure-Python
unit tests construct parser inputs only; none are written into a capture corpus.

## Reproduction

After restoring the original source, dependencies and installed prefix, source
the legacy build environment and use the existing authenticated portrait Xvfb.
Each attempt requires independent new work/output directories. The producer
owns `/workspace/shared/gimp-painter-build.lock`; do not nest that same lock.

```sh
python3 -B tools/tests/test_value_invert_capture.py -v
python3 tools/run-painter-xvfb.py python3 tools/capture_legacy_value_invert.py \
  --capture --legacy-source "$source_dir" --legacy-prefix "$prefix" \
  --work /absolute/fresh/base-work --output /absolute/fresh/base --timeout 600
python3 tools/run-painter-xvfb.py python3 tools/capture_legacy_value_invert.py \
  --capture --large --legacy-source "$source_dir" --legacy-prefix "$prefix" \
  --work /absolute/fresh/large-work --output /absolute/fresh/large --timeout 600
python3 tools/run-painter-xvfb.py python3 tools/capture_legacy_value_invert_projection.py \
  --capture --legacy-source "$source_dir" --legacy-prefix "$prefix" \
  --work /absolute/fresh/projection-work --output /absolute/fresh/projection --timeout 600
```

Omitting `--capture` performs source pin-checking and plan preparation only.
`--case-pattern` may diagnose a subset, but a subset cannot pass full sealing.
Keep failed attempts with their logs, never repair or overwrite their outputs.
`seal_legacy_value_invert_capture.py` independently validated the complete
genuine captures and created the base, large and projection archives/manifests. Inspect its hashes
before explicitly populating archive pins. It never creates expected raster data
or fills pins automatically. Native owner/XCF acceptance remains separate.

## Completed capture evidence

The combined base/large matrix has 142 scenes, including 92 live scenes and 126
live output comparisons. There are two changed-source reruns, 32 unchanged-byte
lower notifications, 14 actual merge-entry edits and 16 strictly mid-work edits.
All 24 declared original sources and three executable inputs per batch remained
unchanged. The two writer files carry identical 56-byte capsule bytes, SHA-256
`120249ec2b49d9c243a74edbf44135e9a8d3ea2e3c2ccc6771def3e755c20aa5`.

Projection history has 16 scenes and 48 observed runs. Its distinguishing
witnesses include 888 pixels retained beyond the parent intersection on run two,
444 nonzero retained pixels, 390 retained pixels changed from their seed, 2,664
group-owned pixels outside the image, 10,608 checked raw callback pixels and
8,940 zero-alpha hidden-color channel observations. The eight seeded scenes use
the real mark-as-loaded API and produce no initial job. They do not establish
actual legacy XCF reopening; those old-writer files are separate base artifacts.

The full sealed archive validators and 36 pure-Python controls pass. No generated
expected raster is substituted for observed bytes. The captured source and
plug-in code remain pinned to the original revision; the independently checked
point/merge equations validate those observations and do not author them.

## Reference-runtime recovery boundary

This is a fresh source-rebuilt reference using the pinned GIMP/babl/GEGL commits
and the retained signed-snapshot dependency lock. It is not a historically
binary-identical executable. Six GIMP files have explicit modern-toolchain build
adaptations; the 24 transformation, context and writer files sealed per capture
still match the original source. GEGL has a documentation-tool regeneration
change and retains the known `test-gegl-tile` failure (simple suite exit 2).
Overall recipe exit zero establishes the build, not a green dependency suite.

`legacy-value-invert-runtime-provenance.tar.gz` preserves the exact recovery and
linked-library receipts, full build commands/logs, source diffs and 42,370-entry
post-build recovery runtime file manifest. Its own manifest records every retained hash.
The 24 old static archives used for the harness link are included in the receipt
with seals matching that later runtime manifest; this does not establish a
pre-capture archive seal. Executable before/after seals match each capture.
Binary payloads themselves remain in the separate recovery package. No claim of
an unmodified entire source tree or warning-free dependency build is made.
