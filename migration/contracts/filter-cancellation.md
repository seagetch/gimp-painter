# Explicit Filter generation cancellation

The owner-thread `gimp_filter_layer_cancel()` entry and typed
`FilterLayerRef::cancel()` stop the current generation, discard staged import,
retain serialized definition and completed drawable pixels, and keep independent
worker cleanup scheduled. A later legitimate edit can retry. Cancellation is
visible as a failed generation; it does not certify stale pixels as current.

The staged buffer is detached before any callback. State notification may close
the owner or install another definition; the outer operation does not overwrite
that replacement afterward. No worker join or PDB execution occurs here.

The fresh restored Linux build ran two native tests, covering cancellation in
WAITING, PREPARING, RUNNING and IMPORTING, no idle retry, retained raw definition
and cache, subsequent edit convergence, and notification-driven replacement or
owner closure. Both passed with unchanged selected source hashes.
`filter-cancel-reconstructed-normal.json` and the associated immutable source
archive preserve this bounded evidence. The test fixtures emitted existing
plug-in dummy-file and writable-data-folder diagnostics; this is not a clean
full-application result.

This source was reimplemented after runtime replacement; it is not a recovery of
the lost unpublished original commit or its test logs. Both cases subsequently passed focused ASan/UBSan with381 immutable source
inputs in `xcf-reconstructed-sealed-verification.json`. Whole current-source
Filter acceptance remains pending. Foundation rows06.028
and07.012 retain their historical reports but are now partial until refreshed
against the changed Filter source. Their parent dependencies remain unchanged.
