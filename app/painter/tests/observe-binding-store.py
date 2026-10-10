#!/usr/bin/env python3
"""Instrument a test-only copy of both actual BindingStore resolution sites."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()
replacements = {
    '#include "binding-store.hpp"':
        '#include "binding-store.hpp"\n#include "tests/binding-store-observation.hpp"',
    'BindingStore *BindingStore::find (GObject *owner) noexcept\n{':
        'BindingStore *BindingStore::find (GObject *owner) noexcept\n{\n  painter_observe_store_resolution (0);',
    'BindingStore::EntryBase *BindingStore::entry (const void *identity) const noexcept\n{':
        'BindingStore::EntryBase *BindingStore::entry (const void *identity) const noexcept\n{\n  painter_observe_store_resolution (1);',
}
for anchor, replacement in replacements.items():
    if text.count(anchor) != 1:
        raise SystemExit('BindingStore observation anchor changed: ' + anchor)
    text = text.replace(anchor, replacement)
output.write_text(text)
