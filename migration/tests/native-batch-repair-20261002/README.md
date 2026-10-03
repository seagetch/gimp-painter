# Native file loading and Script-Fu baseline repairs

This is a new implementation and new test evidence on the restored published
checkout `3e30c05726039550cc09c0d68a3f1207a099c708`. It does not recover or claim
results from any lost, unpublished implementation.

The published aggregate under `aggregate-20261002-1455` remains unchanged: it
reported 107/108 Meson OK results, one failed save/export target, and Script-Fu
child crashes in 12 otherwise-OK libgimp targets. Its all-pass gate is false.

## Changes

`gimp-file-load` now goes through `file_open_image`, the same pipeline used for
ordinary application opens. This sets the import/native association, honors the
loader and import policy, clears undo history and restores enabled undo. The
public result preserves cancellation and error status/diagnostics. Success
without an image is rejected, including generic file handlers which may otherwise
legitimately return no image. Both the PDB source and generated invoker are updated.

Script-Fu's named user/system init-directory constants are derived from the
actual profile/data roots. They no longer index a configurable path as if it
always contained exactly two entries. An empty expanded path without a GError
is valid and no longer dereferences a missing error. Init discovery continues
to use the caller's configured order.

The build runtime selects the built libscriptfu before any installed library.
`in-build-gimp.py` creates a private disposable profile and copies the current
production extension/init files selected by Meson's install map. The current
map has 41 production scripts and five init/compatibility files. Source-tree
tests, standalone interpreter plug-ins and uninstalled scripts are excluded.
Explicit user/system gimprc flags and batch arguments pass through unchanged.
The wrapper does not edit their files; its temporary profile is removed even
when the child process fails. macOS temporary rpath edits only remove entries
added by this invocation.

## Regression coverage

- `file-load-pipeline`: six native cases through the public PDB, covering real
  XCF association, import association and undo restoration, cancellation, exact
  calling-error diagnostics, and nongeneric/generic success without an image
- `script-fu-startup`: seven real application launches covering production-script
  availability and actual `--no-data --no-fonts` Quit, plus
  empty/single/two/reversed/missing-first/missing-all configured
  paths, directory constants and init precedence; child crash diagnostics fail
  the test even if the application exits zero
- `in-build-gimp-wrapper`: five subprocess tests covering authoritative staging,
  long/short/equals/combined config options, opaque batch arguments, preservation
  of an existing profile/config, child exit propagation and failure cleanup
- The existing `save-and-export` target and all 12 C/Python libgimp targets are
  rerun without excluding the previously crashing Script-Fu child

These are focused checks. They do not replace a fresh, fully sealed aggregate,
platform/package acceptance, or hardware tests. Final results and exact evidence
hashes belong in the machine-readable report alongside this document.

## New results

All 16 focused normal targets pass, with no failed/skipped/timeout targets and
no fatal, segmentation-fault, sanitizer, critical or traceback diagnostics in
their returned logs. The native test passes six assertions groups, the wrapper
suite passes five test methods, and real Script-Fu startup passes seven cases.

Focused ASan/UBSan passes the six native cases and seven Script-Fu cases. Five C
units are instrumented: the PDB file invoker, font factory, native regression,
Script-Fu library startup helpers and Scheme wrapper. Private executables and an
overlay shared library are linked against the normal build's other objects and
libraries; those dependencies are not instrumented. LeakSanitizer is disabled.
The loader resolves the overlay libscriptfu, and the full compilation/test input
closure is unchanged before and after the final run.

The tests exposed a separate real `--no-fonts` Quit critical in
`gimp_font_factory_finalize`, where a null Pango context was unreferenced.
`g_clear_object` fixes it. The retained old-console diagnostic includes its
symbolized stack; a small included preload turns GLib's critical into SIGTRAP
only to collect that stack. The final normal and sanitizer default Script-Fu
case deliberately retains `--no-data --no-fonts` and reaches a clean Quit.

`normal-results.jsonl` and the earlier failed-attempt files retain only explicit
result, command and output fields, with no environment objects. Raw complete
Meson logs remain outside this evidence directory. The initial sanitizer launch
failed before tests because a runner environment merge omitted a dependency
path; the corrected and finally sealed runs pass. No failure is described as a
pass or reused as evidence from the lost implementation.

`sources.tar.gz` is a deterministic, verified archive created before the final
instrumented compilation. Its manifest is in `sanitizer-report.json`; it includes
the instrumented C sources, project/generated include closure, relevant test and
runner sources, Meson inputs, and current production Script-Fu scripts. External
headers are recorded by hash, not bundled or described as instrumented. The
report also hashes private generated binaries/logs without distributing them.
`manifest.json` hashes every delivered evidence file.
