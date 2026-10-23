# Original25.010: rotation reset action

Original acceptance **状態と UI 表示が一致**, dependency25.009, is preserved.
The two original work rows refer to the old rotate implementation/header hunks;
full source/hunks and other owner IDs remain pinned in
`migration/tests/navigation-reset-action/audit.json`.

The existing registered `view-rotate-reset` action reaches the absolute callback
and public rotate-to-zero path. No new production patch is necessary. Six actual
UI reset cases plus the original50 navigation cases pass in both normal and
ordinary ASan/UBSan configurations, all below the original60-second per-invocation
budget.48 state/repeat scenes and8 immediate default/custom gesture reentries
exercise112 action activations per configuration. Native matrix/inverse, zero
angle/non-exact provenance, origin-cache invalidation, fraction clearing,
unchanged flip/zoom/pixels, and visible statusbar agreement are checked. At zero,
the rotation widget is intentionally hidden rather than displaying a stale angle.

Before/after evidence captures actual action activation on authenticated
1080x1920 Xvfb with oncanvas enabled. Earlier detector settings and the immutable
196-duty predecessor evidence are preserved, not mislabeled as new runtime.
All eight held candidate paths remain excluded from the tested private baseline.
Historical application runtime remains USER-SKIPPED; no physical-device,
cross-platform or whole-port completion is claimed. Original25.011/25.012 duties
are not closed by these bounded reset tests.
