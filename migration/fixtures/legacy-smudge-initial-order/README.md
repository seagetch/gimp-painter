# Old Smudge initialization ordering

The complete pinned old Smudge source and unchanged complete paint orchestration
function are retained. The latter is compiled through explicit start/motion
observer stubs: successful start records a seed before motion, failed overlap
cannot initialize or invoke motion, and later visible motion does not restart.
The adapters do not emulate old pixels or claim an old application execution.

The full source independently shows clamped initial seeding and opacity-zero
return before accumulation update. Native fixed-color/rate100 tests use the
already captured old byte-kernel arithmetic: the seed remains unchanged and a
fully opaque subsequent dab exposes it. An actual positive-pressure native dab
observer and changed-byte check avoid mistaking insufficient spacing for a defect.
