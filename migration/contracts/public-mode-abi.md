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
