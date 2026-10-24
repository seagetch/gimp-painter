# 24.009/duplicate-saved-resource — fresh migration leaf

This is new work against `b35ed653e2f84f8912a5323c0745b7279d02ca13`, not reconstruction of a lost commit. Parent 24.009 and the surrounding resource/history migration remain open.

## Contract and cause

Legacy `afa43fae3e920210146abed514f136fd49f671b5:app/actions/data-commands.c`, `data_duplicate_cmd_callback`, duplicates the selected factory resource, not editor state. The callback (trailing whitespace stripped) is retained in `legacy-duplicate.c`. Modern Duplicate incorrectly dispatched the Save As prompt and copied the unsaved options draft.

Duplicate now has a distinct prompt and clones the selected factory-owned saved resource via the existing typed duplicate handle. Save As still copies the draft. Both persist before publishing the copy. Selection records an unchanged dirty draft in application history; a synchronous callback's newer draft/name is never overwritten. Deleted/unlisted source resources are rejected. The internal standard fallback is not a factory-owned saved resource.

## Native acceptance

`app/tests/test-painter-editor-duplicate.inc` drives actual GTK buttons and dialog responses, not a parallel test implementation. Its four cases cover:

- Saved A versus dirty B, distinct unknown JSON, curves, texture references, icon PNG and name; tags; Unicode destination; persisted resource reload; unchanged original file bytes; dirty history restoration; separate Save As regression.
- Empty name, collision, Cancel, unwritable destination, stale selection.
- Recursive confirmation and newer draft/name from synchronous factory-add callback.
- Source deletion while the prompt is pending and restoration of the dirty draft from history.

The new Meson suite is `painter-editor-duplicate-saved-resource`, with the existing 120-second budget. Existing editor suites/assertions were not weakened. The optional `GIMP_TEST_EDITOR_DUPLICATE_SCREENSHOT` prefix records two real root-window screenshots; the fixture explicitly enables oncanvas because this baseline predates the separately reviewed default-oncanvas fix.

## Reproduction

Configure/build using the project's supported dependency environment, then run on 1080×1920 Xvfb:

```
meson compile -C BUILD painter-mypaint-editor
python3 tools/run-painter-xvfb.py meson test -C BUILD --no-rebuild --num-processes 1 painter-editor-duplicate-saved-resource
GIMP_TEST_EDITOR_DUPLICATE=1 GIMP_TEST_EDITOR_DUPLICATE_SCREENSHOT=/absolute/output/duplicate \
  python3 tools/run-painter-xvfb.py BUILD/app/tests/painter-mypaint-editor
```

Build the generated menus/icon fixture as required by native GUI test setup. `results.json` records actual source/binary/artifact hashes and outcomes. Focused sanitizer coverage does not imply a fully instrumented application or dependencies; LSan is disabled and vptr instrumentation is excluded. This project mixes RTTI and `-fno-rtti` translation units, so an instrumentation incompatibility is suspected but not proven. The first full-vptr attempt stopped in the existing BindingStore constructor before any test; that diagnostic remains an unresolved separate coverage gate, retained rather than counted as a pass. Portrait screenshots are direct captures, not mockups. These gates do not establish whole-port completion or recover historical test counts.

## Verified outcome

Final original-editor red control fails the saved-A copy assertion. Final native registered run: **5 suites, 38 cases pass**, zero skip/timeout; includes the original 25-case editor suite and named-entry/page-memory/control-lifetime suites. Scoped six-TU ASan + UBSan **excluding vptr**, with LSan off: **4/4 pass**. Full-vptr instrumentation fails before tests with both original and fixed editors at the same existing BindingStore constructor; this differentiates the Duplicate change from the baseline but does **not** resolve the diagnostic or prove it benign. Exact sanitized compile/link argument arrays are retained in the sanitizer build records. Two final 1080×1920 captures show saved A (0.21) and restored dirty B (0.83), with actual visible canvas/oncanvas controls.
