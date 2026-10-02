#!/usr/bin/env python3
# SPDX-License-Identifier: ISC
# See COPYING for the retained brushlib copyright and permission notice.
"""Generate pinned painter brush metadata, without depending on the old tree."""

import argparse
import ast
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent
NUMBER = re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z')
IDENTIFIER = re.compile(r'[A-Z][A-Z0-9_]*\Z')
NAME = re.compile(r'[a-z][a-z0-9_]*\Z')
ENUM_HEADER = 'mypaintbrush-enum-settings.h'
DATA_HEADER = 'mypaintbrush-settings-data.h'


def constant_value(expression, values):
    """Evaluate only integer literals, previous names, addition and subtraction."""
    def visit(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name) and node.id in values:
            return values[node.id]
        if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Sub)):
            left, right = visit(node.left), visit(node.right)
            return left + right if isinstance(node.op, ast.Add) else left - right
        raise ValueError('unsupported constant expression: ' + expression)
    return visit(ast.parse(expression, mode='eval').body)


def number(value, sentinel=False):
    if sentinel and value == 'None':
        return 'FLT_MAX'
    if not isinstance(value, str) or not NUMBER.fullmatch(value):
        raise ValueError('invalid numeric literal: ' + repr(value))
    return value


def validate(data):
    if data['schema_version'] != 1:
        raise ValueError('unsupported manifest schema')
    values = {}
    for entry in data['constants']:
        name = entry['name']
        if not IDENTIFIER.fullmatch(name) or name in values:
            raise ValueError('duplicate or invalid constant: ' + name)
        value = constant_value(entry['expression'], values)
        if type(entry['value']) is not int or value != entry['value']:
            raise ValueError('constant expression/index mismatch: ' + name)
        if type(entry['spacing']) is not int or not 1 <= entry['spacing'] <= 32:
            raise ValueError('invalid formatting for ' + name)
        if type(entry['blank_before']) is not bool:
            raise ValueError('invalid separator for ' + name)
        values[name] = value
    categories = [
        ('inputs', 'INPUT_COUNT', 0, 'gfloat'),
        ('settings', 'BRUSH_MAPPING_COUNT', values['BRUSH_MAPPING_BASE'], 'gfloat'),
        ('switches', 'BRUSH_BOOL_COUNT', values['BRUSH_BOOL_BASE'], 'gboolean'),
        ('texts', 'BRUSH_TEXT_COUNT', values['BRUSH_TEXT_BASE'], 'gchar*'),
        ('states', 'STATE_COUNT', 0, 'float'),
    ]
    all_setting_names = set()
    for category, count, start, storage_type in categories:
        entries = data[category]
        if len(entries) != values[count]:
            raise ValueError(category + ' count differs from constants')
        names = set()
        for offset, entry in enumerate(entries):
            name = entry['name']
            if not NAME.fullmatch(name) or name in names:
                raise ValueError(category + ' duplicate or invalid name: ' + name)
            names.add(name)
            if category in ('settings', 'switches', 'texts'):
                if name in all_setting_names:
                    raise ValueError('duplicate cross-category setting: ' + name)
                all_setting_names.add(name)
            if (type(entry['index']) is not int or entry['index'] != start + offset
                    or values[entry['symbol']] != entry['index']):
                raise ValueError('noncontiguous or mismatched index: ' + name)
            if entry['storage_type'] != storage_type:
                raise ValueError('unexpected storage type: ' + name)
            if category == 'inputs':
                for field in ('hard_minimum', 'soft_minimum', 'normal', 'soft_maximum', 'hard_maximum'):
                    number(entry[field], sentinel=True)
            elif category == 'settings':
                for field in ('minimum', 'default_value', 'maximum'):
                    number(entry[field])
                if type(entry['constant']) is not bool or entry['property_type'] != 'G_TYPE_DOUBLE':
                    raise ValueError('unexpected property metadata: ' + name)
            elif category == 'switches' and type(entry['default_value']) is not bool:
                raise ValueError('nonboolean switch default: ' + name)
            elif category == 'texts' and entry['default_value'] is not None and not isinstance(entry['default_value'], str):
                raise ValueError('invalid text default: ' + name)
            elif category == 'states':
                number(entry['constructor_default'])
                if entry['minimum'] is not None or entry['maximum'] is not None:
                    raise ValueError('legacy state metadata declares no range: ' + name)
    if not (values['BRUSH_MAPPING_BASE'] == 0
            and values['BRUSH_BOOL_BASE'] == values['BRUSH_MAPPING_END']
            and values['BRUSH_TEXT_BASE'] == values['BRUSH_BOOL_END']
            and values['BRUSH_SETTINGS_COUNT'] == values['BRUSH_TEXT_END']):
        raise ValueError('invalid setting index-space boundaries')
    return data


def c_string(value):
    # JSON ASCII string escapes used here are also valid C string escapes.
    if value is None:
        return 'NULL'
    if not isinstance(value, str) or '\0' in value:
        raise ValueError('metadata strings must be NUL-free strings')
    return json.dumps(value, ensure_ascii=True)


def enum_header(data):
    # Preserve the original header byte for byte, including historical layout.
    result = '// DO NOT EDIT - autogenerated by generate.py\n\n'
    result += '#ifndef __MYPAINTBRUSH_ENUM_SETTINGS_H__\n#define __MYPAINTBRUSH_ENUM_SETTINGS_H__\n'
    for entry in data['constants']:
        if entry['blank_before']:
            result += '\n'
        result += '#define ' + entry['name'] + ' ' * entry['spacing'] + entry['expression'] + '\n'
    return result + '\n#endif\n'


def data_header(data):
    result = data['license'] + '\n\n/* DO NOT EDIT - generated from brush-settings.json by generate.py. */\n'
    result += '''#ifndef GIMP_PAINTER_MYPAINT_SETTINGS_DATA_H
#define GIMP_PAINTER_MYPAINT_SETTINGS_DATA_H

#include <float.h>
#include <stddef.h>
#include "mypaintbrush-enum-settings.h"

/* Read-only metadata, usable from C11 and C++14; no GObject or engine ownership. */
typedef struct {
  const char *name;
  int index;
  float hard_minimum, soft_minimum, normal, soft_maximum, hard_maximum;
  const char *displayed_name, *tooltip;
} GimpPainterMyPaintInput;
typedef struct {
  const char *internal_name;
  int index;
  const char *displayed_name;
  int constant;
  float minimum, default_value, maximum;
  const char *tooltip;
} GimpPainterMyPaintSetting;
typedef struct {
  const char *internal_name;
  int index;
  const char *displayed_name;
  int default_value;
} GimpPainterMyPaintSwitch;
typedef struct {
  const char *internal_name;
  int index;
  const char *displayed_name, *default_value;
} GimpPainterMyPaintText;
typedef struct {
  const char *name;
  int index;
  float constructor_default;
} GimpPainterMyPaintState;

'''
    def table(ctype, name, count, rows):
        return ('static const ' + ctype + ' ' + name + '[' + count + '] = {\n'
                + ''.join('  {' + ', '.join(row) + '},\n' for row in rows) + '};\n\n')
    result += table('GimpPainterMyPaintInput', 'painter_mypaint_inputs', 'INPUT_COUNT', [
        [c_string(e['name']), e['symbol']] + [number(e[f], True) for f in
         ('hard_minimum', 'soft_minimum', 'normal', 'soft_maximum', 'hard_maximum')]
        + [c_string(e['displayed_name']), c_string(e['tooltip'])] for e in data['inputs']])
    result += table('GimpPainterMyPaintSetting', 'painter_mypaint_settings', 'BRUSH_MAPPING_COUNT', [
        [c_string(e['name']), e['symbol'], c_string(e['displayed_name']), str(int(e['constant']))]
        + [number(e[f]) for f in ('minimum', 'default_value', 'maximum')]
        + [c_string(e['tooltip'])] for e in data['settings']])
    result += table('GimpPainterMyPaintSwitch', 'painter_mypaint_switches', 'BRUSH_BOOL_COUNT', [
        [c_string(e['name']), e['symbol'], c_string(e['displayed_name']), str(int(e['default_value']))]
        for e in data['switches']])
    result += table('GimpPainterMyPaintText', 'painter_mypaint_texts', 'BRUSH_TEXT_COUNT', [
        [c_string(e['name']), e['symbol'], c_string(e['displayed_name']), c_string(e['default_value'])]
        for e in data['texts']])
    result += table('GimpPainterMyPaintState', 'painter_mypaint_states', 'STATE_COUNT', [
        [c_string(e['name']), e['symbol'], number(e['constructor_default'])] for e in data['states']])
    return result + '#endif\n'


def generate(manifest):
    data = validate(json.loads(Path(manifest).read_text(encoding='utf-8')))
    return {ENUM_HEADER: enum_header(data).encode('utf-8'), DATA_HEADER: data_header(data).encode('utf-8')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'brush-settings.json')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--check', action='store_true', help='Compare existing outputs; never write')
    args = parser.parse_args()
    try:
        outputs = generate(args.manifest)
        if not args.check:
            args.output_dir.mkdir(parents=True, exist_ok=True)
        for name, content in outputs.items():
            path = args.output_dir / name
            if path.is_file() and path.read_bytes() == content:
                continue
            if args.check:
                raise ValueError('missing or stale generated file: ' + str(path))
            path.write_bytes(content)
    except (ValueError, KeyError, OSError, SyntaxError) as error:
        print('brush settings generation failed: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
