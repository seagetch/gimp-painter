# Public Painter mode identities

The native app assigns Painter modes64–72, after the upstream private
`GIMP_LAYER_MODE_ANTI_ERASE=63`. The public PDB enum generator skips private
entries before advancing its implicit value counter. App-only pixel tests did
not detect that the generated libgimp header consequently assigned63–71.
That would send a different mode from a plug-in than the same named app mode.

The first Painter enumerator now explicitly reserves64 in the source header.
The standard enumgen/enumcode pipeline therefore emits the correct numeric
values into the public header. No existing native app or upstream enum value
changes, and the private upstream mode is still not exported. This follows the
current3.0 baseline's public/private allocation; another target version must
revalidate its allocation rather than blindly assume64 is available.

`tools/check_painter_public_modes.py` checks all72 public names against the app,
all65 app values in the pinned unmodified3.0 baseline, and all nine Painter
identities. A registered Meson test catches this hole regression in source and
generated metadata. The Linux proof also compiles separate C11/C++14 probes
against both headers and checks every public value through the newly built
libgimp GEnumClass, including the absent private slot63.

The before-fix header and nine observed mismatches are preserved. The first
runtime probe inherited an installed-prefix library search path and selected
the earlier libgimp; it failed and remains recorded. The corrected runner
explicitly selects build-tree libgimp and records its checksum and limited
library-path reproducibility information. The final compile and runtime probes
pass. This is API identity acceptance, not a claim that every PDB transport or
Script-Fu legacy alias is implemented or tested.

## Generated PDB table closure

The source-tree `pdb/enums.pl` generated table is also retained in Git. It must
match all72 public values and declare the set noncontiguous because hidden
native slot63 is absent. The generator already produced the corrected64–72
Painter values; this follow-on commits that generated result and extends the
same checker to reject table/header divergence and an incorrect contiguity flag.
The four C11/C++14 compile probes and built libgimp GEnumClass check pass again,
plus four Python checks including deliberate old-shift, hidden-mode and
contiguity mutations. This verifies code-generation metadata, not every PDB or
Script-Fu transport call. Original pre-correction and first-checkpoint reports
remain unchanged; `painter-public-pdb-modes.json` records this exact follow-on.
