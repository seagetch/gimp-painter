# Original 04.013: Meson generation dependencies

The original criterion is that changing a generator input rebuilds its generated
output and affected C/C++ consumers. The common build graph now explicitly tracks
two previously omitted inputs:

- All 14 `meson-mkenums.py` stamp targets depend on the internally executed
  `tools/gimp-mkenums` parser, through `gimp_mkenums_source`
- `pdb/stamp-enumgen.h` depends on `pdb/util.pl`, which `enumgen.pl` loads

The direct libgimp/libgimpthumb parser commands already track their executable.
Existing enum header, Python wrapper, enumgen/enumcode/PDB module and PDB group
dependencies remain in place. The existing upstream source-writing generation
scheme is retained; this task changes dependency declarations only. It does not
modify any enum value, generated C/header contents, exported API or compiler flag.

## Verification

`migration/tests/enum-generation/` contains the executed probe, sealed evidence
and the report. In two isolated projects, the exact baseline and final production
Meson fragments drive the real enum/PDB generators. Eight mutations per project
cover a source enum value/description, the Perl parser, Python wrapper, loaded
Perl helper, enumgen, enumcode, PDB module and a real PDB group definition.

Both missing dependencies reproduce against the baseline. The fixed graph
regenerates the affected files and recompiles their consumers. The four C/C++
source/generated-header consumers return 0 before the enum mutation and 70 after
it. Real generated core-enums.c and internal-procs.c are compiled with small API
declaration stubs; this isolated fixture proves dependency behavior, not the full
application's runtime registration. Each of the 16 mutations settles to a no-op
build immediately afterward.

Separately, the real default and HTTP GIMP builds were reconfigured. Both contain
all 14 parser dependencies and the enumgen helper dependency. All 4,041 recorded
compiler command vectors remain unchanged. A harmless comment mutation of the
real core enum header rebuilds gimpimage.c, gimpclonelayer.cpp and the generation
stamp in each configuration; the next build is a no-op. Protected generator
recipes use byte-identical copied inputs and verify generated outputs against
the original tracked bytes. The source header is restored after each trial.

The existing brush-setting generation test was rerun: canonical manifest changes
rebuild its C and C++ consumers, followed by the expected compatibility-negative
check. The three registered parent Meson brush tests also pass. Their first
no-rebuild invocation found the two test executables unbuilt; that precondition
failure is retained, followed by their successful build and execution. The public
mode identity check still reports 72 public/generated modes and unchanged 65
upstream values; it does not establish PDB transport or pixel compatibility.

## Remaining feature registrations

Four legacy Makefile hunks add only MyPaint-selection PDB input/C/header entries.
They do not change the common generation rule. That feature's current API and
generated outputs remain missing. The exact payloads and previous duty identities
are preserved in `migration/inventory/enum-generation-routing-review.json`.
Their existing 30.012, 30.013 and 34.013 duties remain TODO, including registration,
generated-source synchronization and generated-output verification. Only the four
duplicate 04.013 assignments are removed; all other 22,944 work rows and their
execution states are unchanged, including all 256 DONE rows.

`tools/check_painter_enum_routing.py` reproduces this bounded correction from
public checkpoint 818ed559. It is a historical checkpoint checker; later valid
execution-state updates need their own evidence. Older fixed registration/routing
digests are retained unchanged and are not used as current acceptance claims.

This closes the original common generation/build condition. Full feature API
coverage, runtime enum compatibility, source-generation release checks and
Windows/macOS acceptance remain separate tasks.
