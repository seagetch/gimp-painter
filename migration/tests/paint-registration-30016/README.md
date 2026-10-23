# Original consuming paint registration — bounded 30.016 child

Original `afa43fae3e920210146abed514f136fd49f671b5` exports
`void gimp_paint_register (Gimp *, GimpPaintInfo *)`: add to the application's
native paint-info container, then unref the transferred resource. The private
six-argument provider is renamed `gimp_paint_register_internal`, including the
startup registration callback. This restores those source-level contracts using
one native registry; it does not introduce another factory or C++ model.

Valid application, resource and live-registry arguments transfer one reference.
Invalid arguments are diagnosed and remain caller-owned. Application and registry
leases span native observers. The internal provider retains its local info lease
and only installs the standard paintbrush if the original registry still owns it.
Native paint-info and paint-options hold a deliberate mutual-reference pair;
fixtures use the same explicit dispose teardown as `gimp_paint_exit` and assert
that only the options back-reference remains after removal. The first fixture
incorrectly assumed immediate destruction; its failure is retained, not called a
production regression.

Evidence: genuine baseline missing-symbol link failure; 9 native groups and the
same 9 under selected seven-unit ASan/UBSan; 423 compiled C/C++ header probes;
retained-transfer and omitted-registration mutants rejected. Native tests cover
exact signature/consumption, C++ RAII release, invalid arguments, duplicate
pointer, notification removal, nested registration, registry detachment,
caller-last-owner release, and unchanged 15-provider/standard-paintbrush catalog.
Removal is tested through `items-changed` after native count bookkeeping, not an
unsupported claim that arbitrary synchronous `add` handlers may remove entries.

The header was in the immutable baseline index but not the reviewed compilation
closure. The initial gate rejection is retained. A changed-baseline standalone
route pins the exact original blob and adds two probes; no immutable inventory
was rewritten. C++ keeps the no-RTTI ABI. Dependencies are not fully instrumented;
vptr and leak checking are disabled as in earlier native tests.

Actual authenticated Xvfb 1080×1920×24 regressions: preset UI18, perspective UI24,
feature lifecycle11 and prior preset entries6. The PNG shows the actual native
preferences page selected through its GtkTreeView cursor with oncanvas enabled;
it is not a new pointer-click or painting claim. Build uses clean historical
baseline77bcdf2b7f plus owned committed overlays, not the whole concurrent branch.

`original-five-duties.json` retains exact five original hunk records and ties their
bounded registration behavior to this child. The other eight duties, old dynamic
feature registry/signal ordering and original dependencies remain open under the
original 30.016 parent and existing audit owners. No old C++ PaintInfo layout or
binary ABI claim. Previous evidence remains immutable.

Run `python3 migration/tests/paint-registration-30016/validate.py --check-source`.
