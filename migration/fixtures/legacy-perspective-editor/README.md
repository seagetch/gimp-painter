# Source-only Perspective editor policy

Pinned revision `afa43fae3e920210146abed514f136fd49f671b5` supplies complete,
unchanged old `gimp_display_shell_zoom_xy_f` and `gimp_draw_tool_on_handle`
functions. The native test compiles these through `hit-oracle.inc`, exposing only
scale and offset state via tiny adapter types. Original PROJ_ROUND/RINT arithmetic
is explicit and checked against pinned headers. The circle CENTER adapter is the
unchanged center case (no shift); unused square/northwest execution is refused.
Tests call exactly CIRCLE/CENTER/13x13. The integer half-width is6 and the test is
strictly distance-squared<36 after independently rounded scaled coordinates.
No current geometry or hit helper supplies oracle results.

The complete old nearest-selection function is retained for source audit only;
it is not executed by this single-point hit oracle. The old nearest point is
selected in image coordinates before this display-scale circle test, with first
index winning equal distances. No old application or physical device is run.

`verify-source.py` verifies complete source hashes/Git blob IDs, exact function
bytes and line locations, macro excerpts, and the bounded adapter contract.
