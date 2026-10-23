# Explicit GTK 3 dependency repair (34.003/gtk-atk-menu-guards)

This patch is for the exact Debian `gtk+3.0 3.24.49-3` source package, including
its Debian patch series, not an unpatched upstream tarball. `source-lock.json`
locks all three source archives, both patch input/output files and the patch.
The existing 3.24.52 investigation is retained unchanged in
`migration/tests/gtk-dialog-diagnostics/`; upgrading alone is not this repair.

Run in the pinned Debian 13 build environment:

```
python3 tools/build-gtk-menu-guards.py --archives /path/to/source-archives --work /new/private/work
```

The new work directory must not exist. Source archives may be acquired from the
signed Debian snapshot `20260925T181338Z`; hashes are checked before extraction.
Additional exact build packages used: libcolord-dev 1.4.7-3;
libcups2-dev, libcupsimage2-dev and libcupsimage2t64 2.4.10-3+deb13u2;
sassc 3.6.1+20201027-2+b1 and libsass1 3.6.5+20231221-3+b1.
Their headers, libraries, pkg-config files and Sass executable must be exposed
from a private extracted prefix, never installed over the system. Existing
locked runtime colord/CUPS libraries satisfy their development symlinks.

The build keeps X11, Wayland, Broadway, CUPS/file printing, colord, cloudproviders,
profiler and introspection enabled; documentation/demos/examples and the full GTK
upstream test suite are disabled. Assertions remain enabled. This is not a claim
that Wayland, printers, physical tablets or the whole GTK suite were tested.

Output is a private prefix, not a silent replacement of the dependency lock.
Opt in by prepending `WORK/prefix/lib/x86_64-linux-gnu` to `LD_LIBRARY_PATH`.
Source/license and installed-file hashes are written alongside the build. Do
not pass an overlapping, different GTK root as `--extra-deps` to the runtime
packager: it correctly rejects conflicting bytes. A release dependency root
must explicitly select this source-built GTK and preserve this provenance;
the final aggregate/package/release tasks remain open.

The two guards preserve existing semantics: offscreen grabs remain rejected,
realized ordinary and not-yet-realized menu grabs remain valid, and the ATK
ancestor walk skips objects that are not GtkAccessible rather than casting them.
No log handler is suppressed and no accessibility feature is disabled.
