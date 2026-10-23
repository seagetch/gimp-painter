# Phase E bounded Linux parameter-interface acceptance

`acceptance.json` records `16.023/parameter-schema-acceptance`. This completes the
five parameter-interface children A–E only. The executor parent `16.023`, whole
port, final candidate, source-distribution and release gates remain open.

The synchronous allocation sweep reproduced rejected definitions with an extra
Undo or a replaced model/revision. `gimpfilterlayer.cpp` now prepares fallible
request/progress/native state before publishing the definition, after attachment
and the final reentrant guard. Three regressions cover owner resize, precision
conversion and nested-loop consumption of the old dispatcher wakeup. Rejected
preparation paths without independent callback mutation preserve the model,
revision, raw bytes, completed pixels/cache and this transaction's Undo item and
depth. Dirty notifications and expiration of previous redo are still outside
that invariant. Unsupported definitions retain their prior storage/diagnosis
semantics; accepted definitions may still fail later during execution.

## Results

- Normal: core21, editor/persistence42, existing XCF6 and scoped allocation18
  groups; focused ASan/UBSan: core21, editor/persistence42, allocation18. No skips
  or sanitizer diagnostics. LeakSanitizer is disabled; dependencies and the
  remaining application are not instrumented. UI instrumentation is29 units
  plus45 RTTI-only units; core instrumentation is20 plus42 RTTI-only units.
- Each allocation mode rejects289 exact reached positions: descriptor/current160,
  real route54, C++ patch66, recoverable GLib patch8 and actual GTK matrix1.
  Wrappers are private executable-only, single-thread/single-failure scopes.
  Existing process115, scheduler37 and spool11 cases pass. A separate128MiB
  child limit gives recoverable `g_try_malloc` NULL and expected fatal
  `g_malloc` SIGTRAP for a256MiB request. Whole-app OOM recovery is not claimed.
- One final genuine-old replay: Blinds320, SmallTiles196, Retinex174,
  Convolution296 final merges plus296 raw ROI,8 geometry rejections and41 native
  analytic requests. Additional12 Blinds identities,6 Retinex compositions,
  2 raw-shadow and4 carrier rejection cases pass. The2626-file old oracle seal
  is unchanged. The two corpus drivers are in-build harnesses; their actual
  helper and four plug-ins are loaded from the relocated runtime.
- Actual X11 application: all four editors create, reject invalid input,
  preserve no-op/Cancel/window-close values, accept an edit and show Up to date.
  Ordinary Save→Close View→Open restores five layers and all four edited values.
  Reopened no-op OK leaves Undo/Redo disabled; a further Convolution matrix edit
  saves successfully. Eight saved capsules are CLEAN with complete current
  caches. All three other argument models are byte-exact; only coefficient0
  changes from0.125 to0.25. Exact runtime counters are native assertions,
  distinct from the visible GUI observations.
- Default build, final35 recipe tests, changed-unit compile against actual
  frozen GLib2.70 headers, task validation and diff checks pass.

## Prototype artifact and identity

The tested private artifact is `gimp-painter-linux-x86_64.tar.zst`,130064424 bytes,
SHA-256 `6e10de65e2a96c90650d8e732884652454d87033df99134e9fe1a560665d5a1c`.
Seven volumes, each below20MB, reassemble to that exact archive and restore5271
manifest entries. The same entries remain unchanged after runtime and GUI use.
The bundle contains420 ELF files,175 pinned Debian package dependencies,177
Painter brushes and8 presets, with license notices and source indexes.

This is a Debian13 x86_64 test runtime requiring glibc2.41, host Python3 with the
needed ABI, X11/desktop services and the stated host libraries. It is not a
universal Linux package. The GUI additionally loaded exactly the host glibc
`gconv/ISO8859-1.so` identified in `gui/host-gconv.json`; its bytes match the pinned
libc6 archive. This remains an explicit strict-observer finding, separately
explained as a host glibc resource. The BASE_ABI allowlist was not expanded.
Original source/build/dependency directories stayed present. The explicit
minimal environment and observed helper/plugin/library paths exclude their
execution fallback; `/proc` sampling is not a whole resource-access trace.

The embedded manifest deliberately retains `prototype-not-a-release`,
`source_archive_complete=false`, source-delivery indexes only, and
`source_build_correspondence` unestablished. It is not a verified complete
source distribution. Its Linux-smoke field predates testing. This outer receipt
names the exact tested artifact without modifying it to change embedded gates.
It was packaged from a WIP snapshot based on public commit
`0a7206ddd303ced6e5de33746264225c4e45f05d`. Production/build/recipe hashes are
mapped in `acceptance.json`; later report/docs/WBS changes do not imply that the
bundle was built from the eventual acceptance commit. Freshness/candidate/full
aggregate rules remain intact. No public release or tag is created.

## Evidence and replay

`raw-evidence.tar.gz` is deterministic (sorted members, UID/GID0, mtime0).
`evidence-manifest.json` hashes every member. Raw acceptance/failure logs,
commands, selected identities, GUI steps and source-level failure sites are
included. Full3.3MB build and1MB file manifests live in the private runtime;
the outer receipts name their whole-file hashes. Larger process/seal reports
are explicitly compacted with whole-file hashes and storage descriptions.
The actual GUI XCF saves are generated modern test files, not old pixel oracles.
No profiles, environment dumps, credentials, dependency key text, unrelated
account/desktop data or internal conversation instructions are included.

Load `gimp-build-restoration/env.sh`, then `tools/linux-debian13-env.sh` in a
configured checkout. All builds/native runs use
`/workspace/shared/gimp-painter-build.lock`. Native GTK needs the actual desktop
terminal environment, not just `DISPLAY=:0`. The archive includes the accepted
native, sanitizer, GUI observer and fixture-generation wrappers; adjust their
technical absolute paths together. Canonical runners are:

```
python3 -B migration/tests/build_filter_allocation_tests.py --help
python3 -B migration/tests/run_filter_acceptance_native.py --help
python3 -B migration/tests/run_filter_resource_failures.py --help
python3 -B migration/tests/run_filter_acceptance_runtime.py --help
python3 -B tools/package-linux-runtime.py --help
python3 -B tools/split-runtime-artifact.py --help
```

The core sanitizer builder also automatically attempted its old full harness
without the required fixture wrapper: compilation succeeded, ten groups passed,
then it aborted on the missing fixture. Its unchanged executable subsequently
passed the intended wrapped21 groups. Earlier fault-harness `/proc` setup and
unrecognized executable-location failures are retained. The first relocation
observer misclassified `ld.so.cache` as a shared object; no corpus had begun.
Only the narrow `.so`/`.so.N` classifier changed before resealing; all binaries
and5270 other bundle entries were identical. A final recipe test attempt without
the configured environment skipped21 cases; the corrected invocation passed35
with no skips. None is relabeled as a successful run.

## Open diagnostics and boundaries

The GUI records known GTK/GDK accessibility diagnostics under `34.003` and six
missing application-icon lookups at the compiled prefix. Two first-Save chooser
criticals (`gtk_file_chooser_set_current_folder_file` with invalid GFile, followed
by `g_object_unref`) are a separate unclassified issue. A possible NULL-parent
source path is documented as a hypothesis only; no stack establishes the cause.
Successful Save/Close/Open does not make those diagnostics harmless or resolved.

C's prospective whole-application OOM wording and D's future Phase E statement
are explicitly reconciled by the new bounded acceptance contract. Current
metadata/definition allocation and worker failure handling are tested; universal
OOM resilience, global latency/RSS and wider dirty/redo atomicity remain open.
Convolution's native high precision is not extended to Blinds/SmallTiles/Retinex;
their U8/Gray limitations remain. Unsupported routes, Windows/macOS,
Wayland/tablets, full-source/license delivery, final candidate and whole-port
acceptance remain open. Unchanged A–D malformed/schema-drift/provenance/lifetime
results are reused by exact existing reports, not counted as new runs.
