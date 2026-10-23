# 07.008/editor-page-memory

This package verifies owner-local notebook page memory, not full editor or
legacy UI equivalence. One invoking editor and its transient popup editors
share a page index. Independent editors do not share it. Context replacement
releases the retired notebook's owned `notify::page` connection.

## Accepted normal run

`page-memory-final.json` retains stdout, stderr, commands, return codes and
wall times from the final native run. **All Meson environment maps were
removed**, not merely redacted selectively.

- Existing `painter-mypaint-editor`: **25/25**, 32.070 seconds.
- New `painter-editor-page-memory`: **3/3**, 5.246 seconds.
- Both registered targets retain their 120-second limits. No skipped cases.
- Tests cover context rebuild, detached stale notebook notifications,
  independent editor isolation, popup close/reopen, native show/page
  callback context detachment, and subsequent recovery.
- `report.json` pins the three relevant source files by SHA-256. The candidate
  was based on `4a83db492ffe90db2372866675d56a45b7355f3f`; it was not a clean
  exact-commit whole-project aggregate run.

Normal reproduction, using the repository's documented dependency environment:

```
python3 tools/run-painter-xvfb.py meson test -C BUILD --no-rebuild \
  --num-processes 1 --print-errorlogs painter-mypaint-editor painter-editor-page-memory
```

## Expected failures and excluded fixture attempts

`page-memory-expected-red.json` records page 0 instead of 2 after context
replacement. `page-popup-expected-red.json` records page 0 instead of 1 on
popup reopen. These are the behavior regressions addressed by this change.

`excluded-fixture-attempts/` preserves unsuccessful development fixtures.
The early destructive GTK emission hook ran before the native default
handler, producing an invalid-widget diagnostic and SIGSEGV in alternative
implementations. A later discovery observer did not fire. These runs are
**not** accepted tests or proof that a production crash was repaired.
The final third test discovers the notebook from its parent association and
uses ordinary post-default show/page callbacks to detach the context.

## Actual GUI evidence

The screenshots are unmodified captures from an authenticated loopback Xvfb
screen of **1080 × 1920 × 24**, with `(painter-canvas-ui yes)`:

1. `04-input-curves-selected.png`: Input Curves selected in the popup editor.
2. `05-popup-closed.png`: popup closed, main application remains visible.
3. `06-reopened-input-curves.png`: popup reopened with Input Curves retained.

`capture-editor-memory.py`, `commands/` and `result.json` preserve the capture
steps and successful application exit. The capture script contains historical
absolute build/output paths; adapt its `base` path when reproducing elsewhere.
The left canvas tool strip, right controls and bottom active-tool controls
remain visible. The images establish the demonstrated interaction only,
not the completeness or fidelity of every legacy UI component.

## Limits and diagnostics

This package makes **no sanitizer or leak-check claim**. Separate sanitizer
evidence must be assessed independently. Native stderr retains missing source
application icons, ignored `.dummy` plug-in files, unavailable D-Bus and
virtual input-device disable messages. The GUI log additionally reports
unsupported fonts. These diagnostics are not silently discarded and this
focused run is not a strict warning-free release gate.

Validate the stored logs, screenshot geometry, environment removal, hashes
and current relevant source with:

```
python3 migration/tests/editor-page-memory/validate.py --check-source
```

Omit `--check-source` when checking archived evidence after later source changes.
That mode validates the stored evidence without claiming a new source run.
