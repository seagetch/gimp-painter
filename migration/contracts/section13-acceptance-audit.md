# Original section13 acceptance audit

This audit retains every original criterion verbatim below. It separates direct
acceptance evidence from dependency closure: no task is marked DONE here, and
neither an unlimited scene matrix nor a new requirement is invented. Evidence
is checkpoint-scoped; combining receipts does not imply a clean whole-HEAD run.

| Task | Original criterion | Direct evidence / remaining work |
|---|---|---|
|13.001|alpha・premultiplication・丸め・clamp の仕様を記録|`legacy-seven-formulas.md`: pinned unchanged old formulas, alpha/straight convention and integer rules;29,601,376 exact comparisons. Direct documentation criterion evidenced. Dependency09.017 remains TODO;10.018 DONE.|
|13.001/gegl-operation-route|7 モードの式、Babl 形式、mask、opacity、group 順序と operator の採用・追加を一件ずつ記録|`legacy-mode-operation-routes.md`: seven explicit mappings and dedicated byte operator, Babl/coverage/order contract. Group strength and float work now supplement the initial checkpoint. Dependency13.001 open.|
|13.002|旧代表画素と一致|Erase raw23: old formula and genuine captured/native pixel comparisons; dependency13.001 open.|
|13.003|旧代表画素と一致|Replace raw24: old formula/rounding and genuine captured/native pixels; dependency13.002 open.|
|13.004|旧代表画素と一致|Anti-Erase raw25: old formula and genuine captured/native pixels; dependency13.003 open.|
|13.005|source/destination の扱いが一致|SRC IN raw26: pinned full operand ordering and intermediate rounded alpha; source-derived and native comparisons; dependency13.004 open.|
|13.006|source/destination の扱いが一致|DST IN raw27: full operand swap before the same old IN/Replace route; source-derived and native comparisons; dependency13.005 open.|
|13.007|source/destination の扱いが一致|SRC OUT raw28: source-derived complement-left intermediate and native comparisons; dependency13.006 open.|
|13.008|source/destination の扱いが一致|DST OUT raw29: full operand swap before complement-left route; source-derived and native comparisons; dependency13.007 open.|
|13.009|表示名と内部値が一致|`legacy-mode-ui/`: actual mapped combo changed signals for all seven labels/enums; authenticated portrait screenshot with visible oncanvas UI. Not pointer event replay. Additional9 float labels checked separately. Dependency13.008 open.|
|13.010|演算順が旧仕様と一致|Old source-extracted numerical reference;72 genuine old projection cases and216 direct dispatch cases preserve distinct opacity quantization and mask sentinel/order.54 XCF cycles are supporting transport evidence, not the ordering oracle. Dependency13.009 open.|
|13.011|隔離・通過・順序を現行基盤と整合|54 nested route cases, ordinary reduction controls, causal byte rounding fix,18 reorder contexts, reparent/mask Undo,72 isolated strength and108 pass-through strength cases. Group interpolation explicitly follows current foundation; old flat child reference is not mislabeled old grouped capture. Dependency13.010 open.|
|13.012|旧8bit再現と高精度拡張を区別|Separate immutable byte IDs and opt-in float/nonlinear IDs/operator; independent continuous oracle, six-storage paths, mask/extent endpoints, public labels/GType. Float32 arithmetic explicitly distinguished from double storage. Versioned capsule/fallback persistence clean proof is being completed separately; no old-reader appearance promise. Dependency13.011 open.|
|13.013|透明・半透明・端値・順序を比較|Byte exact matrix plus float326,196 vectors/652,392 aliases, native graph/storage/zero-weight endpoint matrices and grouped order tests. Direct numerical criterion now has both byte and float evidence; prior 'float proof remains open' note is stale. Dependency13.012 open.|
|13.014|CloneLayer と複合構造の見た目が一致|`legacy-anime-current/`: genuine old synthetic anime initial/edited fixtures, Clone type/source and hierarchy, exact7680 pixels each, rendered portrait screenshots. Current edit replay not yet demonstrated. This does not establish all composite fixture families: watercolor/old Edge runtime stays SKIPPED; no old runtime rerun. Dependency13.013 open.|

## Concrete remaining boundaries

1. Original09.017 buffer Undo prerequisite is still TODO in the ledger. The
   section13 formula and numerical work does not waive this prerequisite.
2. The float capsule/fallback storage integration belongs to the parent's XCF
   work and requires its clean-build proof before its new persistence claim.
3. Anime currently proves opening both captured states, not reproducing the
   edit/Undo sequence in the port. A native current-core replay would close this
   known evidence gap without running the skipped old application. New GUI
   evidence must use the coordinated clean build, not dirty primary sources.
4. Broader composite coverage must preserve explicitly skipped unsupported old
   Edge/watercolor runtime boundaries; no substitute positive fixture silently
   establishes those unsupported paths.

No historical source formula, task description, dependency or acceptance text
is changed by this audit. Direct criteria and dependency status should be reviewed
separately when closing tasks; existing finite representative evidence should not
be discounted merely because it is not an impossible exhaustive scene proof.
