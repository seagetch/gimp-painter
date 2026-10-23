# Bounded original brush capture (02.010)

Evidence: `migration/fixtures/legacy-brush-strokes/README.md` and three records.

Measured: 54 original-engine calls across analytic pen, randomized ink and
smudging watercolor; complete boundary inputs and actual per-call state/seed;
source-matched assets and effective settings/resources; nonuniform initial
pixels; native XCF/final RGBA; exact isolated-reader RGBA roundtrips and repeat
original execution; independent preserved-binary XCF reopen checks.

Validation: three record/file checks, 11 existing format tests, nine new
runtime-evidence tests. Full patch applies to pinned original source files.

Scope: synthetic post-transform inputs executed by the old engine. Physical
tablet/UI-path measurements, imported-event or GIMP 3 replay, enabled external
brushmark/texture and the other untested variants remain unproven. Original
runtime warnings are retained and do not become a clean-runtime pass.

Acceptance: 02.010's written requirement to preserve representative brush input
sequences and result pixels is met within this explicitly bounded engine-level
scope. Physical tablet/UI capture and port replay remain separate acceptance
gates. The preserved installed reader checks document structure; exact pixel
comparisons use the rebuilt original-reader harness, not the installed binary.
