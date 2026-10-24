# Known legacy home-profile discovery

Pinned original configure.ac declares `.gimp-painter-gimp_user_version`: the
known 2.8 directory is `~/.gimp-painter-2.8`. Before this patch the real native
installer ignored that directory when no standard source existed. A fresh-process
private-HOME native assertion observed selected source NULL despite its presence.

Only after all existing discovery fails, the patch accepts that exact directory
as version2.8. Standard/XDG/platform/explicit-directory precedence is unchanged.
No hypothetical Painter versions or arbitrary paths are scanned. Directory name
does not establish Painter ownership: existing content detection still decides
migration/remapping. A regular file is rejected. No product introspection API
was added; the private provider has an appended test-only field accessor.

Final native9 and selected three-translation-unit ASan/UBSan9 pass:
missing, regular file, lone Painter folder, standard precedence, XDG precedence,
explicit modern-directory precedence, Unicode/spaced HOME, genuine Painter
migration+retry, ordinary non-Painter-content migration+retry. Source and unrelated
fixture-file hashes are preserved in every invocation; destination edits survive
actual `gimp_user_install_run` retry. The ordinary folder is not classified as
Painter merely because of its name. Standard gimprc output is legitimately
normalized; the test checks its saved value, not accidental byte formatting.

18,814 frozen source files, 1,322 linked inputs and127 runtime library identities
remain unchanged. Clean source is exact2ef predecessor plus explicitly recorded
readiness/preset-help and Filter-test overlays, not a current whole-HEAD claim.
The installer and profile parser were privately compiled from committed cddaf
bytes, with only the fallback inserted into the candidate installer. All shared
build/source paths stayed read-only. Instrumented units are installer, profile
parser and test; no whole-app/vptr/leak or cross-platform claim is made.

Every HOME/XDG/GIMP3_DIRECTORY/TMP and helper output location is private. A tiny
valid default-tags XML is an explicit test-data fixture because this clean build
has no installed default tag resource. No actual user profile/assets were changed.
Initial missing gio include, missing installed tag fixture and overly strict
standard-output byte oracle failures are retained separately and are not product
reds. `red-test.cpp` and `green-test.cpp` hashes preserve exact tested revisions.

The existing separate nine-case native profile suite was not rerun: its clean
binary/objects had been removed and the read-only slot was released promptly.
This proof does not replace that suite or certify all profile resources, hardware,
crash recovery, macOS/Windows discovery, installed-prefix equivalence or UI. These
are model/filesystem tests; no new screenshot or portrait-rendering claim. Original
30.010/30.011 acceptance/dependency closure remains open.

`build.py` is the actual final private-link harness with explicit local roots and
requires a coordinated frozen build lock. Initial-red harness is retained under
`missing-tags-fixture/`. For another environment reconstruct equivalent pinned
source/dependencies and adapt roots explicitly.

```
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/profile-home-discovery/validate.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/profile-home-discovery/check-controls.py
# After parent commits only the reviewed product patch:
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/profile-home-discovery/validate.py --commit <sha>
```

## Existing-destination and alias boundary

Committed `app/app.c:265–273` is the sole production constructor call: installer
entry is guarded by destination query type != DIRECTORY, using QUERY_INFO_NONE
(which follows ordinary directory symlinks). Thus an already existing known
Painter home destination/alias skips first-run installation. `caller-app.c`
archives the exact committed caller and hash. For direct API calls, existing
`gimppainterprofile.cpp:430–445` rejects equal/contained destinations before
migration/preservation writes; its directory creation also rejects symlink
components, while no-overwrite publication preserves existing files.

Four additional isolated probes reuse the sealed native executable without a
rebuild: identical path, canonical dot-dot alias, directory symlink alias, and
identical path with unproven/standard content. Each real public installer run
returns FALSE before modifying any source file or adding a source file. The
existing positive-migration assertion intentionally fails at that exact return
value; the driver treats only this precise failure plus unchanged source tree as
successful rejection evidence. This is documented in `alias-calls.json`, not
misreported as four extra positive migration cases. Dynamic library hashes are
unchanged before/after. No arbitrary filesystem alias or concurrent rename/race
guarantee is claimed, and no unrelated generic discovery policy was redesigned.
