# Legacy MyPaint Surface runtime observations

Oracle: real rebuilt pinned application archives, source commit
`afa43fae3e920210146abed514f136fd49f671b5`. Separate harness executables reuse those
archives without editing or rebuilding feature sources. `capture.json` verifies
behavior source bytes against the commit and records source/archive/executable
SHA-256, compiler commands, value hashes and precise scope. Compressed link plans,
compiler output, stdout and stderr accompany both captures. No inherited process
environment is included. `manifest.json` seals every file in this package.

`capture-surface.cpp` calls the actual old TempBuf Surface using deterministic
synthetic 23×17 RGBA pixels, six dabs/samples and eight combinations of ellipse or
asymmetric bitmap brush, RGB paper and incremental/nonincremental mode. It records
all bytes after every dab. Inputs cover negative coordinates, canvas edges,
hardness/radius/angle changes, erasing, partial alpha lock, overlapping dabs and
separate stroke opacity. Two additional transparent lock-alpha captures record the
pinned executable's hidden-RGB outcome: zero in touched incremental pixels,
unchanged in nonincremental composition. The port defines that measured result
without relying on the old NaN-to-byte conversion. Total: 148 Surface records.

`shape-masks.tsv` records 24 actual old bitmap transform results. The port computes
these independently and asserts equality before using its own masks.

`capture-generated.cpp` invokes actual old GimpBrushGenerated transformation for
72 combinations of shape, even/odd spikes, aspect and six transform parameter
sets. These are real old-code executions with synthetic stimuli, not tablet traces
or historical user artwork. They do not prove full drawable/session/Undo parity.

Reproduce with the existing legacy build environment loaded:

    python3 migration/tests/capture_mypaint_surface.py

The capture uses make's dry-run link plan and disposable compiler outputs; it does
not replace the legacy test executable or feature archives. Golden comparisons
are Meson painter-mypaint-legacy-surface and painter-mypaint-legacy-generated.
