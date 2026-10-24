# Exact committed d0dd12 source-only checkpoint

Commit d0dd12c9431ef7272981cf369382d3032c552fa1, Git-only required input set.
Every selected app/library/PDB/tools/migration/tasks/Meson file was verified
against this commit's Git blob and SHA256, not inferred from a dirty worktree.

- Adapter gate PASS: 2,535 sources, 720 reviewed data sites, four dormant findings.
- Persistence gate PASS: 1,379 source-checked records, 873 candidates, 16 routes.
- 16,840 inputs unchanged before/after and after relocation into /tmp.
- No native application rerun, source edits, held diagnostics or negative preview
  controls. Exact55bf native checkpoint remains separate and earlier.

Initial adapter run rejected symlinked application sources. This setup error is
preserved; the checker was not weakened. Cross-filesystem hardlink creation then
failed before any hardlink existed. Final application sources are materialized
bytes. Non-app input links were independently Git-blob/SHA256 verified, checked
again after both gates; no claim that later mutable link targets remain frozen.
The physical snapshot is /tmp/current-source-gates-d0dd12c943 (shared alias kept).

The manifest records exact Git provenance and before/after hashes; verify.py
rechecks selected tree membership and every manifest SHA256 against Git blobs.
This is source-contract acceptance only, not completion of original WBS tasks.
