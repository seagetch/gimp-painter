# Direct JSON-GLib dependency verification

This is focused evidence for existing `04.009/json-dependency` and the two
original `04.009` configure hunks, `01.002/001891` (required JSON version) and
`01.002/001894` (required pkg-config probe). The port declares the mandatory
`json-glib-1.0 >= 1.2.6` dependency and lists it directly on `appcore`,
`painter-mypaint`, and the latter's exported internal dependency.

Run against already built native GUI/console targets, with that build's
dependency environment loaded:

```sh
source /workspace/shared/gimp-cpp-boundary-04005/env-main.sh
python3 -B tools/check_painter_json_link.py \
  --build /workspace/shared/gimp-cpp-boundary-04005/build-default \
  --configuration default \
  --output-dir /workspace/shared/gimp-json-link-04009/default \
  --report migration/tests/json-link/default.json

source /workspace/shared/gimp-cpp-boundary-04005/env-http.sh
python3 -B tools/check_painter_json_link.py \
  --build /workspace/shared/gimp-cpp-boundary-04005/build-http \
  --configuration http \
  --output-dir /workspace/shared/gimp-json-link-04009/http \
  --report migration/tests/json-link/http.json
```

The checker verifies all of the following:

- Explicit required JSON declaration and direct target dependency registration;
  exact original configure-hunk ownership and payload hashes
- JSON include flags and undefined JSON API references in the real production
  `gimplayerpreset.c`, `gimplayerpreset-apply.cpp`, and `resource.cpp` objects
- Fresh C++14 compilation of unchanged production `resource.cpp`, the real
  painter error implementation, and the focused smoke driver, with only the
  generated brush metadata, application headers, and JSON-GLib/GIO include flags
- Failure when the JSON include path is removed
- Strict isolated links using both the fresh production objects and the
  configured production objects; successful real JSON reading/writing through
  GLib memory streams and one legacy-v2 decode/re-encode smoke
- JSON-GLib present in ELF `DT_NEEDED`, no Soup/GEGL/GTK in direct or resolved
  dynamic-library dependencies, and failure when direct JSON linkage is removed
- Fresh GUI/console production links in both default and HTTP configurations,
  with `--no-undefined` and `--no-copy-dt-needed-entries`; removing only the direct
  JSON library fails with JSON references from the native layer-preset model,
  even though application GEGL and, for HTTP, Soup remain available

No original build products are modified. Reports contain allowlisted facts,
symbol names and content hashes, without inherited environment values or raw
compiler/Meson logs. Binaries and intermediate objects stay outside the checkout.

These checks prove direct dependency registration and real native linkage.
The full application needs GEGL for its semantic APIs, so only the independently
compilable production Resource check removes GEGL entirely. Existing layer-preset
and MyPaint fixture acceptance own complete reader/writer/old-file semantics;
this link check does not replace them or claim their parity, sanitizer coverage,
or cross-platform validation.
