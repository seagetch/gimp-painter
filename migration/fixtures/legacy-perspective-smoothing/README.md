# Old active Gaussian smoothing source

The entire pinned `app/paint/gimppaintcore.c` blob and unchanged complete
`gimp_paint_core_smooth_coords` body are retained. The adapter replaces only the
core storage type with its GArray history field; native options/coordinates are
passed unchanged. It does not call the current smoothing implementation.

This is source-only arithmetic, not an old application or physical pen replay.
The historical circular-queue smoothing declaration has no implementation/caller
in final afa43fae; the active generic paint-tool path uses this Gaussian body.
