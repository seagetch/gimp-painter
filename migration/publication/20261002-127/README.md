# Verified migration publication: 127 taskwise commits

The public `seagetch/gimp-painter` branch `gimp-3-0-port` was verified at
`13b795a27a6fdd4178c7152e56c8ecb5f3d9420d` on 2026-10-02.
Its tree `bee0399705e5da64730e3c28614715d1e5f1731a` is byte-identical to the
frozen original local head `996297f09f1b4715b149b2c4fcdfeca1d99fb3d6`.

The user approved Git-data publication with recreated commit IDs. All 127
original task messages, ordering and tree hashes are mapped in `mapping.tsv`
and `mapping.json`. The connector assigns the connected GitHub account and
current server timestamps; these are distinguished from original local author
and committer metadata. Original local IDs are not asserted to be GitHub IDs.

All 3,683 uploaded blob hashes and each task tree were checked. Branch updates
were non-forced, with the remote parent checked before update and the resulting
ref/tree read back. The final commit had no reported GitHub status checks or
workflow runs; publication is not a CI or migration-completion claim.

The working branch was subsequently based on the verified remote history with
the exact same tree. The original history was retained in the local named ref
`archive/local-migration-127-20261002` and a verified recovery Git bundle. The
untracked worktree inventory did not change. Future work therefore descends
from the published history instead of recreating the original 127 commits again.

Checksums:
- mapping.json: `e99fc9531ffcd955f8651c01241b54f3696d183a0b48d65264c2ba90c10ad9a5`
- mapping.tsv: `80b7cf93bc1781b7807847e68286620305c18b6ebd43bbd41bda98d693deb294`
- original-history recovery bundle: `3a6bdb6a7de54b4fcf5849a5160b81463fd82a43eb32e376f3fafc1b8bc27347`

[Verified final commit](https://github.com/seagetch/gimp-painter/commit/13b795a27a6fdd4178c7152e56c8ecb5f3d9420d)
