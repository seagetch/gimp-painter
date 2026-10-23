# Independent Text toolbar

## Scope and source correspondence

This is a bounded child of `29.010/standard-tool-options`. The pinned source is
`afa43fae3e920210146abed514f136fd49f671b5`,
`app/tools/gimptextoptions.c:499–686`: `gimp_text_options_gui_full(TRUE)`,
`gimp_text_options_create_view`, and `gimp_text_options_gui_horizontal`.
It is not completion of the broader legacy UI/window/session restoration.

The new `app/tools/gimppaintertexttoolbar.inc`, included in the existing ordinary
GTK3 toolbar translation unit, supplies these fresh, independent controls:

| Legacy control | Current binding/location |
| --- | --- |
| Font resource and popup presentation | Inline current `GimpFont` context resource selector; `font-view-type`, `font-view-size` |
| Font size and unit | Inline `font-size`, `font-size-unit`; current `GimpUnit` object model |
| Use editor | Inline `use-editor` |
| Antialiasing | Inline `antialias` |
| Text color | Inline and Details independent panels sharing context `foreground`, current `GeglColor` |
| Hinting | Details `hint-style` |
| Justification | Details `justify` icon radio group |
| Indentation | Details `indent` |
| Line spacing | Details `line-spacing` |
| Letter spacing | Details `letter-spacing` |
| Box mode | Details `box-mode` |
| Language | Details `language`, conditional on `HAVE_ISO_CODES` as in the old factory |

Both legacy text-color access points are retained as distinct property-bound
panels, with independent dialog ownership. GTK3 top-opening
popover/scrolling replaces the old popup implementation; stock icons become
current symbolic icon names. Current font-loading sensitivity/busy label,
`show-on-canvas` and `base-direction` are modern additions, not restored legacy
controls. Modern outline/style/stroke controls remain in the ordinary dock.
No legacy text rendering, font substitution, saved text-layer compatibility,
physical input, exact geometry, cross-platform, or whole-session parity is
claimed by this slice.

## Ownership and values

The existing dispatcher admits the canonical tool options. The ordinary
factory constructs fresh widgets on every call and does not call the native
options GUI constructor. Its top box owns an options reference. It never
assigns `GimpTextOptions::size_entry`, which remains the native dock's handle,
and never reparents a dock descendant. The direct builder can be used before
the native dock exists without introducing that handle.

Font resource values use current `GimpFont` objects, rather than legacy font
strings; colors use `GeglColor`, rather than `GimpRGB`. Foreground context
colors are opaque, consistent with the current context setter. Font units
are current `GimpUnit` objects. The font size is a value in its selected unit,
not a pixel-valued rectangle-style property.

Each size entry independently follows the user context's image and that
image's `resolution-changed` signal. Y resolution matches the native text
tool's font-size axis. No image uses 72 DPI. Switching images detaches the old
image callback, and image ownership is weak. Resolution updates block only
callbacks carrying the options object, preserve canonical font size, and
restore the property's value bounds in the selected unit and the double-precision
value after the underlying size entry's internal float conversion. This prevents
large physical sizes from clamping against obsolete pixel bounds. View destruction disconnects both context and image before
freeing metric state; a destroyed retained entry cannot continue observing
image changes. No mutable global widget state is introduced.

The independent size entry replaces only its own generic outgoing size/unit
callbacks and incoming property notifications with a toolbar-local binding.
Intermediate numeric changes during unit conversion do not update the model
under its old unit. Completed conversions clamp to the font-size property's
0..8192 range before publishing size and unit together; converting 5000 points
at 300 DPI to pixels therefore yields 8192 pixels, without an invalid property
write. Every unit/model update restores the full numeric range in the selected
unit, so a small pixel size converted to points can immediately be edited to
5000 points without waiting for another resolution change. Native/shared
size-entry code is unchanged. The unit regression covers both edges and
programmatic paired size/unit changes.

## Prepared tests and acceptance status

Five tests are registered in the existing `painter-compact-options` target:

- `independent-text-toolbar`: two separate widget trees, native descendants
  and `size_entry` identity, font/resource-popup presentation, size, booleans,
  enums, justification, signed spacing, language and color binding, reverse
  native edits, bounded notify counts, one-view teardown and survivor updates.
- `text-toolbar-model-before-view`: direct factory before a native dock handle,
  canonical dispatcher rejection of duplicate options, actual font factory
  async loading/ready states including `show_all`, and model/view finalization.
- `text-toolbar-units-images`: two views, distinct image X/Y resolutions,
  image switching, inactive-image changes, 5000/8192-point bounds at varying
  DPI and on image switching, point/pixel conversion in both
  directions, empty-image fallback, no model notify during metric updates,
  weak image lifetime, surviving-view updates after peer destruction, and an
  explicitly retained destroyed size entry staying disconnected through later
  image/resolution changes.
- `text-toolbar-native-units`: canonical options, cached native size entry and
  two independent entries at 300 Y DPI; value edits and bidirectional unit
  conversions initiated by each of the three controls. Native resolution is
  initialized using the same public call as the text tool, not canvas editing.
- `text-toolbar-canvas-popup-lifetime`: actual shell, registered native Tool
  Options Dockable and editor, independent bottom toolbar, top Details popup,
  scroll-to-reveal, mapped/grab state, Escape/reopen, inline and Details color
  Cancel/OK plus open-dialog teardown, active
  tool switching, native identities/parents/visibility, and popup/color-dialog
  finalization across repeated cycles.

The source/API review and whitespace check are complete. No build or runtime
pass is claimed here: the coordinator owns the shared build lane and must
record focused, full-target, HiDPI and any screenshot/sanitizer evidence after
actually running them. The expected inventory change is one newly supported
Text factory (40 to 41, absent 10 to 9); runtime inventory remains authoritative.


## Font fixture correction

The first focused native run compiled successfully but reached an invalid
fixture assumption: the public GUI test initializer intentionally disables font
discovery, so its font container can contain zero system fonts. The binding
case now creates two test-owned native `GimpFont` objects using the factory's
normal public construction/lookup-name APIs and registers them temporarily in
the live font container. Their names are deterministic and unique; Sans/Serif
lookup descriptions are valid Pango descriptions, without assuming particular
installed font files. The test selects those exact objects, exercises reverse
model updates, restores the previous context font, removes both resources, and
checks finalization and the original container count. No fontconfig refresh,
filesystem resource or source-profile write occurs. This verifies native font
resource binding, not glyph/font rendering acceptance. The failed run remains
a failed run; all five cases require rerunning with this corrected fixture.

## Final native verification

The [receipt](../tests/text-toolbar.json) records exact tested-source and
executable hashes. Five focused cases pass in 4.68 seconds; the complete
58-case compact-options registration passes in 104.63 seconds; all five new
cases pass with GDK_SCALE=2 in 4.26 seconds. No skips or unexpected diagnostics
occurred; existing expected negative-case warnings remain visible. Runtime
inventory confirms 41 supported independent factories and 9 still absent.

The first focused run exposed the disabled-font-loading test setup; its failure
is retained. The corrected native font fixtures prove binding and cleanup
without a host-font inventory assumption. An installed actual-application
1080×1920 capture verifies the bottom Text bar and open Details popup, including
the retained second color control. Normal GUI quit returns zero. An earlier
startup-window screenshot was not accepted, and a title-selection timeout is
retained. These are normal Linux results, not sanitizer, font-rendering,
complete text-layer editing, or full-port acceptance.
