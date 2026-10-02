# Non-C++ script and generator audit (01.015)

The all-path filename **and shebang** scan identifies **37 changed auxiliary
script/build-input paths**, represented by **83 hunk checks** in
`auxiliary-script-review.tsv`. Six source-generator relationships, including
one absent named generator, are separately recorded in
`auxiliary-generator-relations.tsv`. Every row has a concrete port/replacement
route and independent verification task.

## Source findings that affect migration

1. `data/mypaint-brushes/Makefile.am.skel` is Ruby code. It scans child
   directories for `.myb` and `_prev.png`, sorts basenames and writes their
   `Makefile.am` install lists. Preserve membership in Meson manifests under
   `30.017/brush-manifest-generator`. The root `FX_blender_prev.png` has no
   matching source `.myb`; keep it in the asset inventory rather than dropping
   it during a pair-only scan
2. `label-brush-mypaint.sh` uses **ImageMagick `convert`**, not GIMP batch or
   Python. It labels each PNG and overwrites that input after caption, border,
   resize, unsharp and flatten operations. The migration must preserve those
   operations and handle quoted filenames safely. No original assets were
   modified during this audit
3. The changed Perl `.pdb` source declares three MyPaint selection procedures:
   popup, close-popup and set-popup. Their argument/nullability and generated
   app/lib exports belong in the current `pdb/` generator pipeline, whose
   `pdb/meson.build` invokes enumgen, pdbgen and enumcode through Python wrappers
4. `tools/pdbgen/enums.pl` and `groups.pl` are generated Perl inputs, not
   independent definitions to hand-edit. Preserve the mode semantics and
   `mypaint_brush_select` group in their authoritative inputs
5. No `.scm` source file changed, but the C Script-Fu wrapper adds seven
   interpreter constants. `30.012/scheme-mode-constants` explicitly retains
   their names and maps their meaning rather than reusing conflicting old
   numeric values
6. No `.py` file changed, but the unchanged Python `make-installer.py` is a
   relevant generator dependency. Its `composite_modes` list lacks the four
   added SRC/DST IN/OUT modes that are present in the checked-in generated
   installer. Regeneration from that legacy script would lose the manual
   dispatch additions. The current GEGL dispatch/operation route must preserve
   the checked-in behavior under `13.001/gegl-operation-route`
7. `mypaintbrush-enum-settings.h` names `generate.py`, which is absent anywhere
   in the pinned tree. This remains explicit recovery work under
   `04.013/brush-setting-generator`; generated settings are still functional
8. All changed Automake, Autoconf and Flatpak recipe/patch hunks retain source,
   dependency and resource-install responsibilities in their Meson/package
   destinations. Optional HTTP build inputs are separated from mandatory JSON
   persistence functionality

## Verification and limits

`python3 tools/audit_auxiliary_scripts.py --check --syntax` reproduces the
full per-hunk set, checks generator presence/absence and the four-mode
source/output mismatch, and validates Bash plus all three changed Perl inputs
with `bash -n` / `perl -c`. These checks pass without executing asset-mutating
commands. The Ruby interpreter is unavailable here; Ruby code was inspected
and its input/output contract recorded, not executed. No GIMP 3 generator or
rendering compatibility pass is claimed. All port/runtime rows remain
`NOT_PORTED` and their implementation WBS actions remain open.
