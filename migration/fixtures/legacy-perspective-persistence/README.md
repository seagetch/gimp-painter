# Old guide persistence boundary (source-only)

The complete old app/xcf tree plus image duplicate, image owner/private/model,
and Perspective tool sources are archived from fixed revision
`afa43fae3e920210146abed514f136fd49f671b5`. The checker reconstructs the pinned
Git XCF tree object and checks six additional fixed blob IDs (21 files total).
The entire old XCF tree and image duplicate have no guide references. The model
is an ordinary private image-owned pointer, not an image serialized property.
This is source-bound evidence, not a run of an old Save/Open binary.

Current native acceptance separately writes/reopens XCF, verifies absent guide,
compares exact writer bytes with/without the guide under otherwise unchanged
state, and confirms image duplication omits it. No pixel/property bytes are
masked to obtain equality. The guide is session-only; current Undo uses
GIMP_DIRTY_NONE, preserving clean XCF/export state in a valid metadata fixture.
