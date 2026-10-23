#!/usr/bin/env python3
"""Definition/source consistency only; does not run scheduler or application."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
checks={
 'app/core/gimpfilterlayer.cpp':['snapshot->cache_generation > snapshot->generation || snapshot->generation > G_MAXINT64','impl.scheduler.restore_cache','*snapshot = {1, saved.generation, saved.cache_generation, saved.cache_complete'],
 'app/painter/filter-scheduler.cpp':['cancel ();\n  advance_generation ();\n  cache_complete_ = saved.cache_complete;','dirty_ = !cache_complete_ || saved.generation != saved.cache_generation;','cache_generation_ = dirty_ ? 0 : generation_;','try { commit (token); }','cache_complete_ = false;'],
 'app/xcf/painter-xcf-preserve.cpp':['now.generation != before.generation','now.cache_generation != before.cache_generation','now.cache_complete != before.cache_complete','now.state != before.state','gegl_buffer_dup (gimp_drawable_get_buffer','"generation", g_variant_new_uint64','"cache-generation", g_variant_new_uint64'],
 'app/xcf/xcf.c':['xcf_painter_save_unchanged (prepared)'],
 'app/tests/test-painter-xcf-active.inc':['filter_running_save_close_reopen','filter_importing_save_close_reopen'],
 'app/painter/tests/test-filter-scheduler.cpp':['saved_cache_generations','restored_cache_rejects_colliding_old_job'],
}
for name, needles in checks.items():
 source=(ROOT/name).read_text()
 for needle in needles:assert needle in source,(name,needle)
assert (ROOT/'app/xcf/xcf.c').read_text().count('xcf_painter_save_unchanged (prepared)')==2
paths=list(checks)+['migration/contracts/xcf-generation-definition.md']
print(json.dumps({'task':'10.013','result':'PASS source definition consistency only','files_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2))
