# Original13.013 Script-Fu source duty01.002/002472

The old `afa43fae:plug-ins/script-fu/scheme-wrapper.c` `old_constants[]`
exposed ERASE, REPLACE, ANTI-ERASE, SRC-IN, DST-IN, SRC-OUT and DST-OUT.
The current GI route exposes prefixed `LAYER-MODE-PAINTER-*`, not these names.
The narrow fix restores the seven short names to byte-compatible IDs64–70,
not modern ERASE58/REPLACE62 or opt-in float IDs.

`expected-red/`: exact pre-fix production table and registration/enum bodies
executed by actual current TinyScheme: seven missing-alias checks fail; seven
GI identities and seven no-collision checks pass. Repeated missing symbols
trigger TinyScheme's existing error-port warning; this is a retained negative
receipt, not a clean successful run.

`normal/` and `sanitizer/`: same production extraction mechanism after the fix,
23 checks pass: seven old aliases, seven prefixed GI identities, seven checks
that GI had not already bound the short names, and two unchanged modern modes.
The sanitizer instruments the probe and all three TinyScheme implementation
units, not GLib/GI/dependencies; ASan/UBSan halt on errors, LSan disabled.
Generated C, logs and source/typelib hashes are retained. No full environment.

`production-compile.log` is empty successful output from separately compiling
whole current scheme-wrapper.c with the configured native build compile command,
replacing only source/output and prepending current source include directories.
This guards extraction-only syntax blind spots. It is not a linked whole app.

This is actual Scheme interpreter evaluation with exact extracted production
binding code and the built Gimp typelib, not a synthesized static enum check.
It does not claim full plug-in startup, PDB calls, or GUI interaction. Other
native mode tests own arithmetic and GType/public enum ABI. No old runtime ran.

Reproduce (after loading native environment):

    python3 tools/check_painter_scheme_aliases.py --build BUILD --output OUT
    python3 tools/check_painter_scheme_aliases.py --build BUILD --output OUT --sanitize
