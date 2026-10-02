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

## Unified controller ownership (31.016)

The production controller is now `app/dialogs/painter-layer-dialog.cpp`, with
unchanged C entry points in a C-linkage header and matching Meson/POT paths.
The native `GimpViewableDialog` owns one typed `DialogSlot` in the existing
`BindingStore`. There is no independent `painter-layer-dialog` data key,
second store, or raw implementation pointer in signal closure data.

Construction happens through `emplace` and a scoped `initialize`; ordinary
callbacks dispatch only after activation and obtain a scoped `with` borrow.
Every signal registration is a `Connection`. Its small closure payload contains
only a weak native-owner reference and generation, copied before invoking any
reentrant UI/core code. The operation keeps a strong owner lease, so a callback
can destroy the dialog and drop the caller's last reference without invalidating
the in-flight borrow. No owner ref is retained by the implementation itself.

`GtkWidget::destroy` explicitly invokes the shared idempotent close operation
before child teardown. Close invalidates callback generations, disconnects
connections and clears image/layer weak references; slot destruction remains a
finalization responsibility. This does not assume any generic GObject dispose
signal. A direct `gimp_painter_binding_close(dialog)` also makes the controller
inert while native widgets remain alive. The Clone/Filter argument loading,
unknown-byte preservation, validation, source identity and Undo paths are still
the same production functions. The list model and synchronously notifying
stack/status/error widgets have operation-local leases during reentry.

Five additional native cases extend the sixteen-case suite:

16. Explicit common close, repeated close/destroy, detached callback and retained
    native response on new Filter, edited Filter and Clone, with no late mutation
17. Filter choice callback destroys and drops the last dialog owner from a
    nested stack-visible-child notification
18. Filter status callback destroys and drops the last dialog owner from a
    nested label notification; late model signals remain harmless
19. Real image disconnect destroys and drops the last external dialog reference
    while the owner-leased signal callback is still running
20. Factory reentry drops the last external image reference during native child
    construction; initial activated source selection also common-closes the
    controller without directly destroying the window. Both return NULL with
    expired weak dialog pointers and no orphan native window

Verification evidence and exact tested source snapshots are recorded separately
from the earlier sixteen-case evidence above. The new sanitizer builder
instruments the dialog, layer actions/commands, test and common BindingStore
lifecycle. Remaining production C++ is rebuilt only for compatible RTTI/vptr
metadata, replacing every thin-archive alias; the report distinguishes these
units from instrumented sources. Upstream GTK and other core/dependencies remain
uninstrumented and leak detection remains disabled. A content-addressed immutable
source archive and binary/source SHA-256 hashes bind each result to its actual
build rather than claiming later concurrent tree edits were tested.

All twenty-one cases passed in both the normal and focused ASan+UBSan/vptr native
GTK runs on 2026-10-02, each with exit 0. Results are in
`migration/tests/layer-dialog-store-results.json`; runtime logs are
`layer-dialog-store-normal.log` and `layer-dialog-store-asan.log`. The immutable
source archive is `layer-dialog-store-sources-e508d4e4d095df3a.tar.gz`, with
complete build scope/commands/hashes in `layer-dialog-store-sanitizer-build.json`.
The known isolated test-profile writable-directory shutdown diagnostic appears
in both passing runs. Initial C++ sentinel-cast and concurrent incomplete-header
build failures were preserved rather than overwritten; neither is represented
as a runtime pass. `git diff --check` passes for this controller slice.

Factory review extended the original twenty-case seal with input-reference and
inactive-result cleanup. All four inputs (image, layer, context, parent) are
leased before native construction, then explicitly released while the dialog
lease remains alive before the final active check. A controller closed during
construction/activation is destroyed before NULL is returned. The original
20+20 passing logs, manifest, sanitizer report and immutable archive remain under
`layer-dialog-store-initial20-*` and `layer-dialog-store-sources-5688a88ed8f79615.tar.gz`; they
are historical evidence and do not claim to cover this final factory fix.
