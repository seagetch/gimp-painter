# Direct native layer-filter popup

This bounded change aligns the existing on-canvas FilterLayer options entry and custom layer-tree dropdown with the source-derived gimp-painter 2.8 presentation contract. It does not change ordinary Colors-menu plug-in dialogs, the new-filter creation dialog, non-filter/clone popovers, or unrelated native layer context menus.

The exact legacy reference is `seagetch/gimp-painter:gimp-2-8` at `afa43fae3e920210146abed514f136fd49f671b5`. The old application was not executed. See LEGACY-CONTRACT.md for the distinction between proven source semantics and modern safety guarantees.

The intended production route is one actual native `GTK_WINDOW_POPUP`, directly containing live layer properties plus the shared existing filter controls and explicit Apply. There is no preliminary settings action, hidden child dialog, manual test-only popup wrapper, or separate framed editor. The typed state remains owned by the actual GimpViewableDialog-derived popup.

Filter parameters are staged until Apply. Apply keeps the host open; later unapplied edits disappear on Escape/outside dismissal. Previously applied definitions and live layer-property edits remain. Operation changes reload saved values for the stored operation or defaults for a different operation, not a per-operation draft cache. The layer carrier consumes outside presses, unlike the separately ported MyPaint carrier.

The port retains its validated supported-operation subset, exact-text Colorify RGBA controls, Undo/no-op handling, weak ownership and revision safety. It does not claim all old procedure-generic controls or old visual layout are reproduced. In particular the inspected legacy RGB panel had no argument-change binding; this change does not recreate that broken control or introduce a new color-picker dialog.

Final acceptance: 270 distinct native cases, 4 expected negative controls and 17 scoped sanitizer cases. See acceptance-summary.json and the per-suite receipts. Preliminary and superseded runs are not added to these counts.

## Implementation boundaries

- `app/dialogs/painter-layer-dialog.cpp`: private native popup subtype and shared existing control builder; normal dialog/new-filter constructor stays separate. Apply uses common attachment/revision/operation/draft guards, acknowledges only its exact installed revision, and does not rewrite widgets while acknowledging. Uncertain post-publication results disable Apply until reopen, rather than treating arbitrary latest data as clean.
- `app/widgets/gimppainterlayertiles.cpp`: existing FilterLayer properties route opens the composite directly, preserving non-filter/creation popovers.
- `app/widgets/gimpcellrendererpopup.cpp`: existing custom dropdown opens the same host for the clicked FilterLayer, not a later selected target; unrelated native context menus are not converted.

Portrait PNGs are raw snapshots of one real default-oncanvas fixture before and after Apply. The test resizes only the main window; the popup is positioned by production code. Input coverage uses pointer/keyboard-device GTK event dispatch, not physical X-server input. The same source has a separate negative run against the preceding published implementation. See portrait-capture.json and final test receipts for precise scope.

The final isolated-route regressions specifically cover the preserved-argument patch branch: three changed Apply operations remain usable, unchanged Apply adds no revision/Undo, and post-publication draft reentry disables unsafe retry. This branch must report an attempted commit to the outer acknowledgment logic rather than treating its null argument return as a no-op. Tests run all four existing isolated routes with genuine registered plug-in metadata and retain scalar, array, flag and enum checks. Two negative controls using the earlier candidate's exact core object demonstrate that the strengthened tests detect both the lost rebaseline and unsafe postcommit retry. These controls are separate from the two preceding-published-version entry negatives and are not counted as passing native cases.
