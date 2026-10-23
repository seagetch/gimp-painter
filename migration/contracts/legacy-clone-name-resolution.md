# Legacy CloneLayer name resolution (10.010)

This records **source-derived expectations**, not a claim of native old-runtime
execution or complete Clone parity. Baseline: public `seagetch/gimp-painter`
commit `afa43fae3e920210146abed514f136fd49f671b5`. Exact source excerpts and full
source SHA-256 are in `../tests/legacy-clone-name-resolution/source-evidence.json`.

## Search and state expectations

Container index order is the order returned by `gimp_container_get_child_by_index`,
not an assumed visual top/bottom orientation. For each sibling, compare its own
name with `strcmp`, recurse into children, then visit the next sibling. Search
starts at the owning image's root layer container, regardless of clone nesting.

| Case | Source-derived expectation |
| --- | --- |
| Two root siblings both named `match` | First matching container index wins. |
| First root group has child `match`; later root is `match` | Earlier group's descendant wins. |
| Group and its own child both named `match` | Group wins; its name is tested before recursion. |
| Two nested groups contain `match` | First match in recursive pre-order wins, not shallowest or nearest-to-clone. |
| Clone is nested but matching source is another root branch | Whole owning-image tree is searched. |
| `Match` versus requested `match` | No match; exact byte comparison, no case folding, path syntax, normalization, or fuzzy match. |
| No match and existing source is NULL | Getter returns NULL; pending name remains for retry. |
| No match but existing source pointer is set | Getter returns that existing pointer; pending name remains. Unresolved does **not** necessarily mean NULL. |
| Missing name appears before a later getter | Later getter can resolve it. Successful match clears the pending name and calls `set_source`. |
| Direct pointer assignment while a name is pending | `set_source` does not clear the pending name. Update callbacks can cause lazy resolution; do not assume the assigned pointer necessarily survives the whole call. |
| Name request replaced by another name | Setter stores a duplicate of the new name; subsequent search uses that request. |
| Requested name is the clone's own name | Search does not exclude self. This is a search result, **not** evidence that self-reference rendering/update is safe. |
| Child container equals its immediate parent container | That edge is skipped. No general visited-set/cycle bound exists in the old finder. |

The old loader sets a pending name for PROP33, restores the tree, and performs a
recursive post-pass invoking clone getters. Ordering/hierarchy therefore matter.
A failed lookup is not replaced with a similarly named layer. The original
writer passes its nullable source pointer to `gimp_object_get_name` without a
caller-side guard. That getter checks `GIMP_IS_OBJECT` and returns NULL with a
critical diagnostic when validation fails; this source is **not** evidence of
an unconditional NULL dereference or a particular runtime save outcome. Such
unresolved-save behavior is not a new-port requirement.

## Port correspondence and limits

Current `app/core/gimpclonelayer.cpp::find_name` retains index-order pre-order;
`CloneImpl::get_source` retains lazy lookup and existing-source fallback, with
owner lifetime and visited-container guards. Existing native test definitions
`deferred_name` and `recursive_first_match` in `app/tests/test-gimp-clone-layer.c`
cover pending-name fallback/retry and descendant/group first-match examples.
This task checks their presence and source correspondence, **not** claims a new
runtime test execution. Its artifact verifier is not an independent old parser
or layer-tree oracle.

Self-reference/cycles, mutation during lookup, rename/delete/Undo, unresolved
save, full XCF reopening and cross-version rendering remain governed by their
own tasks and native evidence. Unsafe legacy behavior is described, not blindly
reintroduced. No production implementation is changed for this criterion.
