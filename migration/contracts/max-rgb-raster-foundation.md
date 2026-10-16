# Max RGB raw raster foundation

Task: `16.003/max-rgb-raster-foundation`.

The explicit `filter_max_rgb_raw_raster` entry point implements the legacy
Max RGB callback over private, independent, tightly packed RGBA8 storage.
Positive signed mode keeps maximum channels; zero and negative modes keep
minimum channels. Ties survive. Alpha is copied, and hidden RGB is transformed
at alpha zero. Existing byte and normalized-real point routes retain their
previous merged-output hidden-color behavior. Neither route is repurposed.

Operation-tagged point-shadow overloads reuse the captured start ROI, signed
INT32 area preflight, bounded 16 KiB crop transfers, full zeroed shadow, distinct
outside-selection `no_merge`, and transactional vector publication. Existing
Threshold signatures forward without changing their behavior. Invalid tags
are rejected before I/O, allocation or cancellation shortcuts. The new raw
kernel uses 32 KiB bounded transfers; errors propagate and cancellation never
publishes partial work. Owner-side completed-generation swap remains required.

Fresh C++14 normal and focused ASan/UBSan runs pass with unchanged source and
executable seals. Coverage includes signed extrema, ties, all alpha values,
276 generated vector/memory/spill ROI cases, direct raw I/O cancellation/errors,
400 Max RGB shadow cancellation/error cases, and existing 1,222 generated
Threshold cases plus 200 Threshold cancellation/error cases. Sixteen archived
actual old-GIMP final Max RGB outputs match only after explicit full-coverage
REPLACE_INTEN merge. They are not represented as raw callback or fresh owner
captures. Twenty archived Threshold outputs remain matched. The original
Max RGB, Threshold and region-iterator source hashes are verified at afa43fae.

The first sanitizer attempt rejected a concurrent final test edit via its
source seals. It is retained as a rejected attempt; fresh sealed normal and
sanitizer runs supersede it. See `migration/tests/max-rgb-raster-foundation.json`.

This bounded prerequisite does not connect Max RGB owner context, RGB-only
admission, XCF loading, scheduler lifecycle or new old-runtime captures. General
point precision, huge I/O, LeakSanitizer, installed UI, other operating systems,
and full-aggregate/whole-port acceptance remain open. The recovered Sobel
native build is a separate baseline and does not certify these new changes.
