# Pristine legacy reference replay, 2026-10-15

The existing `tools/build_legacy_reference.sh` completed end-to-end in a new
isolated directory using its exact GIMP, babl and GEGL commits and the existing
legacy signed-snapshot package lock. All560 cached archive hashes were verified
before extraction. Existing GIMP3 source, prefix and build directories were not
changed. The recipe itself required no changes.

- babl: 23 passed, zero failed
- GEGL simple suite: 28 passed, two skipped, one failed (known test-gegl-tile)
- GIMP build, install and version query: exit0, version2.8.23
- Installed uninstrumented executable SHA-256:
  `85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1`
- Separate positive builtin FilterLayer capture and original-reader reopen: exit0
- Original direct plug-in-edge negative reader case: exit139 again

This closes the previously unmeasured second pristine recipe replay, while
preserving its known dependency/runtime limitations. It does not establish
historical binary identity, a passing original GIMP test suite, GUI/tablet
behavior, or scheduler acceptance. No historical build-report or linked
manifest has been changed.

`report.json` gives exact provenance and outcomes; `build-logs.tar.gz` preserves
build/test commands and logs, including warnings/failures. `cache-verification.json`
records every verified archive. `build-tool-environment.json` records generated
tool relocation and input/output hashes. The original application was installed
before the independent fixture observer was added and was not overwritten.
