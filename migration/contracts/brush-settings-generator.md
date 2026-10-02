# 04.013/brush-setting-generator evidence

Implementation: `app/paint/painter-brush-settings/`.
Integration: `subdir('painter-brush-settings')` in `app/paint/meson.build`.
Future metadata consumer dependency: `painter_brush_settings_dep`.

Canonical manifest reconstructed from the four exact files at pinned legacy
commit `afa43fae3e920210146abed514f136fd49f671b5`. The manifest contains 103 macros,
9 inputs, 45 numeric settings, 5 boolean settings, 2 text settings and 30 state
names/indices. Every available default and range is explicit, including unknown
state ranges, the FLT_MAX sentinel, gfloat vs G_TYPE_DOUBLE, and constructor-only
state zeroing. The old engine file is `mypaintbrush-brush.hpp`; a file named
`brushsettings.hpp` does not exist in that reference.

The generated enum header exactly matches the original bytes. Generated read-only
C/C++ metadata matches every active legacy table field, including exact compiled
float bytes, names, labels, tooltips and NULL defaults. Historical aliases,
transforms, hidden/disabled names and anomalous groups remain recorded. No brush
assets are rewritten, clamped or filtered; unknown-value preservation remains a
mandatory reader/writer requirement, not a claim made by this metadata task.

Validation commands:

```sh
python3 -B app/paint/painter-brush-settings/tests/test-generate.py \
  --report migration/tests/brush-setting-generator.json
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
python3 -B app/paint/painter-brush-settings/tests/test-meson.py \
  --report migration/tests/brush-setting-generator-meson.json
flock /tmp/gimp-painter-build.lock bash -c \
  'meson setup --reconfigure build-debian13 && \
   meson compile -C build-debian13 painter-brush-metadata-c painter-brush-metadata-cpp && \
   meson test -C build-debian13 painter-brush-generator painter-brush-metadata-c painter-brush-metadata-cpp --print-errorlogs'
```

The first suite has ten tests and strict C11/C++14 compile checks. The isolated
Meson suite checks the real production custom target, byte/semantic comparison,
no-op build, source value change and downstream object rebuild in both languages.
The final command checks registration/build/test in the GIMP parent build.
See the JSON reports and `brush-setting-generator-gimp-build.txt` for outcomes.

This evidence may be integrated taskwise after prerequisite checks by the parent.
It does not complete 04.013 globally (other enum generators), full-feature brush
integration, reader/writer compatibility, rendering, or platform release gates.
