#!/usr/bin/env python3
"""Save Meson clone evidence without unrelated inherited runtime environment."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--logbase", default="clone-layer")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
allowed = {"PATH", "LD_LIBRARY_PATH", "PKG_CONFIG_PATH", "BABL_PATH", "GEGL_PATH",
           "GIMP_DEPS_DIRECTORY", "GIMP_DEPS_ROOT", "GIMP_TESTING_ABS_TOP_SRCDIR",
           "GIMP_TESTING_ABS_TOP_BUILDDIR", "GIMP_TESTING_PLUGINDIRS",
           "ASAN_OPTIONS", "UBSAN_OPTIONS", "LANG", "LANGUAGE", "LC_ALL", "LC_CTYPE"}
rows = [json.loads(line) for line in (args.build / ("meson-logs/" + args.logbase + ".json")).read_text().splitlines() if line]
assert rows and all("gimp-clone-layer" in row["name"] for row in rows)
for row in rows:
    row["env"] = {key: value for key, value in row.get("env", {}).items() if key in allowed}
(root / "migration/tests/clone-layer-testlog.json").write_text("".join(json.dumps(row) + "\n" for row in rows))
text = (args.build / ("meson-logs/" + args.logbase + ".txt")).read_text()
start = text.index("Inherited environment:")
end = text.index("====================================", start)
text = text[:start] + "Reproducibility environment (explicit allowlist only):\n" + json.dumps(rows[0]["env"], indent=2) + "\n\n" + text[end:]
(root / "migration/tests/clone-layer-testlog.txt").write_text("\n".join(line.rstrip() for line in text.splitlines()) + "\n")
