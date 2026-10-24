# Original public MyPaint enum boundary

Scope: `08.018/mypaint-public-enum`, a bounded child of the still-open original
C-entry migration. The original source pin is
`afa43fae3e920210146abed514f136fd49f671b5`.

The legacy `GimpMypaintBrushMode` is a public `libgimpbase` enum, not an
application-private substitute. Its values are NORMAL=0, NORMAL_AND_ERASE=1,
LOCK_ALPHA=2 and COLORIZE=3; its runtime name, nicks, descriptions, translation
domain and translation context are preserved. `gimp_mypaint_brush_mode_get_type`
is exported from the library, and `libgimp` includes it in its enum registration
list and type-name list. The generated PDB enum map contains the same four
values; this is enum metadata wiring, not a PDB transport compatibility claim. The application's `brush-mode` property now uses that single registrar.
The compatibility Options header imports the public enum instead of declaring a
second copy. There is no second GType or model store.

The source `.def` export addition is intentional: the pinned old `.def` did not
list this getter despite the original public header and C registrar. This is
source-export coverage, not a Windows link/runtime or GIR guarantee.

The existing constant Options helper remains unchanged: after its original
instance guard it returns NORMAL, regardless of the stored property. Native
configuration still omits the default NORMAL property and serializes the other
three values using their existing nicks. No default/property behavior change is
intended. The separately committed `options-mode-entry` evidence is immutable;
this successor reruns all six helper cases against the moved public enum.

Tests distinguish a standalone C11 library consumer from C++14 application
consumers. Fresh subprocesses exercise library-first and app-class-first type
registration, identity, metadata, all four stored modes and configuration
round-trip. The model tests use authenticated 1080x1920 Xvfb and explicitly
activate oncanvas preferences; they are not GUI interaction/screenshot proof.

Unmatched original `current_brush`, mutable borrowed-view ownership, copy-props,
readiness history and GUI semantic differences remain open. This restoration
cannot close parent 08.018 or imply those APIs are equivalent.
