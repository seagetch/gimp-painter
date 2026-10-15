# Native drawable Y invalidation correction

`gimp_drawable_set_buffer_full()` translated the old buffer's y extent with
`gimp_item_get_offset_x()`. The new extent correctly uses `bounds->y`. The fix
uses the corresponding old item y offset; no region size, rounding or other
coordinate policy changes.

Two real native regressions failed before the fix:

- Extent `(0,0,4,3)`, item offset `(7,-3)`, unchanged replacement bounds:
  two updates were emitted instead of only the post-replacement update.
- Moving only y from -3 to 7 made the erroneous old extent equal the new one:
  only one update was emitted, dropping the required old-region invalidation.

Both failures are preserved as failed tests (SIGABRT, Meson exit 1). After the
one-line correction, the full core-metadata target passes 10/10 cases, including
six native drawable cases. These cover explicit/default unchanged bounds,
y-only movement, and nonzero GEGL buffer origins. Signal observations verify
exact local and image rectangles and old/new buffer identity during the actual
native setter. No synthetic implementation, wrapper or mock replaces it.

Reproduction in the isolated Debian/TCP environment:

    python3 tools/run-painter-xvfb.py meson test -C BUILD painter-core-metadata --print-errorlogs

For the two original failures, run the unchanged new test file against the
unfixed production source with `--test-args='-p /painter/core-metadata/drawable-buffer/same-size-explicit-bounds'`
and `--test-args='-p /painter/core-metadata/drawable-buffer/y-only-move'`.

Selected summaries retain test status, TAP assertions and original log hashes
without publishing inherited environment variables. This narrow correction does
not establish general coordinate, rendering, UI, sanitizer or platform parity.

Neighboring native registrations also pass: clone-layer, XCF open, XCF
round-trip, layer modes, and the complete core-metadata target (5/5 targets).
