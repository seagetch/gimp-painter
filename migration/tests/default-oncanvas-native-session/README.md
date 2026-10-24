# Default Painter canvas panels

## Requested behavior

Painter canvas controls now default to **enabled**. A clean profile uses the
shipped single-window session, whose native right-side docks appear as canvas
panels. No `painter-canvas-ui` enabling setting, migration or UI action is needed.
An image must be open to inspect the canvas controls.

The sole production change is the boolean default in
`app/config/gimpcoreconfig.c`. Existing explicit `(painter-canvas-ui no)` remains
an opt-out. Older profiles **omitting** the property inherit the new TRUE default;
they are not treated as explicit opt-outs. Existing custom saved dock ownership
is preserved, not automatically merged or overwritten.

This is separate from the original empty-panel diagnosis: the generic GUI test
session explicitly restored four floating dock windows. The normal shipped
session already restores populated native right docks. No production restoration
or dock-transfer defect was reproduced in that investigation, and the proposed
empty-host hiding patch was withdrawn. This change does not include that patch.

## Code and test scope

- Change the production property default from FALSE to TRUE.
- Add a dedicated native-session fixture, byte-identical to `etc/sessionrc`, and
  select it only for the standalone native UI acceptance test.
- Native acceptance never sets the Painter property. It asserts the inherited
  TRUE default, populated canonical right docks, zero floating windows, native
  layer rows and actual file-backed shipped brushes in the mapped brush grid.
- Update the compact-options preference test's initial default oracle to TRUE;
  preserve its existing explicit-FALSE serialization/deserialization roundtrip.
- Update current default documentation without relabeling historical results.

The generic legacy session fixture and production docking/layout implementation
remain unchanged. No synthetic dock population, generated brush substitute,
recursive show-all, forced merge or extra preference is introduced.

## Runtime evidence

See `results.json` and `source-seal.json` for exact statuses and hashes. Baseline
FALSE builds fail both new default assertions (expected RED); the identical test
sources with the TRUE production default pass (GREEN). Generated `--dump-gimprc`
changes from `(painter-canvas-ui no)` to `(painter-canvas-ui yes)` automatically.

The normal-app controls run the **uninstrumented** application with the real
`app/tests/files/gimp-2-6-file.xcf` argument and actual distributed resources:

1. Completely pristine profile, with no initial files.
2. Existing official native session, with no Painter property or gimprc.
3. Explicit `painter-canvas-ui no`, with no saved session.

All initial input files and absence of the Painter enabling setting are recorded
before launch. GIMP_TESTING variables are unset; no command or environment forces
Painter on. `capture-x11.c` is an external observer: it closes the welcome dialog
via ordinary WM_DELETE_WINDOW, resizes the main window to 1040×1800, and captures
the genuine 1080×1920 Xvfb framebuffer. It does not edit GIMP configuration or
create/reparent widgets. Closing Welcome may naturally update its own state.
Normal-app screenshot processes are deliberately SIGTERM-terminated afterward;
these controls establish startup/display behavior, not graceful-shutdown safety.

## Scope limits

Native runtime only; no new ASan qualification. This bounded default change is
not whole-port UI parity or full-repository acceptance. The prior fixture-only
phase retained an existing Perspective damage-1 failure; its separate diagnosis
and correction are not silently absorbed into these results. Historical test
logs and the earlier fixture-only bundle remain immutable.

### Broader failures retained

The full compact-options group fails `rectangle-units-real-popup-events` at
`test-painter-rectangle-unit-popup.inc:161` (`grab == popover`). The original
FALSE-default binary also fails that assertion, at a different tool/row. The
focused `canvas-dock-tool-transfer-preference` case fails its missing MyPaint
menu-button assertion at line394 with both TRUE and original FALSE binaries.
These are not passing regression gates; baseline reproductions show neither
failure is exclusive to the new default, not that all behavior is equivalent.
The dedicated default/explicit-FALSE persistence and populated native acceptance
are PASS. No failed assertion was weakened or removed.

## Reproduction

Prepare the repository's pinned Debian dependency environment and native build;
materialize `gimp-data` at `a31e5dea4f572fb5f33b06f5c4b86db5f30643dc` and stage
normal runtime assets. Use disposable HOME/XDG directories. With SOURCE, BUILD,
EVIDENCE and a fresh OUT set:

```sh
ninja -C "$BUILD" app/gimp-3.0 app/tests/painter-native-session-ui \
  app/tests/painter-compact-options menus/image-menu.ui
python3 "$SOURCE/tools/run-painter-xvfb.py" meson test -C "$BUILD" \
  --no-rebuild --num-processes 1 painter-native-session-ui
# Under the normal GUI-test source/build/plugin environment:
python3 "$SOURCE/tools/run-painter-xvfb.py" \
  "$BUILD/app/tests/painter-compact-options" \
  -p /painter-compact-options/native-visibility-preference
cc "$EVIDENCE/capture-x11.c" -o "$OUT-capture-x11" \
  $(pkg-config --cflags --libs gtk+-3.0 x11)
export CAPTURE_HELPER="$OUT-capture-x11" SOURCE BUILD OUT
for case_name in pristine-absent native-session-absent explicit-false; do
  python3 "$SOURCE/tools/run-painter-xvfb.py" \
    bash "$EVIDENCE/run-normal-control.sh" "$case_name"
done
```

The wrapper refuses to overwrite an existing case directory. Original RED
controls used the same test sources with only the production default left FALSE;
the original baseline executable hashes are preserved in the seal. The earlier
fixture causal matrix and generic RED/proper-profile GREEN records are retained
under `prior-fixture-investigation/` as explicitly historical evidence.
