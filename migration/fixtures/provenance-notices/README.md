# Exact pinned source notices

These files preserve original source bytes from gimp-painter commit
`afa43fae3e920210146abed514f136fd49f671b5`. `original/` contains complete original
notice files. `headers/` contains each selected source file's complete leading
block comment, without rewritten names, punctuation or license terms.
`manifest.json` records the source file hash, byte range, exact notice hash and
pinned source URL. The notices describe their original scopes; they are not a
new license grant for fixture data. See `../PROVENANCE.md` for boundaries.

The source-header coverage follows the original files touched by the reviewed
runtime capture overlays, including the stroke brushlib exception, plus the
three complete copied rotation files and the copied enum header. Newly added
harness files do not acquire an invented upstream header. A file without a
leading license comment is recorded explicitly, with repository notice context.

The root GPL text and libgimp LGPL text are copied exactly. Brush-local files
are complete source declarations; they are not substituted with newly fetched
license texts. Preserve this directory alongside extracted comparison packages
that reference the shared provenance register. This is a bounded notice bundle,
not the complete corresponding source or a repository-wide rights clearance.
