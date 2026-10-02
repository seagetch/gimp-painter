Frozen native Linux aggregate, 2026-10-02

Source commit: 996297f09f1b4715b149b2c4fcdfeca1d99fb3d6
Source tree: bee0399705e5da64730e3c28614715d1e5f1731a
Tests: 2026-10-02T15:00:07Z through 2026-10-02T15:09:03Z
All-pass gate: false
Coverage: 108/108 registered targets, exact registry name/suite/command matches, no exclusions or unmatched results.
Meson outcomes: {'OK': 107, 'FAIL': 1}; exit status 1.
Strict clean-pass targets: 95. Nonclean targets: 13.
Targets with skipped/TODO subtests: 0. Targets with runtime crash diagnostics: 13.

Build and frozen identity
The full default command, ninja -C build-debian13, completed successfully before the aggregate. It rebuilt 71 steps from the exact committed source. All 201 default executable outputs were present, executable ELF files. Of 203 declared executable targets, the two explicitly non-default outputs test-preview-area and test-operations remained absent. All 108 registered tests ran.

The shared lock covered source preflight, the successful default build, both exact capture_freshness_snapshot() calls, and the whole aggregate. It was released at 2026-10-02T15:09:33Z before report preparation. No tracked source edit, commit, stage, upload or push was performed by this run.

Before/after freshness identity: True. The seal covers 3003 concrete Ninja outputs, Ninja graph/commands, generated/build metadata, install inputs/trees, tracked source contents/modes/links, untracked source inventory, and pinned gimp-data. Private Python test-profile symlinks are recorded separately from real executable ELF files and concrete build outputs. All integrity flags: True.

GUI executable SHA-256: 7a0c51b0d306cc01558278feaf4be90c83f2a6bf74858d2277619eaa0049a0b0
Console executable SHA-256: 306f9947e81d852209697e06e9ca94a4ff540b5279fd0e6ed2f3dad851cd39b4
Raw intro-tests.json SHA-256: 9af27e0114a4760bbd3b6cd3a8dbb89050ae655081afbe96ec5deb4ea17503ac
Sanitized archived registered-tests.json SHA-256: 7b7a78a7c0a30f8bcb1568de0a38c1bfb6bca516b792ae8eae462efb5ad609c0
The raw registry digest is intentionally separate from the sanitized archive digest.

Exact aggregate command
meson test -C build-debian13 --no-rebuild --num-processes 1 --timeout-multiplier 3 --print-errorlogs --logbase frozen-aggregate-20261002-1455
UI_TEST=yes; GSETTINGS_BACKEND=memory; native cloud Linux desktop terminal; dedicated global GIMP3_DIRECTORY and XDG config/cache/data/temp roots. Built-in native fixture profiles and uniquely created build-local Python test profiles were also used. The exact wrapper command was visually verified before Return.

Previously corrected UI targets
- gimp:app / painter-perspective-ui: OK; passed
- gimp:app / layer-presets-ui: OK; passed
- gimp:app / painter-mypaint-editor: OK; passed

Every remaining nonclean target
- Registry 53, gimp:app / save-and-export: FAIL; known_upstream_baseline_failure_still_fails
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
  Traceback (most recent call last):
  AttributeError: 'NoneType' object has no attribute 'equal'
- Registry 97, gimp:libgimp+python3 / color-parser: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 98, gimp:libgimp+C / color-parser: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 99, gimp:libgimp+python3 / export-options: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 100, gimp:libgimp+C / export-options: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 101, gimp:libgimp+python3 / image: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 102, gimp:libgimp+C / image: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 103, gimp:libgimp+python3 / palette: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 104, gimp:libgimp+C / palette: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 105, gimp:libgimp+python3 / selection-float: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 106, gimp:libgimp+C / selection-float: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 107, gimp:libgimp+python3 / unit: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault
- Registry 108, gimp:libgimp+C / unit: OK; meson_pass_with_known_baseline_script_fu_crash_not_clean_pass
  /workspace/scratch/5b5281e79681/gimp-painter/build-debian13/plug-ins/script-fu/script-fu: fatal error: Segmentation fault

Suites
- gimp:C: 6 registered; {'OK': 6}
- gimp:app: 42 registered; {'OK': 41, 'FAIL': 1}
- gimp:desktop: 1 registered; {'OK': 1}
- gimp:libgimp: 12 registered; {'OK': 12}
- gimp:painter: 53 registered; {'OK': 53}
- gimp:python3: 6 registered; {'OK': 6}

No baseline failure, skipped subtest, or Script-Fu crash is waived. Meson OK does not mean a clean target when its child process crashed. No candidate package was staged. The committed guard rejects this failing gate.

Evidence
aggregate-gate.json contains the complete indexed coverage and full before/after freshness snapshots. summary.json is a compact machine-readable summary. meson.jsonl, meson.txt and console.log preserve assertions, failures and diagnostics. full-build.log and executable-check.json prove the preceding current-HEAD build. source-and-binary-before.json and source-and-binary-after.json record executable and source fingerprints. validation.json records coverage, seal and fail-closed guard checks. manifest.json hashes every distributable evidence file.

Sanitization
All JSON, JSONL and text evidence was passed through migration/tests/sanitize_filter_evidence.py. Inherited environment metadata was minimized to its explicit reproducibility allowlist. Raw logs, profiles and unfiltered environment metadata remain only in the private, outside-repository raw directory and are not distributable evidence.

Limits
- This is the normal, non-sanitized Linux aggregate. Focused sanitizer checks are separate evidence; this run does not claim a full sanitizer aggregate.
- All registered Meson targets were requested without exclusions. Baseline failures, skipped subtests, and crashes reported inside Meson-OK targets still block all_passed.
- XDG configuration, cache, data, temporary files, and global GIMP3_DIRECTORY are dedicated test roots. Registered native tests additionally use their built-in fixture profiles; Python runners create unique build-local profiles.
- The 203 declared executable targets include 201 default outputs and two explicitly non-default targets absent from this build. No registered test target is omitted.
- Windows, macOS, actual tablet hardware, Wayland parity, clean-machine release acceptance, and package relocation are outside this native Linux normal-build aggregate.

Publication correspondence
The original tested commit996297f09f was published with user-approved recreated commit ID13b795a27a6fdd4178c7152e56c8ecb5f3d9420d. Their complete Git treebee0399705e5da64730e3c28614715d1e5f1731a is identical; mapping is retained under migration/publication/20261002-127. These are the unchanged test-time source IDs, not a claim of rerunning after publication. All13 original sanitized deliverables and their original manifest are stored in evidence.tar.xz; archive.json records exact checksums. The gate remains false and cannot authorize a candidate package.
