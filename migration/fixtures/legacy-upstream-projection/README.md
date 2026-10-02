# Upstream historical XCF pixel reference

The unmodified upstream fixture `app/tests/files/gimp-2-6-file.xcf` was opened
by the real pinned old application. Its direct projection was copied through
`gimp-layer-new-from-visible`, then exported with hidden RGB preserved. This is
an independent standard historical XCF, not an authored Painter-only scene.
The input file blob/SHA, executable SHA, script, full log, exit code, PNG and raw
RGBA are recorded. No parser, compositor or fixture bytes were modified.

Use this to test historical shared-layout arithmetic in addition to metadata
and enum identities. Modern generated low-version XCF files carrying standard
properties 32/33 must retain their separately verified modern mode identities.
A shared layout identifies a canonical historical representation, not proof of
which application authored an arbitrary file. Explicit standard recovery stays
available; genuinely dual-valid extension contexts must remain ambiguous.
