# Scoped mutex release — original 06.022

MutexGuard already locks its referenced native GMutex at construction and
unlocks at destruction. It cannot be copied or moved and its destructor is
nothrow. The caller owns the mutex and keeps it alive until the guard is gone;
this wrapper does not transfer a lock to another thread.

The existing mutex test now covers normal scope completion, early return and
exception unwinding. For each path, a separate native thread tries the mutex
while the guard is alive and must fail. After the guard exits, another try from
that thread must succeed and is unlocked by the same thread. Each observer is
joined before the next step. The same mutex is reused across all three paths,
then cleared. No recursive same-thread locking or timing-based sleep is used.
Normal and rebuilt common-component ASan/UBSan suites each pass 59 cases; the
existing case was strengthened rather than duplicated. All 11 common C/C++ units
are instrumented, system GLib is not. LSan remains unavailable under the verified
ptrace limitation; vptr is excluded for native no-RTTI compilation.

The pinned source separates two responsibilities. scopeguard.hpp:17–21 supplies
scope-exit destructor cleanup. glib-cxx-utils.hpp:470–492 supplies synchronized,
which locks a GMutex and instantiates that guard with g_mutex_unlock. The existing
scopeguard obligation legacy-2e934e579dea6c8eafd8 remains valid and unchanged in
identity. Its real synchronized caller was missing a direct 06.022 assignment.

The cpp-values routing rule now adds that one task to the one affected added-file
hunk 01.002/000047. Exact source blob and +prefixed hunk bytes are verified. The
normal granularity specification generates the new stable work ID
legacy-d9601d797e1afecd978e as TODO. All 22,942 prior work rows remain unchanged at
that stage; no row is removed or reassigned, and assignment correction adds zero
DONE. Only after the scoped-lock tests pass do the existing and added obligations
become DONE. Total source obligations become 22,943, with 535 DONE. Original WBS
completion increases by one, independently of that source-row addition.

reproduce-mutex-routing.py reproduces the bounded correction from public parent
d7008b23c6fe7cb28efb83648dc3fe351d4d3ae5 using the pinned added-file bytes and the
existing route/specification/TSV functions. All consumers keep their existing
formats. It checks unrelated inventory bytes and all existing source identities
and execution fields. Missing/duplicate/unknown work IDs, changed source or task,
and DONE without evidence are rejected in six negative controls. Its write guard
rejects overwriting the later completed rows before changing any inventory file.
The full historical source/base Git trees are not available locally, so this
bounded re-generation is not described as a full historical-generator pass.
Historical source digests and earlier checkpoint reports are not rewritten.

The old exit() and unlocked(F) methods provide additional explicit and temporary
release operations. They are outside this original task's lexical-scope exit
condition. By source inspection, the old unlocked(F) skips reacquisition if F
throws; no fresh legacy runtime failure is claimed. Feature call-site migration
remains separate. Searches of the current tree and supplied excerpts did not
find calls, but historical repository-wide non-use is unverified without the
complete pinned tree. This common gate does not add unused APIs or claim all
mutex-using features/platforms are complete.
