# Borrowed native standard-brush entry (bounded row 40)

The original C `gimp_mypaint_brush_get_standard(GimpContext*)` name/signature
now delegates to the existing native standard provider. The owning handle's
`get_standard` **retains** its transfer-none result rather than adopting it.
There is one native singleton, one weak cache and one resource implementation;
no extra standard resource, model, Interface::cast or static class registration
is introduced. The native provider implementation is unchanged.

## Intentional limits

Canonical entry-map row 40 remains **PARTIAL**. The native display name is
`Painter Standard` and internal collection identifier is
`gimp-painter-mybrush-standard`; the pinned old implementation used `Standard`
and `gimp-mypaint_brush-standard`. This component does not rewrite native
resource identity to appear source/binary compatible with historical saved
references, or claim legacy class-layout/default-settings parity. Parent
08.018 and core-extension-hooks remain open.

A retained closed standard remains the same cached object until finalization;
closed reads remain available and mutations fail under the existing native
binding policy. A new default resource appears only after actual finalization
clears the weak cache. This preserves the native owner-thread provider contract;
it is not a new concurrent singleton or shutdown-lifetime guarantee.

## Cold lifecycle fixture

The native provider has a weak pointer but leaves its original construction
reference outstanding. An ordinary borrowed caller is **not** entitled to
unref that seed. Testing weak reset inside an initialized application could
consume someone else's ownership and is deliberately avoided.

Only the `named-standard/cold-recreation` g_test subprocess skips application
startup. It confirms that its pristine provider object has exactly one seed
reference before acquiring two owning handles. The isolated fixture explicitly
retires that known seed, proves that either retained handle keeps the object
alive, then observes finalization and weak-cache recreation after the final
handle releases. A second generation is proven active by successful mutation
and independent finalization; pointer-address inequality is not required,
because allocators may reuse an address. Closing the first generation confirms
that the provider does not silently replace a still-retained closed resource.

The ordinary C test keeps its borrowed references untouched. It checks the
exact function-pointer signature, repeated native/original/context identity,
unchanged reference count, native name/internal identifier, clean state,
nonfloating ownership and absence of a file association. The subprocess uses
`g_test_trap_subprocess`, available within the project's GLib 2.70 floor; no
newer with-envp API or live-application singleton manipulation is used.

Fresh ordinary Meson native validation passed **30/30 TAP cases**: all 28
previous cases plus the ordinary C and cold-subprocess standard cases.
Results are recorded in `receipt.json` and environment-free
`native.json`. This is a non-UI ownership change; no screenshot or fresh
sanitizer result is claimed. Reproduce under the shared build lock:

```
ninja -C BUILD app/tests/gimp-painter-mybrush
python3 tools/run-painter-xvfb.py meson test -C BUILD --no-rebuild \
  --print-errorlogs --logbase public-entry-standard gimp-painter-mybrush
```
