# Native original popup-window factory checkpoint

Scope: `08.018/native-popover-factory`, original
`GtkWidget *gimp_popover_new(GtkWidget*)`, not GtkPopover and not the paused
cell-renderer/decorator candidate. See `migration/contracts/native-popover-factory.md`.

Final native and bounded sanitizer receipts run in the separate clean aggregate
workspace based on committed `513bec9490`, with only this provider/test/header
registration overlaid. `paused-source-exclusion.json` verifies five paused paths
against that committed baseline before each build. This is not a final-current
whole-repository aggregate. GTK and unlisted Gimp/dependency code are not
instrumented; RTTI-only bridge sources are enumerated separately; LSan is off.

Tests: exact C factory/type/window/frame/view contract; invalid input ownership;
repeated map/hide/grab balance; cancel/confirm action defaults and veto; Escape
with disabled Return/space confirmation; strict outside bounds and child/other
window targets; map/cancel/insertion destructive reentry; C++ retained GTK
ownership; C-compiled fault driver for all four exception kinds at initializer
and post-activation, plus closed raw-instance disposal; actual independent popup
above a real Gimp image with oncanvas enabled. The root-window PNGs preserve the
verified 1080×1920 Xvfb framebuffer, including the separate popup top-level.

Expected-red confirms the original symbol was absent. An earlier harness compile
omitted widgets-types.h; that diagnostic is retained. The initial handle test
incorrectly assumed a weak pointer survives GTK destruction while another strong
reference exists. GTK clears weak pointers during dispose; the corrected test
uses qdata finalization plus retained-object checks and does not change production
ownership to accommodate the mistaken assertion. That failed receipt is retained.

The first primary-workspace runs were not clean-source acceptance: parent review
found paused renderer/canvas sources present in that shared workspace. The pending
sanitizer job was interrupted, primary results were segregated as diagnostics,
and every final run was rebuilt in the committed-source workspace. The fixture
never explicitly invoked the renderer, but no clean aggregate claim is based on
those earlier primary runs.

The original full source files are immutable source oracles, not executed GTK2
binary evidence. Original C++ show/show_over/placement/view-replacement/decorator
consumers remain explicitly owned by `08.019/popover-placement-and-decoration`;
no old class-layout ABI or full parent-task completion is claimed.

Run `python3 -B validate.py --check-owned-source` for immutable archive/receipt and
narrow current-owned-source checks. `--check-source` additionally compares every
checkpoint input against the current checkout and can legitimately fail after
independent commits. Inherited Meson environment variables are scrubbed; only
explicit test/runtime configuration keys remain in deliverable JSON.

Sanitizer build preparation initially found unbuilt ordinary compile-database
objects for the standalone XCF compatibility target, then alternate HTTP GUI
archive/filter worker entries. Those diagnostics are retained; only the exact
missing objects (and the small owning XCF target) were compiled before retry.
No test exclusion, relaxed sanitizer flag, or paused candidate was introduced.
