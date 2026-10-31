# Owned-doc lifecycle proof — isolated CI experiment

This independent hosted CI job is staged for validation, not executed or accepted.
It does not alter GIMP production dependencies, locks, or existing acceptance
gates. A successful proof supports a later dependency decision, not whole-GIMP
acceptance. Standard-library validator units and syntax checks are the only
locally completed validation.

## Basis and provenance

Pinned inputs are committed under `ci/babl-proof-inputs/`; see its
`provenance.json` for public source references, exact hashes, and signature limits.
The source archive is upstream babl0.1.114 as distributed in Debian's source pool.
The patch is the six-added-line GNOME destructor diff, unsigned; its exact raw
diff bytes need not equal a mail-formatted patch response. Do not silently replace
the committed input with a URL fetch. Historical Debian InRelease/Source index
verification was recorded, but dsc signature verification was not completed.
No new signature verification is claimed by this job.

## Matched recipe

Both arms build exact upstream 0.1.114 source, same runner, compiler and options;
only `babl/babl-format.c` differs. Never compare Debian packaged binaries to the
patched source binary. Meson `debugoptimized`, lcms enabled, docs/GIR/VAPI disabled,
SSE4.1/F16C disabled match prior normal recipe. Other CPU/module options retain
upstream defaults. Build uses `--wrap-mode=nodownload` and a fresh private prefix.

Explicit C flags: `-O2 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls
-fstack-protector-strong -D_FORTIFY_SOURCE=2`. Meson adds
`-Db_sanitize=address,undefined`; probe uses `-fsanitize=address,undefined`.
Link flags `-Wl,-z,relro,-z,now` preserve RELRO/BIND_NOW. Installed owned ELF and
probe must have both, ASan instrumentation, and **no RPATH or RUNPATH**. Meson's
intermediate build RUNPATH is not deployed or used by the probe. Resolution uses
only the arm's explicit `LD_LIBRARY_PATH` and `BABL_PATH`, not inherited workspace
paths. Record compiler/tool versions and full flags; this is a matched within-run
experiment, not a reproducible compiler/package lock across dates.
Recipe identity includes SHA-256 of all six proof files, compiler path/hash, an
explicit package-name allowlist, public CI run/image fields (256-character bounds),
kernel system/release/architecture (no hostname), and first-CPU vendor/family/model/
stepping/flags (bounded). No raw environment, secrets, hostnames, serials, or network
configuration are captured.

Instrumented closure: probe, babl core, every installed babl extension. System
libc/libm/loader/lcms and GCC sanitizer runtimes are not rebuilt/instrumented;
actual loaded paths and file hashes must match across arms for the external
closure. Linux vDSO is kernel-provided and explicitly exempt from file hashing.
`dl_iterate_phdr` snapshots extensions while initialized, before unload, without
keeping address roots that could conceal leaks. Both installed and observed loaded
module-name sets must match, with a nonempty extension set. This proves the same
observed lifecycle module set, not exhaustive execution of every extension.

## Gates / failure semantics

* Inputs and exact patched-file digest checked; source manifests differ only in
  the single destructor source file. Fresh output directory required.
* Every command gets stdout/stderr/exit evidence. Timeout, signal, build failures,
  missing modules, unexpected loader resolution, wrong hardening, non-leak
  sanitizer error, or mismatch fails the job (no `continue-on-error`).
* Original: lifecycle must reach both markers; exact exit 23, exactly one LSan
  report, positive leak totals, every allocation block includes `babl_strdup`
  and `babl_format_new` at the pinned original doc allocation line 377. Unexpected diagnostics or totals fail closed.
* Patched: both markers, exit 0, empty stderr. Leak detection remains enabled.
* ASan/UBSan exit codes 24/25 distinguish them from LSan 23; no suppressions,
  disabling leak detection, debugger/ptrace or local sanitizer execution.
* `result.json` exists only after every gate succeeds. CI artifact on failure is
  diagnostic evidence, not a success certificate. A strict-format rejection
  requires reviewing raw output; do not weaken gates to make it green.

## CI integration and evidence

The independent `babl-owned-doc-proof` job in
`.github/workflows/painter-native-ui.yml` runs the committed recipe on a standard
GitHub-hosted Ubuntu24.04 runner. It does not depend on or replace the GIMP
Choice/partition gate or original-provider negative control. Its hosted guard
rejects local execution and must not be spoofed.

Inputs and recipe must remain reviewed together. The job runs validator units,
then `prove.py --archive ci/babl-proof-inputs/babl_0.1.114.orig.tar.xz
--patch ci/babl-proof-inputs/fe7fcc23326680428e4ae9dbb3c41bed033efa9f.patch
--output proof-output`. The artifact `babl-owned-doc-proof-<commit>` preserves
results and failure evidence. Review the exact commit's job conclusion, raw logs,
installed/loaded hashes, module lists, source delta, and `result.json`.
Production dependency adoption and GIMP acceptance remain separate decisions.

Completed local validation: 11 standard-library unit tests and Python
AST parsing of all three Python files. Regression tests reject unexpected non-frame
text inside leak stacks and verify identity metadata allowlisting/bounds. Each
nonblank leak stack line must have a sequential frame index and hex address.
Pending: all compile/runtime checks and CI execution. No acceptance
claim is made. Future runner package/action availability and strict symbolization
format are unverified; failures remain failures rather than allowed exceptions.
