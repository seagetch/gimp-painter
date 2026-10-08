# Enum generation evidence

Scope and remaining gates: `migration/contracts/enum-generation.md`.

`evidence.tar.gz` contains 183 UTF-8 files: exact isolated mutation outputs and
logs, before/after selected native dependency edges, graph comparisons, native
C/C++ rebuilds and the brush/public-mode checks. `evidence-manifest.json` verifies
every member. There are no executable binaries or environment dumps.

Reproduce the isolated eight-case baseline/fixed comparison with a native Meson,
Ninja, Perl, GLib and C/C++ toolchain:

```sh
python3 -B migration/tests/enum-generation/probe.py \
  --source "$PWD" --output /tmp/painter-enum-new-run
python3 -B app/paint/painter-brush-settings/tests/test-meson.py \
  --report /tmp/painter-brush-generation.json
python3 -B tools/check_painter_enum_generation.py
python3 -B tools/check_painter_enum_routing.py
python3 -B tools/check_tasks.py
```

The probe requires the recorded public baseline commit 818ed559 and a fresh
output directory outside the source checkout. Native-header-rebuild.py records
the separate executed full-tree test with exact environment/build paths; adapt
those paths and preserve its source-restoration guard for another environment.
Its input mutation is a comment, whereas the isolated probe additionally changes
and executes a real numeric enum value. Cross-platform or full application
behavior is not inferred from these tests.
