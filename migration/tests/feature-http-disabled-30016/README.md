# Full HTTP-disabled console startup and original feature-duty audit

The existing isolated clean source checkpoint, with the committed feature/preset
owned overlays, was configured in a separate build directory using
`painter-http=disabled`, O2, debug=false and assertions enabled. All707 required
build steps for the actual console executable completed. It ran a real new-instance
no-data/no-fonts initialize→restore→exit→dispose→finalize sequence with exit0,
not merely `--version`.

The runtime deliberately had HTTP_ENABLE=1 and invalid HTTP_PORT=0, without a token.
The disabled executable ignored these controls: no optional HTTP startup error,
no HTTP provider symbols, no HTTP library targets, no direct libsoup dependency.
The original features_entry_point symbol is present. Native enabled/opt-in and
callback lifetime tests remain in the immutable features-entry-08018 predecessor.

This is not a full aggregate branch HEAD or resource-installation claim. The
no-data console profile reported nonfatal first-profile tag-cache/template/welcome
diagnostics, preserved verbatim. It is not an HTTP-disabled GUI runtime test and
adds no sanitizer claim. Previously bounded GUI/Xvfb tests are not relabeled as
this console configuration.

`original13-duty-audit.json` retains every field of all13 original immutable
30.016/feature-entry-point hunk assignments, the original acceptance text and
both dependencies. Image-origin invalidation, old Image/MyPaint typed identities,
consuming public paint registration, and old dynamic feature-runtime differences
remain explicit open owners. The original task is NOT closed by this build.
