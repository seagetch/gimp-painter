# Popup renderer pixel ownership (08.016/pixbuf-owner)

The legacy renderer's raw `pixbuf` member was never initialized. Its first
size request and destructor tested indeterminate storage; failed icon creation
was followed by unconditional width/height access, and its render method passed
the same pointer to Cairo. Regeneration manually released the old reference.
The six original source duties, immutable source identities, and file-specific
replacement decisions are recorded in `popup-pixbuf-owner.json`.

## Native replacement and ownership

`GimpCellRendererPopup` now embeds and derives from `GtkCellRendererPixbuf`.
The inherited `pixbuf` property is initially NULL; the popup initializer instead
sets `icon-name` to `GIMP_ICON_MENU_RIGHT`. Consequently, default NULL pixbuf does
not mean an invisible renderer: GTK draws the named icon. A NULL `pixbuf`
assignment clears the image definition, including a previously selected icon.

GTK retains a non-NULL property value; callers release their factory/getter
references independently. Replacing the property releases the previous image
reference, same-value assignment remains safe, and clearing/destroying the last
renderer owner releases its remaining image reference. External pixbuf owners
remain valid after renderer destruction. No extra `ObjectRef<GdkPixbuf>`, owner
slot, duplicate property, or custom pixel cache is necessary or introduced.

The inherited size and rendering virtual functions handle the empty image.
Tests compare the real renderer's vfunc addresses with its GTK parent and call
native preferred-size and Cairo rendering. Empty content has zero image extent
with the popup's zero padding, and leaves all destination pixels unchanged.
Valid content changes destination pixels. Each draw runs both LTR/RTL and
sensitive/insensitive combinations.

`PopupSlot` owns the popover and layer/image handles, not renderer pixels.
Tree destruction closes that slot, invalidates callbacks, and removes the
popover, while a retained renderer continues owning a usable pixbuf until its
last GTK column/handle reference is dropped. Native dispose chains to GTK.
The four retired popupbutton/popupper files contain no separate pixbuf owner;
their view and popup lifetime duties are distinguished explicitly in the JSON.

## Failure and destruction oracles

The focused native suite contains eight cases:

- `initial-and-empty`: default NULL property, named icon, fresh destruction
  before sizing, inherited vfuncs, repeated explicit empty rendering
- `replace-same-reset`: non-NULL ownership, borrowed and getter-retained
  same-value assignments, replacement, repeated reset and destruction
- `recovery`: valid image to empty to valid image; missing icon-name uses GTK's
  image-missing fallback; valid icon-name and explicit pixbuf recover
- `failed-factory-result`: actual `gtk_icon_theme_load_icon` failure returns
  NULL with `GTK_ICON_THEME_NOT_FOUND`, both on a fresh renderer and after a
  valid image; passing that result clears safely, draws nothing, then regenerates
- `caller-reference-survives-renderer`: renderer releases only its reference;
  an independently owned pixbuf remains usable and frees on the caller's unref
- `column-first` and `handle-first`: either native renderer owner can go first;
  image data survives until the second is released
- `retained-after-tree-close`: popup/store closure does not prematurely free
  image storage owned by a retained renderer

`GdkPixbufDestroyNotify` counts actual pixel-array deallocation once per image,
using owned buffers created by `gdk_pixbuf_new_from_data`. A GObject weak
notification alone is insufficient for that oracle because it can occur at
dispose before final storage destruction. Renderer weak notifications are
counted separately. GLib test warnings/criticals remain fatal; no NULL drawing
warning is expected or suppressed. No production fault injector is needed.

This checks recoverable missing-icon failure and NULL property input. It does
not simulate allocator failure/OOM or assert exact legacy icon/shadow visuals.
The failed-factory case deliberately assigns a real NULL factory result through
the supported inherited property; normal production icon-name loading and its
non-NULL fallback are covered separately, not conflated with that failure.

## Reproduction and evidence

Build the existing `app/tests/painter-popup-handles` Meson target. In a configured
native test environment with an X display, run:

```
meson test -C BUILD --print-errorlogs painter-popup-handles
meson test -C BUILD --print-errorlogs --test-args='-p /painter-popup-handles/pixbuf' painter-popup-handles
GDK_SCALE=2 meson test -C BUILD --print-errorlogs --test-args='-p /painter-popup-handles/pixbuf' painter-popup-handles
python3 -B tools/check_popup_pixbuf_owner.py
python3 -B -m unittest discover -s tools/tests -p test_popup_pixbuf_owner.py
```

Run the same native target in an ASan/UBSan build when available. LSan is a
separate claim. The checker verifies all six duty identities against the ledger
and original Git blobs, native owner/dispatch structure and the eight focused
test registrations. `--run LABEL=LOG` additionally requires every focused TAP
case to pass without skip, duplicate, failure or sanitizer diagnostics; raw-log
hashes and current source hashes are recorded with `--report FILE`. Without a
runtime log it reports source-contract verification only, not native acceptance.

The recorded run in `../tests/popup-pixbuf-owner/native-evidence.json` passes
30/30 registered cases normally and the 8/8 pixbuf cases at `GDK_SCALE=2`, with
no test skips. All five relevant native build inputs match the development
sources. Nine checker mutation controls pass. The preserved TAP and
`contract-report.json` bind the case results to source/log hashes. Missing
generated application-icon assets, disabled plug-in placeholders and headless
D-Bus diagnostics are summarized separately; they did not cause test skips.
No new ASan/UBSan build was available, so these results make no sanitizer claim.

GTK references: [pixbuf property](https://docs.gtk.org/gtk3/property.CellRendererPixbuf.pixbuf.html),
[NULL on missing icon](https://docs.gtk.org/gtk3/method.IconTheme.load_icon.html),
[pixel-data destroy callback](https://docs.gtk.org/gdk-pixbuf/ctor.Pixbuf.new_from_data.html).
