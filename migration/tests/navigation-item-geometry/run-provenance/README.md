Exact executed orchestration and final collector, retained as text rather than
claiming a portable build bootstrap. Absolute paths identify the isolated local
workspace. They source its private env.sh; no environment or credentials are
embedded here. Run only against a suitably prepared isolated native build and
profile, with the stated pinned dependencies/options and its exclusive lock.
The recovery scripts rerun groups missing bound inputs and do not rewrite prior
receipts. The final collector is the corrected version; earlier omissions are
preserved explicitly in the partial-manifest directories.
