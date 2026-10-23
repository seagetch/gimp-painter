# Stroke-start snapshot acceptance evidence

Actual native GimpPaintCore undo snapshot→Surface handoff content isolation is
tested in the new app fixture, without replacing historical tests. The existing
standalone snapshot-isolation two groups are retained unchanged and rerun
normally and with focused ASan/UBSan/float-cast-overflow. See the contract for
precisions, offsets, mutation timing, comparisons and explicit scope limits.

Native tests use authenticated1080×1920 Xvfb and unchanged per-target budgets.
This is pixel/buffer acceptance, not additional UI controls or a new screenshot.
The preceding09.007 UI checkpoint contains actual portrait/oncanvas images.

Three actual production mutations are expected red: native snapshot alias,
too-late supplied snapshot capture, and default snapshot alias. The first two
mutate shared native sources only under the caller's build lock and restore/
rebuild them in finally blocks. The third compiles a private copy of the actual
Surface source, never changing shared sources. No replacement model is used.

Selected Meson fields exclude raw environment maps. Final source hashes match
the restored detached native tree. Dependencies remain uninstrumented and LSan
is off in the standalone sanitizer; no full-app sanitizer claim is made.
