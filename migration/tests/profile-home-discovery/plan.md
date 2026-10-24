# Known legacy Painter home discovery — planned independent task

Pinned configure.ac:2131 declares `.gimp-painter-gimp_user_version`; pinned2.8 profile is therefore `~/.gimp-painter-2.8`. Current installer tries XDG/current-derived versions, platform migrations, then `.gimp-VERSION`, never the Painter-specific home directory. Existing first-run test manually creates sibling2.8/3.0 under a temporary root and cannot prove automatic home discovery.

Bounded proposed product change: only if all existing discovery fails and home exists, accept the exact existing directory `.gimp-painter-2.8` as old_major2/old_minor8. Do not reorder standard/XDG precedence, scan hypothetical Painter versions, follow arbitrary environment paths, or infer Painter semantics solely from directory name. Existing content-based origin detection, source preservation and no-overwrite migration remain authoritative. Platform path lookup uses GLib; Linux proof is not cross-platform claim.

Causal test: fresh subprocess with HOME/XDG/GIMP3_DIRECTORY all private; only hiddenPainter2.8 exists; invoke real gimp_user_install_new and observe selected fields through a test-only helper appended to its exact native translation unit, not a copied detection implementation. Baseline must report no source; candidate selects exact path/version. No UI claim.

Matrix: missing, regular file instead of directory, lone Painter folder, existing standard old-home preference, existing XDG preference, spaces/Unicode HOME, unrelated filenames untouched, explicit modern source precedence. Use exact pre/post filesystem hashes; only test creates fixtures. Separate actual migration fixture should invoke public run with a native test Gimp, source file bytes preserved and destination edit survives retry; existing nine native profile cases repeated when build slot available. Helpers are not product exports. Preserve exact original source/legacy/configure hashes and all9 paused providers.

Do not call task done on detection alone if actual first-run migration/ownership proof missing. Original30.010/011 dependencies and wider profile/resource semantics remain open.
