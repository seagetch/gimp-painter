# Exact committed integration checkpoint: FAIL, not acceptance

Source `a84cd0b051e7b648ec9519ede82ae9541c44ea1c`, freshly extracted from Git.
Every one of 15,806 tracked blobs matched the commit before build and the
SHA256 post-run seal. External gimp-data asset bytes are pinned separately;
matching the declared gitlink commit is not inferred from that asset snapshot.
No held candidate source was copied into this build.

17 selected native targets compiled; **15 passed, 2 failed**, zero skips or
timeouts. Authenticated Xvfb was verified at1080×1920×24. This is a normal build,
not a sanitizer run or all-branch/all-WBS certification. Existing oncanvas UI
checks remain in the actual UI fixtures. Static source gate passed2520 sources
and717 reviewed object-data sites, with4 known dormant legacy findings.

The unchanged native tool save-pending test exposed a real integration bug:
the new controller pending handler labels a finishable open stroke unsafe for
save preflight. Saving must still finish it via the existing saving signal.
The paired UI fixture separately lacks a default screenshot directory in its
registered Meson invocation. Neither assertion was weakened. The new bounded
paired child is reopened until these integration defects are corrected;
earlier scoped17/17 receipts remain valid historical evidence.

The driver intentionally returned failure. Its final source check was then
performed read-only and recorded manually because its success assertion had
terminated before that check. All recorded source hashes remained unchanged.
Subsequent isolated fixture overlays are not part of this checkpoint.
