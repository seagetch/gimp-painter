# Warmed native active-pipe reference

Pinned source: afa43fae3e920210146abed514f136fd49f671b5. This capture links the
old application archives and does not use the port renderer. The same synthetic
stimulus is separately compiled as the modern app/tests/painter-mypaint-pipe-trace.

- 32 scenes: eight native selector modes × smudge/no-smudge × incremental/nonincremental
- 416 full nonlinear-u8 RGBA snapshots (64×48), including Undo/Redo
- 1,920 native selector observations with consumed current coordinates and defined warmed previous coordinates
- 416 index/current-child/selection-count/Undo-depth records
- 32 Undo result pairs and 32 next process-global RNG values
- 2,817 records, 10,409,801 uncompressed bytes

The observer never reads the first undefined previous coordinates on a fresh
old Surface. Warm-up uses constant selection. Static old core storage defines
its otherwise uninitialized initial options pointer. Each new drawable is warmed
incrementally before nonincremental is enabled. Cold uninitialized state is not
an oracle or a behavior to reproduce.

The first extended capture reproduced the old cold nonincremental drawable-switch
crash before that additional warm-up. Its source and failed output are kept in
cold-nonincremental-drawable-switch/, explicitly excluded from successful parity.
Do not rerun that failed setup as part of normal fixture verification.

capture.json records behavior-source, archives, harness and executable hashes,
compile command, result and diagnostics scope. reproduction.json confirms an
independent second run reproduced the full values exactly. Compressed runtime
logs include known startup warnings; these are not concealed or described as
sanitizer success. Raw fixture values are independent of runtime log timestamps.

Reproduce from the repository root, using the prebuilt pinned old environment:

    source /workspace/shared/gimp-legacy-build/env.sh
    flock /workspace/shared/gimp-painter-build.lock python3 migration/tests/capture_mypaint_pipe.py --verify-fixture --output /workspace/shared/mypaint-pipe-recapture

Normal port comparison uses migration/tests/compare_mypaint_pipe.py. All consumed
current axes, pixels, counts, index/child, Undo and RNG records must match exactly.
It separately reports previous-coordinate differences, since both built-in old
and current native pipe selectors ignore last_coords. Raw old fields remain in
the fixture. The current transient providers intentionally retain their defined
cold defaults; arbitrary custom selectors are outside this native built-in proof.
