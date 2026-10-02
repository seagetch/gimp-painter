#!/usr/bin/env python3
"""Keep test output/results and only an explicit reproducibility environment."""
import json
from pathlib import Path
import re
import sys

allowed = {
    "GIMP_TESTING_ABS_TOP_SRCDIR", "GIMP_TESTING_ABS_TOP_BUILDDIR",
    "GIMP_TESTING_PLUGINDIRS", "UI_TEST", "GEGL_PATH", "BABL_PATH",
    "LD_LIBRARY_PATH", "PKG_CONFIG_PATH", "GI_TYPELIB_PATH", "XDG_DATA_DIRS",
    "GIMP_DEPS_DIRECTORY", "GIMP_DEPS_ROOT", "ASAN_OPTIONS", "UBSAN_OPTIONS",
    "MSAN_OPTIONS", "MESON_TEST_ITERATION", "MALLOC_PERTURB_", "G_DEBUG",
    "LANG", "LC_ALL", "LC_CTYPE",
}
for filename in sys.argv[1:]:
    path = Path(filename)
    if path.suffix == ".json":
        rows = []
        for line in path.read_text().splitlines():
            row = json.loads(line)
            if "env" in row:
                row["env"] = {k: v for k, v in row["env"].items() if k in allowed}
            rows.append(json.dumps(row, ensure_ascii=False))
        path.write_text("\n".join(rows) + "\n")
    else:
        text = path.read_text()
        text = re.sub(r"Inherited environment:.*?(?=\n=+ 1/)",
                      "Inherited environment omitted; see whitelisted JSON reproducibility variables.\n",
                      text, flags=re.DOTALL)
        path.write_text("\n".join(line.rstrip() for line in text.splitlines()) + "\n")
