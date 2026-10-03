#!/usr/bin/env python3
"""Create a test-only observation copy; production worker control is unchanged."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()
anchors = {
    "namespace GimpPainter {":
        'extern "C" void filter_spool_observation (int stage);\nnamespace GimpPainter {',
    "    phase.store (Phase::exporting, std::memory_order_release);":
        "    phase.store (Phase::exporting, std::memory_order_release);\n    filter_spool_observation (1);",
    "    state->phase.store (Phase::complete, std::memory_order_release);":
        "    state->phase.store (Phase::complete, std::memory_order_release);\n    filter_spool_observation (2);",
}
for old, new in anchors.items():
    if text.count(old) != 1:
        raise SystemExit("Spool observation anchor changed")
    text = text.replace(old, new)
output.write_text(text)
