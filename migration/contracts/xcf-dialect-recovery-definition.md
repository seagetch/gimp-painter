# Ambiguous dialect cases and recovery (10.016)

This enumerates source-defined decisions and recovery paths, not proof of all
historical files or a newly executed Open test. The byte-decoder and Open
integration contracts contain existing fixture/runtime evidence. A metadata
candidate is not full image/pixel validation and an old-looking integer is not
permission to reinterpret standard data.

| Situation | Safe decision / remaining ambiguity |
| --- | --- |
| v004 zero-word dual-layout example | Old END at26/tables34,38 and standard precision0/END30/tables38,42 can both parse. Signature or first word cannot choose. Retain ambiguity. This example is synthetic, not a genuine old Open fixture. |
| v0–3 both parse with no collision fields | Shared layout, not writer identity. Application can retain historical common-mode interpretation; do not claim that the writer was Painter. |
| v0–3 mode>=23 or layer property32/33 | Can mean different dialect semantics despite low header version. Both plausible candidates remain ambiguous. |
| Both metadata candidates complete | No generic tie-break by file version/name. A bounded valid standard candidate plus conclusively invalid old extension layout may exclude old; incomplete/limit evidence cannot. |
| Only one candidate completes without unresolved diagnostics | Candidate structural evidence; selected native loader still performs actual object/pixel decoding and may fail/recover. |
| Either candidate reaches resource/work/allocation/recovery limit | Inconclusive, even when the other candidate completed. Do not treat budget exhaustion as invalidity. |
| Neither metadata candidate completes | Invalid metadata probe does not prove native salvage or historical recovery impossible. Expose explicit recovery. |
| Validated header above4 | No audited legacy dialect; retain standard native recovery even when metadata probing is limited/damaged. Legacy-forced recovery rejects versions outside0–4. |
| Explicit recognized new standard image marker | Complete image-property sequence and validated versioned namespace can declare standard independently of later object-work limits. Later namespace declarations can override earlier ones; don't decide from an incomplete prefix. |
| Unknown/future capsule or duplicate dictionary keys | Not an automatic trusted standard marker. Preserve raw data and apply the same ambiguity/recovery policy. Repeated namespace records are distinct from duplicate dictionary keys: marker selection follows later-declaration precedence. |
| User chooses an explicit recovery type | Fresh independent attempt on unchanged bytes; overrides automatic marker selection, subject to requested dialect's supported header range. |

## Recovery entry points

The actual Open file-type list includes `gimp-xcf-load-standard` (“GIMP XCF image
(standard recovery)”) and `gimp-xcf-load-painter` (“GIMP Painter XCF image (legacy
recovery)”). Neither has suffix/MIME/magic automatic matching, so they cannot
hijack ordinary detection. When automatic selection is unsafe, the diagnostic
names both choices and includes both candidate diagnostics/byte offsets.

Each attempt snapshots the original input and constructs through the requested
native path. It never overwrites the input, silently rewrites the header, casts
colliding legacy mode numbers, or performs a trial destructive load into an
already published image. Recovery is an explicit interpretation attempt, not a
guarantee of success or proof of provenance. Keep the original immutable source
and candidate diagnostics even if selected construction ultimately fails.

The verifier checks these source branches, recovery registrations, and existing
ambiguity/inconclusive test definitions. It does not execute native Open or
certify arbitrary corruption, late callbacks, or UI presentation.
