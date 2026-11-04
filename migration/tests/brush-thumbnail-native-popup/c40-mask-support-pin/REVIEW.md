# C40: repair omitted mask-provider support pin

Local metadata/test correction based on public C39
ebafc05949389229b9dacbba35a5217523be229e. No native repair, acceptance or publication.
Run57 is still in progress at preparation; only completed normal-job evidence is
used here. Brush21/21 passes both scales, but the independent mask-provider gate
fails before old/fixed/full107 tests can run. Do not report those unrun tests passed.

## Failure and minimal correction

C39 changed app/tests/tests.c to finish icon-theme setup before window restoration.
Its reviewed source SHA256 is
bf982e17fbe7548bcd672698aea969fc2bb6f4355e3cf737e6ac0aae80efb195.
The global runtime source seal was refreshed, but canvas_mask_provider.py's separate
SUPPORT pin still required the preceding source hash56303878e316e41fcb7c439b199cde5bd9fc4ac84b46b48efcbfbc79737747e7.
Run57 provider.json correctly reports pinned-source-mismatch; the checkout is clean.
Enumeration of all nine provider pins identifies only this support source mismatch.

Update only that reviewed SUPPORT hash. Keep all runtime provider admission checks,
object/link provenance, exact old-control assertion, full107 and leak policies.
No production source, test harness code, fixture or native deadline changes.

## Why the1140 local tests missed it

The synthetic provider fixture generates fake source bytes and monkeypatches PINNED
to their corresponding hashes, which is necessary to test provider mechanics but
cannot validate the repository's committed pin map. Existing source integration
checks covered production/test/old fixture, not shared tests.c support. The global
seal consistently described C39, so its own checks could pass despite the independent
stale map. This was an integration coverage omission, not a false native failure.

Add real-checkout checks for every provider pin, exact inventory including SUPPORT,
and agreement with the independently sealed source/bundle map. These tests never
mock PINNED or calculate replacement policy values. Before updating the pin, the
new checks fail on exact C39 support mismatch; after correction they pass. Existing
synthetic source-mutation rejection remains. Run these source checks in bootstrap
before expensive builds. Refresh only the workflow integrity hash and derived seals.

## Other old support hashes are not blanket replacements

The matched-preclick experiment intentionally builds frozen e25 and b671 source
arms, not current HEAD. Its COMMON/manifest hashes remain valid for those arms and
must not be rewritten to C39. The geometry_source_sha256 map in pins.json is legacy
supplementary source metadata, not consumed by the current runtime gate; its old
receipt context is retained. This correction does not silently retarget historical
comparisons or broaden the mask provider's accepted source set.

Native old/fixed/full107 revalidation is still required after publication. All26
protected files, original private3s predicate, sanitizer settings and leak guards
remain unchanged. Task308 stays DOING / NOT ACCEPTED. Root must review and obtain
specific publication authority; no GitHub write is performed here.

## Local validation and retained failed attempt

The new integration suite fails twice with exact C39's stale SUPPORT pin, then
passes12/12 after the one-hash correction. Provider synthetic127 tests and reviewer
matched-driver20 tests pass. WBS, protected26 and whitespace checks pass; app source
is byte-identical to C39. Independent review found no required correction.

Initial full1143-test execution recorded two failures in unchanged owned-child
cleanup timing tests (startup record absent and debugger termination not observed),
while a parallel provider-only suite was running. That failure log is retained;
its cause is not established. Without changing deadlines or test code, isolated
cleanup38 tests pass, followed by a serial full1143-test PASS. No failure is waived
or hidden, and these local results do not substitute for native provider execution.
