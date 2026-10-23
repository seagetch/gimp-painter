# Original 04.016: installed startup

The Linux x86_64 default and HTTP-enabled builds were installed into separate
DESTDIR trees, assembled with their pinned runtime dependencies, and relocated
to a path containing spaces and Japanese characters. The application and all
Filter descendants ran in a filesystem namespace where `/workspace` did not
exist. The source tree, both build trees, generator copies, configured prefix
and development dependency roots were therefore unavailable at their paths.
Only the installed runtime, copied test inputs, explicit test output and system
runtime files were mounted. No source/build directory was moved or overwritten.

Both configurations passed these routes:

- Normal console resource restore, create/fill/Save/reopen and orderly shutdown
- Real GUI resource restore, visible reopened image and orderly shutdown
- The installed Filter worker and native Blinds plug-in, exact legacy pixels,
  Save/reopen, and cleanup of observed processes and private worker profiles

GUI observation used the existing desktop display. Xvfb could not create a
Unix socket in the shell environment; no socket/security settings were changed.
The final GUI probe binds its external observation to a new run identifier and
the exact reopened image name. The screenshot and observation record are test
evidence, not a substitute for the application assertions and shutdown result.

Each normal restore loads all 177 installed Painter brushes and eight layer
presets. Their exact file identities appear in the host's saved tag cache and
in the loader output. This proves factory loading, rather than merely finding
files on disk or querying plug-in-side GType proxies. Both GUI runs load the
nine Meson-installed dynamic modules, and the host tool configuration contains
Painter MyPaint, Fill Brush, Painter Smudge and Perspective Guide registrations.
The default GUI also loads a synthetic current-format brush and preset from its
explicit user profile, retaining their bytes. No current user profile is used.
Both GUI startup/shutdown logs contain no warnings or critical diagnostics.

## Packaging correction and controls

The runtime recipe previously accepted only one dependency directory. The
restored HTTP build has libsoup in a separate pinned directory. Repeatable
`--extra-deps` now declares additional roots explicitly. Runtime assets,
libraries, locked archives, notices and the schema compiler retain root/path/hash
provenance. Identical duplicates are accepted in declared order; inconsistent
copies, ambiguous archives, root escapes and non-baseline host libraries fail.
Extracted absolute `/usr/...` symlinks are resolved within the selected package
root and safely relocated, preserving prior supported behavior.

The 60 packaging controls and five startup-receipt controls pass. A real
negative startup with the installed Painter brush directory hidden exits
normally but is correctly rejected by the acceptance probe for missing resources.
The final recipe restaged both configurations: all 5,057/default and 5,059/HTTP
runtime files, modes and links equal the tested stages. Packaging metadata may
differ; no release candidate is claimed. The application binaries were built by
the normal native graphs, using the previously documented guard for 17 known
source-writing generators; compiler and linker commands are unchanged.

The initial console probe had incorrect expectations for the Small Tiles PDB
name and normalized tag-cache identifiers. Its failing output and exact probe
are preserved separately. Later guard improvements anchor the module set and
bytes to the staging manifest, reject stale GUI observations, and require the
Perspective Guide registration. The final default GUI exercises these guards;
earlier accepted scripts remain recorded with their actual hashes.

## Original source obligations and remaining work

All 39 original verification duties retain their identities and assignments:
19 module/source registrations, six link-only hunks, 13 initialization/lifetime
entries, and the Painter profile-path identity hunk. The installed runs verify
the current initialization/restore/finalization boundary. They do not complete
the broader implementation contracts under 30.016.

For the profile-path hunk, 04.016 proves installed and explicitly selected user
resource lookup. Automatic discovery of `~/.gimp-painter-2.8` is still missing
from the old-directory selection path and remains open under 30.010/30.011.
Missing-resource information retention remains under 36.022. None of those
implementation duties are marked done by this startup evidence.

The optional HTTP listener is not enabled during these startup tests. Full
endpoint behavior, complete legacy feature equivalence, aggregate release
freshness, dependency source delivery, GTK AT-SPI action behavior, Wayland,
Windows/macOS and physical tablet acceptance retain their separate gates.

Run `python3 -B tools/check_painter_installed_startup.py` for the frozen evidence
and source correspondence check. `tools/check_installed_startup.py` is the live
probe; it requires an externally prepared isolated filesystem and, for GUI use,
a fresh visible-window observation. The archive contains exact commands,
executed scripts, reports, logs, staging manifests and synthetic screenshots.
