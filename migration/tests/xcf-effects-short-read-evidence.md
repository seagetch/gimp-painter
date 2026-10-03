# XCF effects-offset short-read cleanup checkpoint

Status: verified narrow cleanup. No multipart transport is enabled.

`xcf_load_layer()` can parse and own a valid `FilterData` and mask, then fail
reading the next effects offset before handing the accumulated list to layer
qdata. The error path now frees that local list; successful publication steals
it so later error cleanup cannot free layer-owned data twice.

The regression saves a genuine brightness/contrast effect and named effect mask
through native Save and verifies ordinary Open succeeds. A memory input then
injects every 0..7-byte short read at the next 8-byte offset, after the valid
effect has been parsed. Each case verifies failure, exactly one mask finalizer,
and empty selected/linked layer lists. This is host I/O failure coverage, not a
replacement for malformed-file or all-allocation-failure coverage.

Evidence recovered and checked read-only on 2026-10-03:

- `xcf-effects-short-read-normal-meson.log`: normal native target 1/1 pass
- `xcf-effects-short-read-sanitizers.json`: 247/247 cases, exit 0; 31 source
  units instrumented with ASan/UBSan/float-cast-overflow and 34 RTTI-only
  compatibility units; dependencies and remaining host units uninstrumented
- LeakSanitizer disabled; this does not claim whole-GIMP leak freedom
- 143 selected source/header/runner inputs and compile database were archived
  before compilation with `--seal-inputs`; no reported build/run source drift
- All archived source hashes and the three changed source/runner hashes match
  the report. Input archive SHA-256:
  `4c42f95de4c83616fbdff34b0f11c6e3db878d22786049b093045d8e845d2b7c`

The sanitizer runner now chooses each production archive compile entry instead
of a later standalone duplicate, excludes genuinely unregistered compatibility
sources, and can seal selected inputs before compilation. Source snapshots
inside the evidence archive are provenance only; they do not broaden this
commit's source ownership or silently include unrelated work in the commit.
The existing initial diagnostic log is retained as historical evidence and is
not counted as a passing run.
