# Original 04.015: shared-header incremental builds

The current default and HTTP production graphs were checked from the GUI,
console and isolated Filter worker roots. These closures contain 1,370 and
1,375 compiled objects. Configured but unbuilt tests and the separate 04.014
header-probe archives are outside this production measurement.

Two shared headers cover all current translation-unit counterparts of the 28
original Makefile verification duties:

| Header | Configuration | Changed objects | Changed archives | Relinked finals |
|---|---|---:|---:|---:|
| painter/gimp-painter-visibility.h | default | 69 | 13 | 3 |
| painter/gimp-painter-visibility.h | HTTP | 74 | 15 | 3 |
| core/core-types.h | default | 1,159 | 29 | 3 |
| core/core-types.h | HTTP | 1,164 | 31 | 3 |

The expected object sets come from current compiler dependency records, checked
against `ninja -t deps`. Archive and final-target sets are restricted to actual
reachability in the full native Ninja graph, including SDK edges. Every changed
set equals its expected set; every other monitored production object/archive
retains its modification time and size. Temporary comment mutations change no
API or enum value. Each header is restored byte-for-byte and rebuilt, producing
the same required object/archive/final sets. Immediate builds after mutation and
after restoration perform no object, archive or final-binary work.

The two configurations share source mtimes. After switching configurations the
graph must be primed before taking a new baseline, even if source bytes were
restored. The accepted default measurements were retained while the HTTP baseline
was normalized, rather than repeating those measurements. A final synchronization
and no-op check leaves both graphs current. The existing VCS-tag commands may
still run; their unchanged outputs do not cause consumer recompilation or links.

## Scope corrections during verification

An initial archive expectation included `app/tests/libapptestutils.a`, which
reuses production objects but is not reachable from these three final targets.
Restricting archive ownership to production reachability removes that test
archive. A first incomplete graph copy also omitted SDK nodes. Both harness
scope issues were corrected before acceptance; no product dependency failure
was found. Diagnosed measurements and the exact earlier driver are retained
separately from accepted measurements.

All 28 original verification identities remain intact: 19 current module/source
registrations, six final-link-only hunks and three private-header registrations.
The two broad mutations cover every current production consumer in those mapped
groups. `gimpclonelayer-handle.hpp` and `gimpfilterlayer-handle.hpp` have no current
production consumers; their explicit zero sets and 04.014 compilation probes
are retained. The five HTTP source files and two HTTP private headers correctly
have no disabled-production consumers.

`source-duty-review.json` contains the current mappings and source hashes.
Historical archive-extraction results remain in their original 04.009 reports;
they are not substituted for current incremental measurements. This task changes
no product source or build rule and claims no runtime, full feature or platform
acceptance. Remaining feature implementation obligations keep their states.

The structural WBS/source-ledger checks and this task's current source seal pass.
Older checkers that freeze all unrelated execution-state rows describe their
own checkpoints; their fixed digests are not rewritten as later tasks progress.
