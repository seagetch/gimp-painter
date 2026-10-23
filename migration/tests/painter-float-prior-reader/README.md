# Pre-extension native-port reader behavior

This is a captured prior-reader diagnostic for proposed13.012 float identities,
not a float implementation or old GIMP2.8 runtime test. Before adding float enum
members, current native port reader opens nine generated standard XCF files
whose PROP_MODE payload is73 through81. All other bytes/offsets are unchanged.
One exact registered diagnostic test passes1.28s under authenticated portrait
Xvfb. Each loaded layer retains the unsupported numeric mode and emits nine
Gimp-Layer-Modes critical diagnostics. The test fatal handler suppresses only
the expected gimp_layer_mode_info assertion to inspect subsequent native load
behavior; unrelated warnings/errors remain fatal. No appearance/backward
compatibility guarantee is made. No resave or rendering was tested here.

Crucially, this is NOT a clean Normal-mode conversion: the unknown ID remains
on the layer, while internal mode-info lookup falls back to its first entry.
Float-mode files therefore require a float-aware reader for intended appearance.
The new modes must be explicitly opt-in, never silently replace existing IDs.

reader-sources.tar.gz freezes relevant prior read/write/native-mode/enum and
exact test source. All production files were verified byte-identical to the
main committed pre-extension sources at capture. It is a relevant-source seal,
not an entire application/dependency compiler archive. The uncommitted private
diagnostic harness was registered only in the isolated build source, not the
main current suite (where float IDs will become known). Source namespace proof
is maintained separately by the float-kernel task. An initial build attempt
failed before tests because a registered preset header was absent in the build
source; that diagnostic is retained, not counted. The dependency was copied
from committed source without weakening the header gate.
