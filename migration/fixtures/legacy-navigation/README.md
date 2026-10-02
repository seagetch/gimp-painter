# Legacy rotation arithmetic oracle

`capture.c` contains the unchanged start/update function bodies extracted from
`app/display/gimpdisplayshell-tool-events.c` at the pinned old commit. GTK pointer
grab, cursor and expose calls are no-op stubs. This is executable old-source
arithmetic evidence, **not** a captured GUI, mouse or tablet session.

`rotation.tsv` records 5,184 start-relative updates. Columns: viewport width,
height, horizontal mirror, initial legacy angle, start x/y, current x/y, modifier
bit mask, resulting legacy angle. It covers even/odd viewport centers, center and
axis start points, all quadrants, 0/360 and exact half-snap angles, modifier
addition/removal and unchanged coordinates. Updates share a fixed start anchor.
The modern test also compares equivalent Cairo transforms, since the old display
used `Flip * Rotate` while GIMP 3 uses `Rotate * Flip`.

Reproduce with:

```
python migration/tests/capture_navigation_arithmetic.py --legacy ../gimp-painter-legacy
```

The script verifies the original full file against the pinned Git blob before
extracting it. `manifest.json` records hashes, compile command and exact scope.
