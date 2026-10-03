#!/usr/bin/env python3
"""Run through the installed python-fu-eval, using only explicit fixture paths."""
import hashlib
import json
import os
from pathlib import Path
import time

oracle = json.loads(Path(os.environ['GIMP_PAINTER_FILTER_ORACLE']).read_text())
destination = Path(os.environ['GIMP_PAINTER_FILTER_RESULT'])
events = Path(os.environ['GIMP_PAINTER_FILTER_EVENTS'])
start = Path(os.environ['GIMP_PAINTER_FILTER_START'])


def emit(kind, **details):
    with events.open('a') as stream:
        stream.write(json.dumps(dict(event=kind, monotonic=time.monotonic(), **details))+'\n')


def first_row(layer):
    rect = Gegl.Rectangle.new(0, 0, oracle['width'], 1)
    return bytes(layer.get_buffer().get(rect, 1.0, "R'G'B'A u8", Gegl.AbyssPolicy.NONE))


def raster_hash(layer):
    checksum = hashlib.sha256()
    buffer = layer.get_buffer()
    for y in range(0, oracle['height'], 32):
        rect = Gegl.Rectangle.new(0, y, oracle['width'], min(32, oracle['height']-y))
        checksum.update(bytes(buffer.get(rect, 1.0, "R'G'B'A u8", Gegl.AbyssPolicy.NONE)))
    return checksum.hexdigest()


def layers(image):
    by_name = {layer.get_name():layer for layer in image.get_layers()}
    assert set(by_name) == {'quit source', 'quit Blinds'}, 'Restored layer identities changed'
    assert image.get_width() == oracle['width'] and image.get_height() == oracle['height']
    return by_name['quit source'], by_name['quit Blinds']


image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                       Gio.File.new_for_path(os.environ['GIMP_PAINTER_FILTER_FIXTURE']))
assert image is not None
source, effect = layers(image)
assert first_row(source) == bytes(oracle['source_pixel']) * oracle['width'], 'Fixture lower input changed'
assert first_row(effect) == bytes(oracle['prior_cache_pixel']) * oracle['width'], 'Fixture cache already changed before explicit update'
emit('READY', image=image.get_id())
deadline = time.monotonic()+30
while not start.exists():
    if time.monotonic() > deadline:
        raise RuntimeError('Installed Filter observer did not acknowledge readiness')
    time.sleep(0.01)
assert source.update(0, 0, oracle['width'], oracle['height'])
emit('INVALIDATED', image=image.get_id())
deadline = time.monotonic()+90
while hashlib.sha256(first_row(effect)).hexdigest() != oracle['expected_first_row_sha256']:
    if time.monotonic() > deadline:
        raise RuntimeError('Installed Filter did not publish the expected old Blinds pixels')
    time.sleep(0.01)
observed = raster_hash(effect)
assert observed == oracle['expected_rgba_sha256'], 'Completed Filter bytes differ from actual old PDB output'
emit('EXACT_OUTPUT', sha256=observed)
saved = destination.with_suffix('.xcf')
assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(saved)), None)
assert image.delete()
reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(saved)))
assert reopened is not None
_, restored = layers(reopened)
assert raster_hash(restored) == observed, 'Saved/reopened Filter pixels changed'
assert reopened.delete()
destination.write_text(json.dumps(dict(status='passed', expected_sha256=observed,
    saved_xcf=str(saved), saved_bytes=saved.stat().st_size,
    saved_sha256=hashlib.sha256(saved.read_bytes()).hexdigest(),
    scope='Explicit lower update, complete genuine-old Blinds pixels, Save and reopen; not a complete editing or GUI test'), indent=2)+'\n')
emit('SAVED_REOPENED', sha256=observed)
print('INSTALLED_FILTER_EXACT_SAVE_REOPEN_OK', flush=True)
