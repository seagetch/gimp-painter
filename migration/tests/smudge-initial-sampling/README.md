# Original23.002: transparent first contact retains the old initial sample

This bounded fix advances「初期採色と更新が一致」. All52 original source duties,
criteria and dependencies remain OPEN; no new DONE child. Blending-output and
size/reallocation remain separate23.003/004 obligations. No old application is
executed here; existing genuine old1024-kernel and48-scene recordings are reused.

## Causal production defect and narrow fix

Old `gimp_smudge_paint` calls start on the first successful overlapping contact,
then motion. The latter exits for zero opacity before accumulator updates. The
native port instead returned for zero opacity before creating/seeding its
accumulator. A transparent first contact therefore lost the original color.

The five-line production change permits first overlapping initialization before
that return. If an accumulator already exists, zero opacity still returns early;
after seeding it returns before blend/update/paint. A wholly outside area still
returns before allocation. The existing retained owner/resources and `current()`
revalidation before transformed area/seeding are preserved, as is the enclosing
owned-frame validity check. No geometry, rate, shading, mask or blend arithmetic
changes. Existing safe allocation/coordinate bounds remain, not universal old
behavior beyond those bounds.

The final exact same test source gives original-production red versus candidate
green. Original has4 observed positive-pressure native paint calls but paints
right-sideR220 where old initialization requires retained left-sideR40. Candidate
passes interior, edge, negative-clipped and outside→first-overlap variants:
4/4/5/4 positive dabs,486/486/522/486 changed bytes, exact seedR40/R40/R220/R40.
All repeat-zero motions leave drawable bytes unchanged, and each final stroke
Undo/Redo round-trips. Explicit10% brush spacing and a second constant-opacity
segment ensure the tested pixel has an opaque dab, not an opacity ramp.

The unchanged old paint orchestration is compiled through explicit start/motion
observer adapters. They prove call order, failed-overlap retry and no reseeding;
they are not an old pixel emulator. Fixed-color/rate100 expectations follow the
old byte-kernel endpoint, independently checked across all1024 captured cases.
The new initial-zero fixture usesRGBA; existing48 scenes coverY/YA/RGB/RGBA,
rate0/50/100, blending and size dynamics. Do not relabel this as new zero-contact
coverage for every format or physical device.

## Results and scope

Final normal registered tests:24native cases (original13 lifecycle +10 generic
+1new), old48scenes on both synchronous and stepped owned paths,193records and
2,952,800bytes each exact;1024pixel cases891,487bytes exact. All scopes retain60s.
Authenticated Xvfb is1080×1920×24. No UI production change or new UI image claim.
The preceding committed Perspective oncanvas portraits are unrelated evidence.

Focused ASan/UBSan/float-cast-overflow:18 native units,24cases plus both complete
old48-scene routes PASS. Pure1024kernel focused sanitizer also PASS. Host/dependency
units outside the listed closure remain uninstrumented; leak detection is off.
No sanitizer check is disabled. The historical harness initially mixed an
instrumented BindingStore with a no-RTTI Filter-config owner and failed at global
config startup before Smudge tests. The new harness adds the existing committed
private Filter-config owner/archive to the compile closure; it imports no current
Filter candidate or held provider change. That failure remains preserved.

The57-input archive is sealed before final normal/red builds. Focused reports
retain their own hashes measured before their earlier compilation; every such
hash matches this archive. We do not claim the later archive was created before
that earlier sanitizer compilation. All9 held providers remain committed private
baseline bytes, checked before each phase. No raw environment JSON is copied.

## Preserved fixture mistakes, not causal proof

The first1px-final-motion expected-red had no visible dab and is NOT accepted
as production causal proof. Private tracing showed the candidate already seeded
R40 but no positive motion executed. An8px retry still relied on implicit brush
spacing; only the explicit-spacing observer and constant-opacity final sequence
establish the final red/green result. The clippedR41 diagnostic was partial opacity,
not relaxed to a tolerance. All exact assertions remain; no historical log is
rewritten as a pass. Final red/green source and binary hashes identify the admitted
sequence separately from these diagnostics.
