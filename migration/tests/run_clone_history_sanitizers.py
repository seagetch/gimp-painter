#!/usr/bin/env python3
"""Clone model sanitizer with the actual derived-cache Undo policy instrumented.

Retains the ordinary Clone runner's archive/RTTI/DSO verification; adds only
native publication and pixel-undo units. LSan remains disabled. Use --test-path
for the three original14.006/editability-contract regressions.
"""
import run_clone_layer_sanitizers as runner

runner.SOURCES = runner.SOURCES | {
    "app/core/gimpimage-undo.c",
    "app/core/gimpdrawableundo.c",
}
if __name__ == "__main__":
    raise SystemExit(runner.main())
