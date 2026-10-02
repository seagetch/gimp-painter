# Painter CloneLayer / FilterLayer GTK3 entry points

## Audited source and scope

Reference: `../gimp-painter-legacy` at
`afa43fae3e920210146abed514f136fd49f671b5`.

- `app/actions/layers-actions.c:106–116` registers independent Filter and Clone
  creation, not GIMP drawable effects.
- `app/actions/layers-commands.c:394–444` opens the Filter creation dialog, while
  Clone creation immediately binds the active layer and inserts it above that
  layer in its actual parent (including when the source is a group).
- `app/actions/layers-commands.c:1087–1136` creates the independent Filter with
  user name/size, full opacity and the old REPLACE mode.
- `app/widgets/gimplayerpopup.cpp:120–214` finds image/drawable procedures from
  Filters/Colors menus, excluding several categories; `217–731` builds argument
  controls; `1022–1134` shows the definition and explicitly applies it. The old
  “Live update” checkbox is constructed without an implementation callback.
- `migration/contracts/clone-layer-port.md` documents the legacy cycle contract:
  retain references and last completed pixels, suppress recursive evaluation.
  The new source editor does not silently reject or rewrite those definitions.

This slice connects actual existing core types and their Undo APIs to GTK3.
It is not complete support for all former PDB procedures or the old canvas popup.
Only the independently implemented edge/Gauss compatibility executors are
selectable for new executable definitions. Unknown procedures/argument shapes
remain visible and unchanged through a Keep saved definition option, a typed
argument preview and raw byte preview (bounded display; complete data retained).
Nothing scans or dispatches arbitrary PDB/script/file procedures.

## UI contract

- Layer menu and layer context menu register `layers-new-clone`,
  `layers-new-filter`, `layers-edit-clone`, `layers-edit-filter` without removing
  standard actions. Default layer edit also selects the custom editor.
- Clone creation uses exactly one selected layer, preserves the actual source
  parent, and records normal image layer-add Undo. Painter Normal is used.
  Multiple selection, a selected channel and a floating selection disable it.
- The source chooser stores item IDs rather than names. It displays group paths
  and IDs to distinguish duplicates, explicitly refreshes its list, and validates
  source image/attachment at acceptance. A removed source leaves the dialog open
  with an error; Cancel and unchanged Keep current reference make no edit.
  Explicit source changes use the source Undo API.
- Filter creation allows name/size and creates an independent FilterLayer in the
  active parent, with Painter REPLACE obtained by raw-mode mapping `24`, not the
  standard modern floating-point REPLACE operator. Empty definitions can be
  created for later configuration, as in the old two-step flow.
- Edge and Gauss parameters edit through `gimp_filter_layer_edit_definition`.
  Unchanged OK makes no Undo and does not normalize metadata. Known same-procedure
  edits preserve original prefix arguments/types, float numeric types, and the
  five-slot edge form unless a non-Sobel algorithm is explicitly selected.
  The original raw bytes remain provenance even on explicit procedure replacement.
- Blur zero/zero is rejected before mutation. Runtime errors and asynchronous
  state are visible in the editor; RGB8 non-linear is currently the core's
  supported execution format. Other formats are not presented as executed.
- Dialog state holds weak image/layer references. Image disconnect and layer
  removal close dialogs; response revalidates attachment. Destroy is idempotent,
  and late status notifications do not touch destroyed controls. Existing
  per-image/per-layer dialog attachment keys prevent duplicate action dialogs.
- Child-widget signals use dialog-owned closures, including independently
  retained/detached widgets. Responses lease the dialog state across core
  notifications and check `closed` before any post-call UI work or insertion.
  Refresh leases its list model while clearing, since a `changed` callback may
  destroy the combo while `gtk_list_store_clear` is still executing.
- Unknown procedure/name/error text is repaired to UTF8 only at presentation
  boundaries; saved definition and typed/raw metadata bytes remain untouched.
- New visible strings are registered in `po/POTFILES.in`.

## Verification

`app/tests/test-painter-layer-ui.c` is a real GTK/core regression executable,
registered as `painter-layer-ui`. It returns SKIP77 when no display exists, rather
than claiming that headless omission passed. On 2026-10-02 it passed all sixteen
cases on the actual cloud Xfce GTK3 display, launched from its native terminal:

0. Source deletion during Clone factory signal reentry safely cancels insertion
1. Direct Clone action creation, correct group parent, repeated create, Undo/Redo
2. Clone Cancel, duplicate-name identity, source rename, real pixel following,
   source Undo/Redo and clearing
3. Deleted-source validation and retained legacy ancestor-cycle reference
4. Repeated Filter Cancel, zero/zero error, creation/type/mode and Undo/Redo
5. Unknown procedure and typed/raw preview, nonmutating OK, explicit replacement,
   raw preservation and definition Undo/Redo
6. Numeric edits retain float and five-slot edge arguments
7. Layer removal and image close destroy edit/new dialogs safely
8. Action enablement for no image, single/multiple selections and custom types
9. Eight retained child-widget signals after dialog destruction/finalization
10. The same eight widgets detached before destruction (independent lifetimes)
11. Clone core edit reentry destroys/final-unrefs editor and releases the outside
    image owner, both on successful edits and closed-binding failures
12. Filter core edit reentry destroys/final-unrefs editor and releases image owner
13. Filter definition creation notification closes editor before insertion
14. Clone Refresh selection notification destroys editor during list clearing
15. Non-UTF8 saved procedure/string preview opens safely without changing bytes

The source and built image-menu XML parse successfully and `git diff --check`
passes for this slice. Recorded native test output is
`migration/tests/painter-ui-native.txt`. The existing test-profile shutdown
writable-directory diagnostic also exists in earlier clone-layer logs; the
native harness exit status was 0.

Full compiled-app manual checks (fresh isolated profile, no user's profile):
creation actions are visible and initially disabled without an image; a new
image enables them; Clone creation adds a real layer and its GTK editor shows
Background as live source; Cancel returns cleanly; Filter creation offers the
compatibility options and makes a real layer; its edit dialog shows exact typed
arguments and a completed/background-update status. Layout was inspected in
native screenshots. The unrelated browser state was left untouched.

This full-app check detected an integration defect outside the dialog: a clean
Filter with a completed black thumbnail left its initially transparent canvas
cached until visibility was toggled. Toggling off showed white sources, toggling
back showed the correct opaque black result. The Filter core owner corrected
completion flush/projection. After rebuilding and restarting the app with that
fix, the same 1920×1080 RGB8 Clone+edge Filter creation visibly produced opaque
black immediately, without a visibility toggle or another user flush. A real
mouse-drag brush stroke on Background then appeared as a white edge outline
through the live Clone+Filter stack. Ctrl+Z cleared it; Ctrl+Y restored it, all
without forcing a refresh. These checks were observed in native screenshots.

`migration/tests/build_painter_ui_sanitizers.py` builds an isolated
ASan+UBSan version of the dialogs/actions/UI test translation units. It does not
replace shared objects, instrument all GTK/core dependencies, or claim leak
coverage. It must be executed with a display and the documented sanitizer flags.
The build report is `migration/tests/painter-ui-sanitizer-build.json`; all sixteen native GTK cases also pass under those flags; runtime output is
`migration/tests/painter-ui-sanitizer-native.txt`. Machine-readable scope/results
are in `migration/tests/painter-ui-results.json`.

## Review regression evidence

The initial nine cases did not cover independent widget lifetime or mid-response
closure. Review added seven cases and reproduced four failures before fixes:
a detached choice callback accessed freed dialog state (ASan); a failed Clone
edit touched an already destroyed error label; a Filter was inserted after its
creation dialog closed; and destroying the editor during Refresh freed the GTK
list model still being cleared. `migration/tests/painter-ui-review-before-fix.txt`
records those deliberate failures. Owner-bound closures, response state leases
and closed checks, plus a temporary model reference, fix these paths. The full
sixteen-case normal and UI-scoped sanitizer suites then passed. Both suites
were relinked against the Filter core definition-reentry guards and passed again
at 03:11 UTC, with exit markers recorded in the logs. Non-UTF8 preview
coverage additionally verifies presentation repair does not mutate stored bytes.

## Remaining boundaries

Full generic procedure editors/executors, the canvas popup/tile view,
platform/tablet QA, translated locale visual QA, comprehensive display timing,
and all-workflow XCF round trips remain separate gates. No Windows/macOS/tablet
claim follows from these Linux GTK tests.
