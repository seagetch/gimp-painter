#!/usr/bin/env python3
"""Keep test results and only explicitly approved reproducibility variables.

Supports Meson JSONL, full/nested JSON reports and human-readable test logs.
This is evidence preparation, never a filter on test assertions or diagnostics.
"""
import json
from pathlib import Path
import re
import sys

ALLOWED = {
    "GIMP_TESTING_ABS_TOP_SRCDIR", "GIMP_TESTING_ABS_TOP_BUILDDIR",
    "GIMP_TESTING_PLUGINDIRS", "GIMP3_DIRECTORY", "UI_TEST", "GEGL_PATH", "BABL_PATH",
    "LD_LIBRARY_PATH", "PKG_CONFIG_PATH", "GI_TYPELIB_PATH", "XDG_DATA_DIRS",
    "GIMP_DEPS_DIRECTORY", "GIMP_DEPS_ROOT", "ASAN_OPTIONS", "UBSAN_OPTIONS",
    "MSAN_OPTIONS", "MESON_TEST_ITERATION", "MALLOC_PERTURB_", "G_DEBUG",
    "LANG", "LC_ALL", "LC_CTYPE", "GSETTINGS_BACKEND", "GDK_SCALE", "GDK_DPI_SCALE",
}

def sanitize_value(value):
    if isinstance(value, list):
        return [sanitize_value(item) for item in value]
    if isinstance(value, dict):
        return {key: ({k: v for k, v in item.items() if k in ALLOWED}
                      if key in {"env", "environment", "inherited_environment"} and isinstance(item, dict)
                      else sanitize_value(item)) for key, item in value.items()}
    return value

def sanitize_json_text(text, jsonl=False):
    if jsonl:
        rows = [json.loads(line) for line in text.splitlines() if line.strip()]
        return "\n".join(json.dumps(sanitize_value(row), ensure_ascii=False) for row in rows) + "\n"
    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        rows = [json.loads(line) for line in text.splitlines() if line.strip()]
        return "\n".join(json.dumps(sanitize_value(row), ensure_ascii=False) for row in rows) + "\n"
    return json.dumps(sanitize_value(value), ensure_ascii=False, indent=2) + "\n"

def sanitize_text(text):
    text = re.sub(r"Inherited environment:.*?(?=\n=+ 1/)",
                  "Inherited environment omitted; see whitelisted JSON reproducibility variables.\n",
                  text, flags=re.DOTALL)
    # Refuse unknown layouts rather than silently preserving a whole environment.
    if "Inherited environment:" in text:
        raise ValueError("Unrecognized inherited-environment log framing")
    return "\n".join(line.rstrip() for line in text.splitlines()) + "\n"

def sanitize_file(path):
    path = Path(path)
    original = path.read_text()
    text = sanitize_json_text(original, path.suffix == ".jsonl") if path.suffix in {".json", ".jsonl"} else sanitize_text(original)
    path.write_text(text)

if __name__ == "__main__":
    for filename in sys.argv[1:]:
        sanitize_file(filename)
