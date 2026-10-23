# Original 04.009: static archive order and retained registration roots

This is a bounded GNU/Linux link check against the current default and optional
HTTP builds. It does not start GIMP, GTK, HTTP, plug-ins, or a user profile.
The actual production links already use archive rescan groups; no production
source or Meson change was necessary.

`roots.json` declares 22 reviewed current roots: the owner-close bridge, lazy
Clone/Filter types, brush factory, paint and tool registration, session/options,
provenance, device profile options, Perspective Guide, presets/actions/views,
layer tiles, brush editor and optional HTTP lifecycle. Each applicable root
must have its source anchor, a real undefined reference in the compiled
consumer, its provider member in the actual linker extraction map, and its
required symbols in both the existing and replayed executable. Lazy types are
allowed to register on demand; retention is not an assertion that `get_type`
runs at startup. This maintained list is not a universal discovery mechanism
for unknown future registration conventions.

The frozen native report records all four original binary identities, actual
app archives and member hashes (including the bytes referenced by thin
archives), extracted members, explicit root chains, strong archive dependency
SCCs and the compile-source identities inherited from the prior actual build.
A missing source match or changed original binary rejects a refresh. Detailed
link maps, commands, error logs and member-to-member dependency witnesses are
in `evidence.tar.gz`. No replayed GIMP executable is executed or retained.

## Verification

Default mode verifies the historical checkpoint plus the recorded translation-unit,
selected configuration, root and checker identities. It does not reconstruct the
complete compiler/header dependency closure or discover added source-list inputs.
A pass must not be treated as a current full-link proof after an unbound header,
generated dependency or source-list change; rebuild the affected artifacts and
refresh the link evidence for that claim.

From the repository root, these checks need only Python and the frozen files:

```sh
python3 -B tools/check_painter_archive_order.py
python3 -B migration/tests/archive-order/test_checker.py
```

To refresh, use the existing configured compiler/dependency environment
(including the HTTP dependencies). The directory supplied to `--link-commands`
contains the four final link commands, `default-verification.json`,
`http-verification.json`, and `source-hashes-after.json` from the preceding
actual build checkpoint. Build targets and archives must already exist.

```sh
python3 -B tools/check_painter_archive_order.py \
  --build /path/to/build-default \
  --http-build /path/to/build-http \
  --link-commands /path/to/final-link-evidence \
  --output-dir /path/to/separate/archive-order-results
```

Only separate probe output is written. The original binaries, archives and
archive member object bytes are checked unchanged after each replay.

## Negative and corrected cases

The 12 controlled comparisons are explicitly separated in `native.json`:

- Two production console comparisons remove the rescan group and fail with
  unresolved symbols whose providers are present in the real input archives.
  DSO `--as-needed` is disabled in both the failed and corrected commands, so
  the correction changes only archive rescanning. Restoring the group succeeds
- Two real `libappcore.a` partial-link comparisons omit an explicit root, then
  require `gimp_clone_layer_get_type`. The unrooted member disappears despite
  rescan grouping; the rooted member is retained. These probes intentionally
  leave unrelated external references unresolved and do not execute a type
- Four illustrative constructor-only registration fixtures show silent
  dropout from ordinary and grouped archives (exit 23), then retention with a
  C-call anchor or `--whole-archive` (exit 0). The production contract continues
  to require explicit feature roots, not static-constructor side effects
- Four illustrative A/B cycle cases fail in either single-pass order, and
  pass with a rescan group or A/B/A repetition. Successful fixtures return 0

The five Python tests protect mixed inline/wrapped/thin/whole-archive map
parsing, distinguish real cycles from a DAG, and reject removed root evidence,
a falsely successful negative link, and source drift.

This closes only the original infrastructure criterion when adopted with its
ledger/source-duty review. It does not close JSON direct dependency,
Windows/macOS qualification, feature runtime registration/behavior, later
feature ports, or historical strict source-drift acceptance failures.

A wording-only amendment preserved the exact executed checker inside the
evidence archive as `executed-checker.py`. Its recorded digest distinguishes
the generator used for the fresh links from the subsequently clarified checker.
