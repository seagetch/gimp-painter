# Original C brush construction and type entry

This bounded component connects canonical entry-map rows 41 and 43 to the
existing native resource. It does not complete parent 08.018.

The pinned old factory ignores context, requires a non-NULL/nonempty name,
and constructs a transfer-full named GimpData with MIME
`application/x-mypaint-brush`. The restored original C signature retains those
requirements and calls the existing native factory; get_type returns its one
native GType. There is no extra resource, type, slot, global registration object
or Interface::cast. The existing owning handle's create method now delegates
to this original C factory. Ordinary native factory callers remain unchanged.

The return is nonfloating. The resource owns its name and its independent
BrushSlot settings; it has no file association. Invalid names trigger the
original GLib precondition policy and return NULL. Native construction failure
is contained by the existing factory boundary. The native GimpPainterMybrush
runtime name, class layout and resource representation are intentional
migration differences, not restored old subclass or binary ABI.

Fresh ordinary Meson `gimp-painter-mybrush`: **28/28 TAP cases passed**, including
all 25 original cases and three new factory cases. The C caller really compiles
as C11, provider and handle as C++14. Function-pointer/static type checks and
unmangled linked symbols establish C/header/link compatibility. Tests cover
both NULL and native contexts, type identity, name copying, MIME, lack of file,
resource independence after mutation, two separate finalizations, original
NULL/empty failure diagnostics, and owning-handle state mutation/finalization.
The existing suite also loads/copies all 177 bundled resources.

The runner verified authenticated loopback Xvfb at 1080x1920x24, rejecting an
empty authority file. This is a non-UI change; no visual screenshot is claimed.
The test emits existing synthetic plug-in dummy-file/profile diagnostics;
`native.json` retains stdout/stderr with workspace paths tokenized, excluding
the inherited environment. It exits zero. No fresh sanitizer result is claimed.

Reproduction in the configured native environment, under the shared build lock:

```
ninja -C BUILD app/tests/gimp-painter-mybrush
python3 tools/run-painter-xvfb.py meson test -C BUILD --no-rebuild \
  --print-errorlogs --logbase public-entry-mybrush gimp-painter-mybrush
```

`receipt.json` seals all five exact built source/test files, native binary,
selected compiler commands, TAP count and scope limits. The broader 76-row
review and reproduced pre-existing aggregate checker failures are recorded in
`../../public-entry-routing/`. No legacy allowlist, tasks or progress statuses
are silently changed by this component.
