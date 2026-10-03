#!/usr/bin/env python3
"""Run through the installed python-fu-eval, using only explicit fixture paths."""
import hashlib
import json
import os
from pathlib import Path
import time


def filter_capsule(path):
    """Read this smoke's standard-XCF Filter capsule, never infer live state."""
    data = path.read_bytes()
    assert data[:9] == b'gimp xcf ' and data[9:10] == b'v' and data[13:14] == b'\0'
    version = int(data[10:13])
    assert 11 <= version <= 23, 'Unexpected smoke XCF version'

    def integer(at, width=4):
        assert 0 <= at <= len(data)-width, 'Truncated smoke XCF integer'
        return int.from_bytes(data[at:at+width], 'big')

    def string(at):
        length = integer(at)
        at += 4
        assert 0 < length <= 1048576 and at+length <= len(data), 'Invalid smoke XCF string'
        assert data[at+length-1] == 0, 'Unterminated smoke XCF string'
        return data[at:at+length-1], at+length

    def properties(at):
        result = []
        for _ in range(1024):
            tag, length = integer(at), integer(at+4)
            at += 8
            assert at+length <= len(data), 'Truncated smoke XCF property'
            if tag == 0:
                assert length == 0
                return result, at
            result.append((tag, at, length))
            at += length
        raise AssertionError('Too many smoke XCF properties')

    _, at = properties(30)  # 14-byte header, dimensions/type and precision
    matches = []
    for _ in range(16):
        offset = integer(at, 8)
        at += 8
        if offset == 0:
            break
        name, layer_at = string(offset+12)
        props, _ = properties(layer_at)
        if name != b'quit Blinds':
            continue
        for tag, cursor, size in props:
            if tag != 21:  # standard PROP_PARASITES
                continue
            end = cursor+size
            while cursor < end:
                parasite_name, cursor = string(cursor)
                length = integer(cursor+4)  # flags, then payload length
                cursor += 8
                assert cursor+length <= end, 'Truncated smoke XCF parasite'
                if parasite_name == b'gimp-painter-item':
                    matches.append(data[cursor:cursor+length])
                cursor += length
    else:
        raise AssertionError('Too many smoke XCF layers')
    assert len(matches) == 1, 'Expected one saved Filter capsule'
    capsule = matches[0]
    assert capsule[:12] == b'GPXCF\0\0\0\1\0\0\0', 'Unexpected Painter capsule version'
    return capsule[12:]


def filter_record(path):
    from gi.repository import GLib
    value = GLib.Variant.new_from_bytes(GLib.VariantType.new('a{sv}'),
                                       GLib.Bytes.new(filter_capsule(path)), False)
    assert value.is_normal_form(), 'Malformed saved Filter capsule'
    record = value.unpack()
    assert record['version'] == 1 and record['kind'] == 'filter'
    return record


def definition(record):
    return {key:record[key] for key in
            ('procedure', 'has-definition', 'definition', 'has-arguments', 'arguments')}


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


def main():
    global oracle, destination, events, start
    oracle = json.loads(Path(os.environ['GIMP_PAINTER_FILTER_ORACLE']).read_text())
    destination = Path(os.environ['GIMP_PAINTER_FILTER_RESULT'])
    events = Path(os.environ['GIMP_PAINTER_FILTER_EVENTS'])
    start = Path(os.environ['GIMP_PAINTER_FILTER_START'])
    missing = os.environ.get('GIMP_PAINTER_FILTER_MISSING_RUNTIME')
    assert missing in (None, 'helper', 'plugin')
    image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                       Gio.File.new_for_path(os.environ['GIMP_PAINTER_FILTER_FIXTURE']))
    assert image is not None
    source, effect = layers(image)
    assert first_row(source) == bytes(oracle['source_pixel']) * oracle['width'], 'Fixture lower input changed'
    assert first_row(effect) == bytes(oracle['prior_cache_pixel']) * oracle['width'], 'Fixture cache already changed before explicit update'
    saved = destination.with_suffix('.xcf')
    if missing:
        initial = destination.with_name('initial-cache.xcf')
        assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(initial)), None)
        before = filter_record(initial)
        assert before['saved-state'] == 0 and before['cache-complete']
        prior_hash = raster_hash(effect)
        prior = hashlib.sha256()
        for _ in range(oracle['height']):
            prior.update(bytes(oracle['prior_cache_pixel']) * oracle['width'])
        assert prior_hash == prior.hexdigest(), 'Fixture full prior cache changed'
    emit('READY', image=image.get_id())
    deadline = time.monotonic()+30
    while not start.exists():
        if time.monotonic() > deadline:
            raise RuntimeError('Installed Filter observer did not acknowledge readiness')
        time.sleep(0.01)
    assert source.update(0, 0, oracle['width'], oracle['height'])
    emit('INVALIDATED', image=image.get_id())
    deadline = time.monotonic()+90
    if missing:
        # Saved-state is an owner-thread snapshot of the real host scheduler.
        # A timeout or unchanged pixels alone can never satisfy this negative.
        while True:
            if time.monotonic() > deadline:
                raise RuntimeError('Missing runtime did not reach terminal Filter failure')
            if Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(saved)), None):
                failed = filter_record(saved)
                if failed['saved-state'] == 6:  # GIMP_FILTER_LAYER_FAILED
                    break
            time.sleep(0.1)
        assert definition(failed) == definition(before), 'Failure changed Filter definition/arguments'
        assert failed['cache-complete'] and failed['cache-generation'] < failed['generation']
        observed = raster_hash(effect)
        assert observed == prior_hash, 'Missing runtime changed the previous committed cache'
        emit('EXPECTED_FAILURE', missing_runtime=missing, saved_state=failed['saved-state'],
             cache_sha256=observed, definition_preserved=True)
    else:
        while hashlib.sha256(first_row(effect)).hexdigest() != oracle['expected_first_row_sha256']:
            if time.monotonic() > deadline:
                raise RuntimeError('Installed Filter did not publish the expected old Blinds pixels')
            time.sleep(0.01)
        observed = raster_hash(effect)
        assert observed == oracle['expected_rgba_sha256'], 'Completed Filter bytes differ from actual old PDB output'
        emit('EXACT_OUTPUT', sha256=observed)
        assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(saved)), None)
    assert image.delete()
    reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(saved)))
    assert reopened is not None
    _, restored = layers(reopened)
    assert raster_hash(restored) == observed, 'Saved/reopened Filter pixels changed'
    assert reopened.delete()
    result = dict(status='passed', expected_sha256=observed,
        saved_xcf=str(saved), saved_bytes=saved.stat().st_size,
        saved_sha256=hashlib.sha256(saved.read_bytes()).hexdigest(),
        scope='Explicit lower update, complete genuine-old Blinds pixels, Save and reopen; not a complete editing or GUI test')
    if missing:
        result.update(missing_runtime=missing, saved_state=failed['saved-state'],
                      definition_preserved=True, cache_preserved=True,
                      generation=failed['generation'], cache_generation=failed['cache-generation'],
                      scope='Missing installed executable reaches terminal Filter failure, retains definition/arguments and full cache, and Saves/reopens')
    destination.write_text(json.dumps(result, indent=2)+'\n')
    emit('SAVED_REOPENED', sha256=observed)
    print('INSTALLED_FILTER_MISSING_RUNTIME_SAVE_REOPEN_OK' if missing else
          'INSTALLED_FILTER_EXACT_SAVE_REOPEN_OK', flush=True)


if __name__ == '__main__' or 'Gimp' in globals():
    main()
