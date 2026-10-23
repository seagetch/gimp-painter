# Channel property ordering (12.015/field-owner-checkpoint, still DOING)

The native writer emits legacy OPACITY/COLOR and modern FLOAT_OPACITY/FLOAT_COLOR.
Six generated record orders preserve precedence within each representation pair,
while interleaving the independent RGB and alpha fields. Each is opened, compared,
saved and reopened through the actual registered XCF procedures. Complete records
move without changing offsets, record sizes or values; unknown/other properties
remain present. This is not a malformed-file or all-owner completion claim.

The unchanged candidate test exposed a real loss when FLOAT_COLOR preceded the
byte-opacity record: gimp_channel_set_opacity calls gimp_color_set_alpha, which
converts RGB through linear float before restoring its format. An already loaded
0.2700000107 component became0.270000219. The loader now changes alpha directly
in its explicit sRGB-double color representation, as its existing FLOAT_OPACITY
route does. No general interactive channel setter, file schema, field/default,
legacy representation precedence or test tolerance was changed.

All6 orders and2 rounds now pass with RGB epsilon2e-7 and alpha epsilon1e-12.
Full registered97 native cases and97 focused ASan/UBSan/float-cast cases pass:
XCF4, Open31, roundtrip50 (including original slow large-record case), provenance12.
Original60/180-second budgets remain. Instrumentation is41 units plus40 RTTI-only
units, not the whole application or dependencies; LSan remains disabled.

The existing helper seals its selected source/header list before compile/run.
The new owned include and three owned source files are additionally archived
after the run; these were unchanged throughout the run and equal the detached
source at capture. This is explicitly not a complete dependency-header seal.
The source-only1378-field/873-candidate/16-route/46-control gate uses an isolated
committed checkpoint plus this loader change, not siblings' unfinished edits.

The original185-row field ledger is unchanged. This adds bounded evidence for
XCF-F042/F046/F074/F078; old-type/mask-specific permutations, malformed properties,
recursive graphs, big-endian/platform behavior and final integrated gates remain.
No new GUI/physical input claim is made for these file/model tests.
