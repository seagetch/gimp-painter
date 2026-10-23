# Tool-group retirement with oncanvas UI enabled

The native expected-red retains old tool groups through actual deserialization:
root-container clear alone leaves live tool-info parent links in externally held
groups, aborting in gimp_tool_group_child_add. This reproduces the pre-existing
old-toolrc integration failure exposed by enabling oncanvas UI; neither the UI
nor that original test was disabled.

Reset, deserialization and reset fallback now explicitly retire shared tool-info
membership through their parent child-containers before clearing the root. This
preserves remove notifications, parent links and active selection invariants even
when old toolbar/group references outlive root removal. The retiring parent is
leased across synchronous notifications.

All original 22 perspective UI cases plus 2 new native regressions pass normally
and with bounded ASan/UBSan/float-cast-overflow instrumentation under authenticated
1080×1920×24 Xvfb. The unchanged registered 60-second budget is used. New cases
repeat deserialization and reset three times, retain old groups, cover caller
pre-clear/orphan groups, verify old membership/active selection is retired and
new parent membership and original first-group selection remain consistent.

The screenshot is the actual integrated UI with oncanvas visible from the
add-move-remove-undo capture hook, using the reset-fix build. The two new cases
ran earlier in this process, but this is a fresh image in the add/move scene,
not a literal frame of a reset operation. The sanitizer scope is 9 C units (listed in the build
report); C++ UI callbacks, other app units and dependencies are uninstrumented.
LSan is disabled. No whole-app sanitizer or complete historical UI-parity claim.
Runtime diagnostics remain; inherited environment maps are excluded. Exact tested
source snapshots and hashes are included, and the old production source is retained
with the expected-red receipt.

This integrated execution also used the separate canvas-factory checkpoint saved
under tested-factory-checkpoint/ and factory-checkpoint-sha256.json. Those files
were uncommitted at execution and are not part of the tool-group production fix.
This receipt does not claim an isolated HEAD-only UI execution. The actual window
capture is 1040×1800 within the authenticated 1080×1920 screen.
