# Original 04.014: registered C/C++ header compilation

The `painter-public-header-compilation` Meson test now depends on real C11 and
C++14 compile targets. Meson builds them with its selected compilers, dependency
objects and include paths; the test does not invoke a host compiler or execute a
target program. The two static archives are neither installed nor built by
default. Running the named test builds its prerequisites automatically.

The reviewed scope is 102 C-facing headers: the 101 unique routes from original
04.004 plus the dependency-free visibility header added in 04.012. Each header
is included twice using the real application type prelude, with the historical
outer `extern "C"` adapter removed. Two additional XCF cases supply the internal
XCF context; all 18 configuration macros expand in both languages; the visibility
macros also compile without the application prelude. Every generated translation
unit asserts its actual language.

The three original private-header registration duties are retained. Their eleven
legacy headers map to 21 current `.hpp` counterparts, compiled only as C++:
19 in the default configuration and two more with HTTP enabled. The C-facing
HTTP header is tested in both configurations and requires no Soup dependency.
Generated brush header prerequisites are attached to the C++ compile target.
No claim is made that private C++ headers are valid C or that their feature
behavior is complete.

Both reviewed JSON inventories explicitly participate in Meson reconfiguration,
because they determine output names and prerequisite lists. The generator rejects
missing/duplicate routes, lost legacy source-duty mappings, misplaced `.hpp`
targets and newly introduced production C headers outside the reviewed scope.
It preserves generated source timestamps when content is unchanged. The test
compares current source fingerprints against the generated manifest and rejects
stale inputs or missing compile archives.

## Results

- Default: 106 C and 125 C++ probes, 231 total, registered Meson test PASS
- HTTP: 106 C and 127 C++ probes, 233 total, registered Meson test PASS
- All 4,041 pre-existing compiler vectors are unchanged; only the 464 test
  translation units are added
- Both new archives are absent from the installed/default-built target set
- Eighteen bounded controls matched their expected outcomes: five invalid route
  inventories, six real compiler failures, four valid compiler controls, two
  successful generations and one stale-input rejection

The negative compiles cover C++ templates leaking into C, a C-compatible name
reserved by C++, unguarded repeated structs and both wrong-language selections.
An initial validator missed removal of a route sharing a public umbrella target;
route-count and legacy-duty identity checks now reject both deletion and
same-count reassignment. Initial diagnosed evidence is retained separately from
the final acceptance evidence. The final source matches both registered reports.

The existing upstream ignored scalar-return qualifier warnings stay visible via
`-Wno-error=ignored-qualifiers` when supported. No other warning is suppressed.
This is native Linux compile/registration acceptance. Cross builds do not require
executing target code by construction, but no Windows/macOS/cross execution is
claimed. C linkage remains the separate 04.004 ABI contract, not an inference
from successful syntax compilation.

`migration/tests/header-registration/source-duty-review.json` maps all 92 original
verification rows to these probes: 89 C-boundary duties and three private-header
registration duties. No assignments are removed or retargeted; unrelated work
rows and all immutable historical header inventories remain unchanged.
