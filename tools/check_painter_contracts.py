#!/usr/bin/env python3
"""Check exhaustive common C++ type/entry contracts without claiming a port."""
import csv
import hashlib
import sys
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


# Reviewed bounded native claims, not completion of legacy feature/layout duties.
# Updating a claim requires reviewing its exact row, receipts and source proof.
BOUNDED_HANDLE_PROOFS = {'GimpActionGroup': {'row_sha256': '64edf75d98900079a3a179d3ab9e28de850d9a6deefaadffa94bb49ee69a6d9d',
                     'directory': 'migration/tests/adapter-removal/features-entry-08018',
                     'files': {'report.json': 'eeb4b4711aaf562d78222bc59a4dbcac98e781664a5ba4d1f8f5026839a72ba5',
                               'source-manifest.json': '95e7e8adc01d61671249040aabfa300e57ec9d06db2c49f951cd2b62030a32a2',
                               'tested-owner-ui-sources.tar.gz': '772e131d5d2b4d0de3b5d01e05d5ab9358dad47fc57c992b5dba76fc3a7defca',
                               'normal-results.json': 'd01dc2d2b24950a5b0095f73d32cf3fd7aef480324a5999fcd0f75d8a07d831b',
                               'asan-build.json': '5a802f67368494e2acad55ff44909c0014f280a6d2c1d3ff4bb33b8a85d99b38',
                               'asan-results.json': '679d488d7a817c2c87d4832d7b6b23792021fd30e74e2585d01b55ea245f457f',
                               'regression-results.json': 'd9e06e9147cf1fe5edf7fa24483c44f6b530b56c02e35f14e5ffaa4441748834',
                               'header-compile-result.json': '09d1c95aaf744a81b61f9a8603659e6f2090ae08282b1bbb34fb1fb2c667363f',
                               'native-mutations.json': '4e25afca84a5108a815c40c5eb8b50029fd1b7b68e5e106a1e04ec9bfe726401',
                               'console-version.log': '2c7a1ebd0e3fa42208aaf64124a40f1e6426b228a120763947eac70f2df8b635',
                               'disabled-compile.json': 'b5b975bc182181999f63d84371f54d323d9ea750924dcfe95556db9f3f1b80bf',
                               'pre-extraction-symbol-audit.json': '06d2e48e6187311d94635586a8e3d38acab9740a3c70c2b102ed5cd36423fa8c',
                               'validate.py': 'bb112a67b47a9f4c1669e301a51ec7de32bbc09e1519ceba53b527f1677508cc',
                               'feature-prefs-oncanvas-portrait.png': '7f8543d13473e1b56a9f232e8b79ae8b32a4933450b3b9ed49f451199a9c2cc7',
                               'build-policy.json': '890b213da737cdd70cc74b8306afbd1ace0b7356664c260e1129ae3bfc96b23d'},
                     'predecessors': [{'row_sha256': '9cefd8d72905c39348344fc6a0d7bf255be1b7aeec985f82e7ffbb881a58b37c',
                                       'directory': 'migration/tests/adapter-removal/preset-action-group-entry-08018',
                                       'files': {'validate.py': '59946db2032b3e211397584ae84ba4a6b1e452d78be5ce051239a2f4966c8bcd',
                                                 'source-manifest.json': '98bdb9b04aa7d4e030226265d28fb2713bd041669dae906758299681064087b6',
                                                 'tested-owner-sources.tar.gz': 'eec41d504c59326a0222da5a1a07175a4dafe5a315cffffd5bb19c38e5a06e8f',
                                                 'normal-results.json': 'b644b49e000d181655ca342df0f177bc5687c6deaf0340c5d50067c823f7bd8f',
                                                 'asan-results.json': 'cf01c6cd495130ea763c2e74799fdb82ff084884726f8dbe5c8ef457b776f17a',
                                                 'asan-build.json': '68cf29576b131848d82cf6158c83f2dd76030e1db62ed25bff5fc0094f9d0245',
                                                 'header-compile-result.json': 'f3dafc14f883074ca908c52d8759027298200baafeb3e230a227fa35aa54ecee',
                                                 'red-status.txt': '4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865',
                                                 'expected-red.log': '6ca100c3e30875c8b7531c185c7ab2141ef5e6a7d60ac9d7292be43349027267',
                                                 'native-mutation.json': '6fa8284e75cb132de91f7c90e60b30934d8d9a5044725d67451283b189bbbc52'}},
                                      {'row_sha256': '8f118ef4184e6216937298be6751debb6b6c0463801d6c0b51c0b5989e86a189',
                                       'directory': 'migration/tests/adapter-removal/preset-dialog-entry-08018',
                                       'files': {'report.json': 'b4bb736fc0c1e84907779b37fe72fbb5edca1c518447964d864d3054ccc922be',
                                                 'source-manifest.json': '5921ec3b1d2268d8b75b6f7eb7777aa633bd1d57596d5cf0613473b1e9379563',
                                                 'tested-owner-ui-sources.tar.gz': '8a2638e973029528726f750ceb088d6bdb9be3817ef595e2c7d08df06527ce03',
                                                 'normal-results.json': '505bdd21c102ff019a8f58132652c24f8f00cfea45412a6d0631e5bd883d5602',
                                                 'asan-build.json': '358efcfb815cafbc485a49d443add97573a5a33e95660628a25e19d43041405e',
                                                 'asan-results.json': 'f5c8dd1f37eaeb5d05e089320b00a941ef27537d6dd5e8901ac680d05bc60ce4',
                                                 'ui-results.json': '004b58ad94a9deed683436c55b96c1ca1d4253719a1c9271891283f68deb725c',
                                                 'header-compile-result.json': '23290ad361dcf93d77e688bbe179cf3bf10a0b70ed99d1f45dbdda042e9425c9',
                                                 'native-mutation.json': '315ab1fa698e9678044cc830d3c37b878d981a6adda36fbe6ff54ee1147f7e31',
                                                 'expected-red.log': '2513ef16fcae2081b7c04dd5cd979c80bde222f7ce21f30bb675984ce201918a',
                                                 'red-status.txt': '4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865',
                                                 'validate.py': '5932f6bda0c43f559ace75d60a5081866dc1e970196e600e8221b97937b325e1',
                                                 'original-dialog-entry-oncanvas-portrait.png': '50417cc5a47c300e0c4d42a5bd07a80514768902eefd40d7e7eb6b4479e519a2'}},
                                      {'row_sha256': '1ee6f423a7c33d094ba80366efc369c478be4443929f27f169a1200a5f9a2917',
                                       'directory': 'migration/tests/adapter-removal/preset-prefs-entry-08018',
                                       'files': {'report.json': '486c816b1a776af4922d40cb05d9ec3da55c2bf11d14293d8250485ee72947cc',
                                                 'source-manifest.json': '831bf30b83d961573d87f869c96e4f6cede7cab354f410e007a6db6d3be57fe8',
                                                 'tested-owner-ui-sources.tar.gz': '511cfb397b964d509205bb49689e9d352e506f50f0a0d0c9f9cd01f43a751cf4',
                                                 'normal-results.json': '33b5f802188be92b58f6fa1340b20c6daa4bc39e8bcdb476209182ad694bbd21',
                                                 'asan-build.json': 'f65fcf638675fe1d1bb25befe5d8a4125bb9f024553732619b057263f2bf8055',
                                                 'asan-results.json': 'b45afd2d9719b3343fd31b79837bfd6296bade8d99bb7a8fcf6d6d20339bd16a',
                                                 'ui-results.json': '38a830b92794e5388b5f48fed4567fe8cdc2e3023e74b4cf68c7f62b79d668bd',
                                                 'header-compile-result.json': '6fb53a7a0b05efa258ead44a031f9ac583483022f5fc88e37a81cd93e199527c',
                                                 'native-mutation.json': 'a42b2c2658021cd2370083d7ddaa00563b03979a2936ab991f2d1b089b5c052c',
                                                 'expected-red.log': '5c6f1db2a841ee36f6f5dac2c0936f524e92aa21e7dc2aa0b2f3d7ef8d1a5e52',
                                                 'red-status.txt': '4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865',
                                                 'validate.py': '4d0d74505ce1d2f6dd9bfa67fb96b954a868600d790ba7b35bab087ee4092379',
                                                 'original-prefs-oncanvas-portrait.png': '52f19fcdf339251a87225eebcc3c87c106f2eecff9adbdfe31f5ebed3b4cb871',
                                                 'native-page-red.json': '53f4baf54e41c0900ae761af8fb53d630481aa156fd8c0ab71e797f2113fa95b',
                                                 'native-page-red.log': 'b889916bffc468cf45d98a038e6c5692a4187ac48233f3775e42f4a773fa21ec'}}]},
 'GimpJsonResource': {'row_sha256': '88077bf418398c57fb76263381d4ce962977c4c4c39c486256b05061af70748f',
                      'directory': 'migration/tests/adapter-removal/json-resource-entries-08018',
                      'files': {'validate.py': '36919260ead5d843581e0fea8f4b167fc877c4d744c16140965cb0ee437a682a',
                                'source-manifest.json': '8b72de54ff7ccba9dd7955d864efa15ac1561829781e73c9f1182eef74556eea',
                                'tested-owner-sources.tar.gz': 'f13c1e63d57b6d6d6c72ef2da0fdc91ef60f4f906c283d575dd33086b4382a77',
                                'normal-results.json': 'efdd6d112164459926a8d45761994d93f0c44f00beca9bf3fd2c54d9cda41c76',
                                'asan-results.json': '23b0be7cbd3d608b21d8604de3a737f057b1486c88b450fd85568acc73343895',
                                'asan-build.json': '6c472228eae28b7372fccbf24e1df7fc8802b0d6105af27d81f430046ad895cd',
                                'header-compile-result.json': 'da6d9a648233cbe0650865f3d8b790f52dfdd3fb915770bba374e9078fcf443f',
                                'facade-header.json': 'c213b5bc73511f65836d24025383d4bfb864c96e3d538fa831895e541be995e9',
                                'source-contract.json': '90998f9c3dab503f57278653a1b60c60aab57a2060fbbb2cd9e7119f4eb3a22a'}},
 'Gimp': {'row_sha256': '4f2b881fbce8aa78762f079f49eb15c6e27d905c05081938d77d552536d9fc06',
          'directory': 'migration/tests/adapter-removal/features-entry-08018',
          'files': {'report.json': 'eeb4b4711aaf562d78222bc59a4dbcac98e781664a5ba4d1f8f5026839a72ba5',
                    'source-manifest.json': '95e7e8adc01d61671249040aabfa300e57ec9d06db2c49f951cd2b62030a32a2',
                    'tested-owner-ui-sources.tar.gz': '772e131d5d2b4d0de3b5d01e05d5ab9358dad47fc57c992b5dba76fc3a7defca',
                    'normal-results.json': 'd01dc2d2b24950a5b0095f73d32cf3fd7aef480324a5999fcd0f75d8a07d831b',
                    'asan-build.json': '5a802f67368494e2acad55ff44909c0014f280a6d2c1d3ff4bb33b8a85d99b38',
                    'asan-results.json': '679d488d7a817c2c87d4832d7b6b23792021fd30e74e2585d01b55ea245f457f',
                    'regression-results.json': 'd9e06e9147cf1fe5edf7fa24483c44f6b530b56c02e35f14e5ffaa4441748834',
                    'header-compile-result.json': '09d1c95aaf744a81b61f9a8603659e6f2090ae08282b1bbb34fb1fb2c667363f',
                    'native-mutations.json': '4e25afca84a5108a815c40c5eb8b50029fd1b7b68e5e106a1e04ec9bfe726401',
                    'console-version.log': '2c7a1ebd0e3fa42208aaf64124a40f1e6426b228a120763947eac70f2df8b635',
                    'disabled-compile.json': 'b5b975bc182181999f63d84371f54d323d9ea750924dcfe95556db9f3f1b80bf',
                    'pre-extraction-symbol-audit.json': '06d2e48e6187311d94635586a8e3d38acab9740a3c70c2b102ed5cd36423fa8c',
                    'validate.py': 'bb112a67b47a9f4c1669e301a51ec7de32bbc09e1519ceba53b527f1677508cc',
                    'feature-prefs-oncanvas-portrait.png': '7f8543d13473e1b56a9f232e8b79ae8b32a4933450b3b9ed49f451199a9c2cc7',
                    'build-policy.json': '890b213da737cdd70cc74b8306afbd1ace0b7356664c260e1129ae3bfc96b23d'},
          'predecessors': [{'row_sha256': '9cefd8d72905c39348344fc6a0d7bf255be1b7aeec985f82e7ffbb881a58b37c',
                            'directory': 'migration/tests/adapter-removal/preset-action-group-entry-08018',
                            'files': {'validate.py': '59946db2032b3e211397584ae84ba4a6b1e452d78be5ce051239a2f4966c8bcd',
                                      'source-manifest.json': '98bdb9b04aa7d4e030226265d28fb2713bd041669dae906758299681064087b6',
                                      'tested-owner-sources.tar.gz': 'eec41d504c59326a0222da5a1a07175a4dafe5a315cffffd5bb19c38e5a06e8f',
                                      'normal-results.json': 'b644b49e000d181655ca342df0f177bc5687c6deaf0340c5d50067c823f7bd8f',
                                      'asan-results.json': 'cf01c6cd495130ea763c2e74799fdb82ff084884726f8dbe5c8ef457b776f17a',
                                      'asan-build.json': '68cf29576b131848d82cf6158c83f2dd76030e1db62ed25bff5fc0094f9d0245',
                                      'header-compile-result.json': 'f3dafc14f883074ca908c52d8759027298200baafeb3e230a227fa35aa54ecee',
                                      'red-status.txt': '4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865',
                                      'expected-red.log': '6ca100c3e30875c8b7531c185c7ab2141ef5e6a7d60ac9d7292be43349027267',
                                      'native-mutation.json': '6fa8284e75cb132de91f7c90e60b30934d8d9a5044725d67451283b189bbbc52'}},
                           {'row_sha256': '8f118ef4184e6216937298be6751debb6b6c0463801d6c0b51c0b5989e86a189',
                            'directory': 'migration/tests/adapter-removal/preset-dialog-entry-08018',
                            'files': {'report.json': 'b4bb736fc0c1e84907779b37fe72fbb5edca1c518447964d864d3054ccc922be',
                                      'source-manifest.json': '5921ec3b1d2268d8b75b6f7eb7777aa633bd1d57596d5cf0613473b1e9379563',
                                      'tested-owner-ui-sources.tar.gz': '8a2638e973029528726f750ceb088d6bdb9be3817ef595e2c7d08df06527ce03',
                                      'normal-results.json': '505bdd21c102ff019a8f58132652c24f8f00cfea45412a6d0631e5bd883d5602',
                                      'asan-build.json': '358efcfb815cafbc485a49d443add97573a5a33e95660628a25e19d43041405e',
                                      'asan-results.json': 'f5c8dd1f37eaeb5d05e089320b00a941ef27537d6dd5e8901ac680d05bc60ce4',
                                      'ui-results.json': '004b58ad94a9deed683436c55b96c1ca1d4253719a1c9271891283f68deb725c',
                                      'header-compile-result.json': '23290ad361dcf93d77e688bbe179cf3bf10a0b70ed99d1f45dbdda042e9425c9',
                                      'native-mutation.json': '315ab1fa698e9678044cc830d3c37b878d981a6adda36fbe6ff54ee1147f7e31',
                                      'expected-red.log': '2513ef16fcae2081b7c04dd5cd979c80bde222f7ce21f30bb675984ce201918a',
                                      'red-status.txt': '4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865',
                                      'validate.py': '5932f6bda0c43f559ace75d60a5081866dc1e970196e600e8221b97937b325e1',
                                      'original-dialog-entry-oncanvas-portrait.png': '50417cc5a47c300e0c4d42a5bd07a80514768902eefd40d7e7eb6b4479e519a2'}},
                           {'row_sha256': 'e546e7c029432c4d100033c0f5dadfc6d60530ad6671f4a00c5e0caf406ff645',
                            'directory': 'migration/tests/adapter-removal/preset-prefs-entry-08018',
                            'files': {'report.json': '486c816b1a776af4922d40cb05d9ec3da55c2bf11d14293d8250485ee72947cc',
                                      'source-manifest.json': '831bf30b83d961573d87f869c96e4f6cede7cab354f410e007a6db6d3be57fe8',
                                      'tested-owner-ui-sources.tar.gz': '511cfb397b964d509205bb49689e9d352e506f50f0a0d0c9f9cd01f43a751cf4',
                                      'normal-results.json': '33b5f802188be92b58f6fa1340b20c6daa4bc39e8bcdb476209182ad694bbd21',
                                      'asan-build.json': 'f65fcf638675fe1d1bb25befe5d8a4125bb9f024553732619b057263f2bf8055',
                                      'asan-results.json': 'b45afd2d9719b3343fd31b79837bfd6296bade8d99bb7a8fcf6d6d20339bd16a',
                                      'ui-results.json': '38a830b92794e5388b5f48fed4567fe8cdc2e3023e74b4cf68c7f62b79d668bd',
                                      'header-compile-result.json': '6fb53a7a0b05efa258ead44a031f9ac583483022f5fc88e37a81cd93e199527c',
                                      'native-mutation.json': 'a42b2c2658021cd2370083d7ddaa00563b03979a2936ab991f2d1b089b5c052c',
                                      'expected-red.log': '5c6f1db2a841ee36f6f5dac2c0936f524e92aa21e7dc2aa0b2f3d7ef8d1a5e52',
                                      'red-status.txt': '4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865',
                                      'validate.py': '4d0d74505ce1d2f6dd9bfa67fb96b954a868600d790ba7b35bab087ee4092379',
                                      'original-prefs-oncanvas-portrait.png': '52f19fcdf339251a87225eebcc3c87c106f2eecff9adbdfe31f5ebed3b4cb871',
                                      'native-page-red.json': '53f4baf54e41c0900ae761af8fb53d630481aa156fd8c0ab71e797f2113fa95b',
                                      'native-page-red.log': 'b889916bffc468cf45d98a038e6c5692a4187ac48233f3775e42f4a773fa21ec'}}]}}


def verify_handle_implementations(handles, root=None):
    root = ROOT if root is None else Path(root)
    errors = []
    for row in handles:
        name = row['legacy_type']
        if not row['declared_at'] or not row['contract']:
            errors.append('incomplete handle contract: ' + name)
        if row['implementation'] == 'TODO':
            if name in BOUNDED_HANDLE_PROOFS:
                errors.append('reviewed bounded handle claim was rolled back: ' + name)
            continue
        proof = BOUNDED_HANDLE_PROOFS.get(name)
        if not proof:
            errors.append('unreviewed handle implementation claim: ' + name)
            continue
        digest = hashlib.sha256(json.dumps(row, sort_keys=True).encode()).hexdigest()
        if digest != proof['row_sha256']:
            errors.append('bounded handle claim differs from reviewed scope: ' + name)
            continue
        try:
            # Immutable predecessors retain their reviewed archives/results; only
            # the successor checks current sources. Never reseal old receipts.
            chain = proof.get('predecessors', []) + [proof]
            for checkpoint in chain:
                evidence = root / checkpoint['directory']
                for filename, expected in checkpoint['files'].items():
                    if hashlib.sha256((evidence / filename).read_bytes()).hexdigest() != expected:
                        raise ValueError('changed receipt: ' + filename)
                command = [sys.executable, '-I', str(evidence / 'validate.py')]
                if checkpoint is proof:
                    command.append('--check-source')
                result = subprocess.run(command, cwd=root, capture_output=True, text=True)
                if result.returncode:
                    raise ValueError('source-bound evidence validation failed: ' +
                                     (result.stderr.strip().splitlines() or ['no diagnostic'])[-1])
        except (OSError, ValueError) as error:
            errors.append('invalid bounded handle proof: ' + name + ': ' + str(error))
    return errors


def read(path):
    with (ROOT / path).open() as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def entry_sources():
    result = {}
    for row in read('migration/inventory/cpp-definition-review.tsv'):
        if row['classification'] in ('C_HEADER_ENTRY', 'LOCAL_C_DECLARED_GET_TYPE'):
            result[row['symbol']] = row['cpp_site']
    for row in read('migration/inventory/generated-gtype-review.tsv'):
        result[row['symbol']] = row['macro_site']
    for row in read('migration/inventory/non-gimp-c-entry-review.tsv'):
        if row['classification'] in ('C_DIRECT', 'C_CALLBACK'):
            result[row['symbol']] = row['cpp_definition']
    return result


def verify():
    errors = []
    handles = read('migration/contracts/cpp-handle-registry.tsv')
    types = {row['legacy_type'] for row in handles}
    source = json.loads((ROOT / 'migration/baseline/baseline.json').read_text())['source']['commit']
    declaration_lines = subprocess.check_output(
        ['git', 'grep', '-n', '-E',
         '^[[:space:]]*__(DECLARE_GTK_(CLASS|CAST|IFACE)|DECLARE_GIMP_INTERFACE)__',
         source, '--', 'app'], cwd=ROOT, text=True)
    declarations = {match.group(1).lstrip('_') for match in
                    re.finditer(r'__(?:DECLARE_GTK_(?:CLASS|CAST|IFACE)|DECLARE_GIMP_INTERFACE)__\s*\(\s*(\w+)', declaration_lines)}
    if not declarations <= types:
        errors.append('missing declared standard/custom types: ' + str(declarations - types))
    for path, field in [('cpp-types.tsv', 'runtime_gtype'),
                        ('generated-gtype-review.tsv', 'instance_type')]:
        expected = {row[field] for row in read('migration/inventory/' + path)}
        if not expected <= types:
            errors.append('missing type contracts: ' + str(expected - types))
    if len(types) != len(handles) or len({r['handle'] for r in handles}) != len(handles):
        errors.append('duplicate type or handle contract')
    errors.extend(verify_handle_implementations(handles))
    entries = read('migration/contracts/cpp-c-entry-map.tsv')
    expected_entries = entry_sources()
    if len(entries) != len(expected_entries) or {r['c_entry'] for r in entries} != expected_entries.keys():
        errors.append('C-entry coverage differs from audited source')
    if len({r['cpp_operation'] for r in entries}) != len(entries):
        errors.append('more than one C entry assigned to one C++ operation')
    handle_names = {r['handle'] for r in handles}
    for row in entries:
        operation = row['cpp_operation']
        method = operation.rsplit('::', 1)[-1]
        if method in {'new', 'delete', 'register', 'class', 'struct', 'template',
                      'operator', 'return', 'default', 'private', 'public',
                      'protected', 'this', 'throw', 'try', 'catch', 'using', 'typename'}:
            errors.append('reserved C++ operation name: ' + operation)
        if operation.startswith('TypeTraits<'):
            if operation.split('<', 1)[1].split('>', 1)[0] not in types:
                errors.append('unknown C type in TypeTraits route: ' + operation)
        elif operation.split('::', 1)[0] not in handle_names:
            errors.append('unknown named handle: ' + operation)
        if row['legacy_site'] != expected_entries.get(row['c_entry']) or not row['cpp_operation'] or not row['rule']:
            errors.append('invalid C-entry source or route: ' + row['c_entry'])
    return errors, len(handles), len(entries)


def main():
    errors, handles, entries = verify()
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'{handles} unique typed-handle contracts; {entries} C entries map to a single C++ route; reviewed bounded native handle proofs verified; full legacy feature/layout implementation not claimed')


if __name__ == '__main__':
    main()
