# Legacy RGB cross-image CloneLayer observations

Captured from the real rebuilt GIMP-Painter 2.8.23 application core at
`afa43fae3e920210146abed514f136fd49f671b5` on 2026-10-02. The process completed
with exit status 0. This separate package does not alter or replace the sealed
`legacy-runtime` or `legacy-clone-undo` fixtures.

## Ordinary scene and measured result

Both images are 64×64 RGB. All sources/clones are 16×16 RGBA in Normal mode.
The original source is filled with (204,51,102,128); its direct clone is placed
at (32,30). Copy uses `gimp_item_convert(clone, destination, CloneLayerType)`
and ordinary image insertion. The copied clone lives in the destination but
keeps the original source identity and the same pixel/offset. Changing that
source to opaque red updates both original and copied clones.

A group contains a blue source named `inside`, an `internal` clone referring
to it, and an `external` clone referring to the original red source. The same
ordinary convert/copy API copies the group into the destination. The internal
reference points to the copied `inside`; the external reference still points
to the original source. The original group's internal reference is unchanged.
Changing original `inside` to yellow leaves copied internal clone blue; changing
copied `inside` to cyan leaves original internal clone yellow.

Source relocation keeps the original source object alive across ordinary
remove/unset-removed/convert-vfunc/add operations. The old vfunc takes item and
destination only. Its image changes to the destination; original direct clone,
destination direct clone, and destination external group clone retain exactly
the same source identity. Changing it to green updates all three.

After closing the original image, all destination references still match their
correct live sources. Changing the relocated source to magenta updates the
direct/external copies; the copied internal clone remains cyan. All measured
sizes/offsets remain 16×16, with direct copies at (32,30) and group children at
(0,0). `observations.json` contains the 4 reference and 15 pixel/geometry records
transcribed mechanically from `capture.log`.

## Warnings and limits

The raw log is preserved without normalizing addresses, process IDs or clocks.
It includes two known startup `G_VALUE_HOLDS_OBJECT` guards, one internal
unattached-drawable projection guard while group duplication remaps references
before final attachment, and four pairs of invalid-instance/signal-handler
criticals during destination teardown. The latter reflect the old borrowed
source/connection lifetime hazard and are not hidden or declared successful
sanitizer checks. A Script-Fu pipe warning appears during process exit. All
observations completed and the executable returned 0.

The capture uses valid ordinary RGB inputs only. It is not GUI evidence, an
indexed/Gray test, a malformed-input probe, a format round trip, a precision
conversion test, a whole-image compositor comparison, or a historical
binary-identical environment. First-pixel values validate the specified solid
fills, not arbitrary artwork. The port intentionally repairs dead reference
lifetimes rather than reproducing those teardown warnings.

## Provenance and reproduction

- `capture-clone-cross-image.inc` is the new instrumentation, also embedded
  directly in test-xcf by the complete `source-overlay.patch`
- The overlay includes inherited build compatibility fixes, prior resize-Undo
  instrumentation, and unrelated Filter logging which this capture does not use
- Original CloneLayer, group duplicate, item/layer conversion, Undo, mask and
  compositor implementations are byte-identical to the pinned source commit;
  `capture-report.json` records their verified hashes
- `build-env.sh`, `gimp-cflags.txt`, and the build command/report hash record the
  existing reference toolchain. `build.log` preserves successful relink warnings
- `build-initial-signature-error.log` preserves an initial test-harness compile
  error from using the newer three-argument convert signature; no executable
  ran from that attempt. The corrected harness uses the declared old signature
- `run-capture.sh` records the exact headless invocation. The report records the
  resulting executable size/hash, build/capture exits and source overlay hashes
- `manifest.json` seals every package file except itself. Run
  `python3 -m unittest migration.tests.test_legacy_clone_cross_image_fixtures`
  to verify integrity, provenance, transcript fidelity and complete observations
