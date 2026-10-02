# Painter brush-setting generation

`brush-settings.json` is the canonical, reviewable source for the pinned painter
brush metadata. The missing historical `generate.py` was not recovered. This
replacement reconstructs its enum output and the surviving metadata without
changing brush evaluation or inventing missing metadata.

## Source and compatibility contract

Reference: seagetch/gimp-painter, commit
`afa43fae3e920210146abed514f136fd49f671b5`.

| Section | Count | Contract |
| --- | ---: | --- |
| Constants | 103 | Original macro names, expressions, numeric values and order |
| Inputs | 9 | Names, indices, five limits/normal values, labels and tooltips |
| Numeric settings | 45 | Names, indices 0–44, constant flag, range/default, labels/tooltips |
| Switches | 5 | Names, indices 45–49, boolean defaults and labels |
| Text settings | 2 | Names, indices 50–51, NULL defaults and labels |
| States | 30 | Names, stable replay indices and constructor zero initialization |

The manifest records gfloat storage separately from the legacy G_TYPE_DOUBLE
numeric property type. Numeric literals remain strings, preserving spelling,
precision and the input `None` sentinel. `None` means `FLT_MAX` in this source;
it does not mean zero or NULL. Input `normal` is an editor reference value, not a
runtime default. State minimum/maximum are explicitly null because no metadata
range is declared. Constructor zero initialization is not the same thing as the
engine's deferred reset or its subsequent runtime state bounds.

The `legacy_groups`, `legacy_migrations`, `legacy_transform_bodies` and
`compatibility` sections retain supplemental historical metadata. They are
review records, not generated runtime APIs. In particular:

- the tracking group lacks a terminating NULL in the old source
- the stroke group uses `BRUSH_SETTING_GROUP_TRACKING`
- the color group contains `colorize`, which has no active setting
- `motion_strength` is commented out and has no active input constant
- six old setting-name aliases and three transformation bodies remain recorded

These are not silently corrected, activated or discarded by this generator.
Repairing the unsafe old group iteration belongs to the reader/editor port with
separate compatibility evidence. The immutable reference files include the
complete original data and notices.

This catalog describes known metadata; it must not be used as a whitelist that
drops unknown settings, inputs, switches, texts or values when loading/saving a
brush. Reader/writer lossless handling is a separate, still-required task. The
generator never reads or rewrites brush assets and never clamps their values.

## Outputs and ownership

`generate.py` reads only the manifest using the Python standard library and emits:

- `mypaintbrush-enum-settings.h`: byte-identical to the pinned original, including
  expressions, whitespace, include guard and the original generator banner
- `mypaintbrush-settings-data.h`: read-only fixed-length C11/C++14 tables with
  explicit metadata types, original strings and the retained brushlib notice

The latter is metadata, not a public binary ABI or an engine replacement. It
uses `const char *`, `int` and `float` corresponding to the old metadata fields,
without GLib ownership or sentinel iteration. Future feature adapters should
consume it deliberately; existing standard GIMP MyPaint behavior is unchanged.

Do not check build outputs into the source tree. Edit the manifest, review the
pinned-source comparison failure if intentional, and regenerate in the build
directory. The historical reference fixtures are never generator inputs.

```sh
python3 -B app/paint/painter-brush-settings/generate.py --output-dir /tmp/brush-generated
python3 -B app/paint/painter-brush-settings/generate.py --output-dir /tmp/brush-generated --check
python3 -B app/paint/painter-brush-settings/tests/test-generate.py
python3 -B app/paint/painter-brush-settings/tests/test-meson.py
```

`--check` is read-only and fails for missing/stale outputs. Identical outputs are
not rewritten. The generator evaluates only integer constants, preceding symbol
references, addition and subtraction; it does not evaluate arbitrary code.

## Meson integration

`app/paint/meson.build` calls `subdir('painter-brush-settings')`. One custom target
owns both outputs. The exported `painter_brush_settings_dep` supplies generated
sources and the source/build include paths; future C or C++ consumers should add
that dependency. Both probe executables already do so. The manifest and generator
script are build dependencies; no external legacy checkout is needed.

Registered tests are `painter-brush-generator`, `painter-brush-metadata-c`, and
`painter-brush-metadata-cpp`. In a cross build, the Python suite skips its direct
host compiler execution and the native Meson executable tests use Meson's
configured cross runner. This is not Windows/macOS validation.

## Verification

`tests/legacy/` contains the four immutable source files with SHA-256 fingerprints
in the manifest. There was no `brushsettings.hpp` in the old tree; state order
comes from the disabled `states_list` in `mypaintbrush-brushsettings.c`, and state
storage/constructor initialization comes from `mypaintbrush-brush.hpp`.

The ten-test Python suite checks reference fingerprints, all 103 enum bytes,
every table field, indices and counts, state order/defaults, old groups/aliases,
licenses, sentinel handling, deterministic generation, unchanged-file mtimes,
read-only stale detection and malformed manifest rejection. It compiles actual
old C metadata declarations/initializers and the generated C/C++14 tables, then
compares serialized field bytes. This avoids struct-padding comparisons and
catches float rounding, labels, booleans, NULL strings and index changes. A
deliberate default-value mutation verifies that the comparison detects drift.
The old GLib scalar types in that isolated probe are represented by their C ABI
primitives; it does not claim to compile or run the old application.

`test-meson.py` copies this module into a disposable project and builds its
production Meson file. It checks a no-op build, changes an actual manifest value,
and confirms that the generated header and both C/C++ consumer objects rebuild.
It also verifies that the deliberate edit fails the pinned compatibility test.
It never mutates the working manifest or the shared GIMP build directory.

Reports live under `migration/tests/brush-setting-generator*`. Passing these tests
establishes generator/metadata equivalence only, not rendered brush equivalence,
asset round trips, runtime integration of feature adapters, or completion of WBS
prerequisites.
