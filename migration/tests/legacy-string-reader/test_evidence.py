#!/usr/bin/env python3
"""Validate retained capture integrity; does not execute the legacy application."""
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parent


def observations(text):
    found = []
    current = None
    for line in text.splitlines():
        match = re.search(r'OBSERVE case=(\w+) positive=(\d) ok=(\d) argc=(\d+) cp=(\d+) ftell=(\d+) bytes=(\d+)', line)
        if match:
            name, positive, ok, count, cursor, position, size = match.groups()
            current = dict(case=name, positive=int(positive), ok=int(ok), argc=int(count),
                           cp=int(cursor), ftell=int(position), bytes=int(size), arguments=[])
            found.append(current)
        elif line.startswith('ARG ') and current is not None:
            arg = re.match(r'ARG index=(\d+) type=(\w+)(.*)', line)
            index, kind, rest = arg.groups()
            value = dict(index=int(index), type=kind)
            if kind == 'gchararray':
                match = re.search(r'string_null=(\d) string_base64=([A-Za-z0-9+/=-]*)', rest)
                null, encoded = match.groups()
                value['value'] = None if null == '1' else base64.b64decode(encoded).decode('utf-8')
            elif kind == 'gint':
                value['value'] = int(re.search(r'int=(-?\d+)', rest).group(1))
            current['arguments'].append(value)
    return found


class Evidence(unittest.TestCase):
    def test_locked_files(self):
        report = json.loads((ROOT / 'report.json').read_text())
        for path, expected in report['evidence_sha256'].items():
            self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), expected, path)

    def test_only_tag_is_changed(self):
        expected_values = {'ascii': b'reader string', 'empty': b'', 'utf8': '日本語'.encode(), 'null': None}
        for name, value in expected_values.items():
            old = (ROOT / 'fixtures' / (name + '-writer-tag7.bin')).read_bytes()
            controlled = (ROOT / 'fixtures' / (name + '-controlled-tag8.bin')).read_bytes()
            self.assertEqual(old[:4], struct.pack('>I', 7))
            self.assertEqual(controlled, old[:3] + b'\x08' + old[4:])
            n = struct.unpack('>I', old[4:8])[0]
            payload = b'' if value is None else value + b'\x00'
            self.assertEqual(n, len(payload))
            self.assertEqual(old[8:8+n], payload)
            self.assertEqual(old[8+n:], struct.pack('>IIIII', 2, 4, 0x12345678, 0, 0))

    def test_actual_reader_results(self):
        first = observations((ROOT / 'capture.log').read_text())
        repeat = observations((ROOT / 'capture-repeat.log').read_text())
        self.assertEqual(first, repeat)
        self.assertEqual(len(first), 8)
        values = {'ascii': 'reader string', 'empty': '', 'utf8': '日本語', 'null': None}
        for row in first:
            self.assertEqual(row['argc'], len(row['arguments']))
            if row['positive']:
                self.assertEqual(row['ok'], 1)
                self.assertEqual(row['cp'], row['bytes'])
                self.assertEqual(row['ftell'], row['bytes'])
                self.assertEqual(row['arguments'], [dict(index=0,type='gchararray',value=values[row['case']]),
                                                    dict(index=1,type='gint',value=0x12345678)])
            else:
                self.assertNotIn('gchararray', [arg['type'] for arg in row['arguments']])
                if row['case'] in ('ascii', 'utf8'):
                    self.assertEqual(row['ok'], 0)
                elif row['case'] == 'empty':
                    self.assertEqual(row['ok'], 1)
                    self.assertLess(row['cp'], row['bytes'])
                else:
                    self.assertEqual(row['ok'], 1)
                    self.assertEqual(row['arguments'][0]['type'], 'INVALID')
        self.assertIn('ORIGINAL_STRING_READER_VALUE_TYPE_ALIGNMENT_PASS cases=4',
                      (ROOT / 'capture.log').read_text())


if __name__ == '__main__':
    unittest.main()
