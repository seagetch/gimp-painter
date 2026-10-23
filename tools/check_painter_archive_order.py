#!/usr/bin/env python3
"""Original 04.009: actual native archive order and explicit root retention.

Default mode checks a historical checkpoint and its recorded source identities;
it does not bind every header or discover new build inputs. --build replays
four existing final link commands to separate files, never builds/runs
GIMP, and runs only six tiny controlled fixture executables. Use the configured
compiler/dependency environment, including HTTP dependencies, for that mode.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'migration/tests/archive-order'
REPORT = FIXTURES / 'native.json'
EVIDENCE = FIXTURES / 'evidence.tar.gz'


def expected_root(root, variant, target):
    return (root['scope'] == 'all' or root['scope'] == 'gui' and target == 'gimp-3.0'
            or root['scope'] == 'http' and variant == 'http')


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def demand(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(argv, cwd=None):
    return subprocess.run(list(map(str, argv)), cwd=cwd, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=180)


def checked(argv, cwd=None):
    result = run(argv, cwd)
    demand(result.returncode == 0, str(argv) + '\n' + result.stderr[-4000:])
    return result.stdout


def symbols(path, build=None):
    """GNU nm POSIX format; preserve strong undefined references, not weak ones."""
    result = {}
    current = str(path)
    for line in checked(['nm', '-g', '-P', path]).splitlines():
        if line.endswith(']:'):
            current = line.split('[', 1)[1][:-2]
            if build:
                current = str(Path(current).resolve().relative_to(build)) if Path(current).is_absolute() else current
            result.setdefault(current, {'defined': {}, 'undefined': set()})
        else:
            parts = line.split()
            if len(parts) < 2:
                continue
            name, kind = parts[:2]
            member = result.setdefault(current, {'defined': {}, 'undefined': set()})
            if kind == 'U':
                member['undefined'].add(name)
            elif kind not in ('w', 'v', '?'):
                member['defined'][name] = kind
    return result


def final_symbols(path):
    return {line.split()[0] for line in checked(['nm', '-P', '--defined-only', path]).splitlines() if line.split()}


def extraction_map(text):
    """Read GNU ld's leading archive-extraction reasons, including thin archives."""
    demand(text.startswith('Archive member included'), 'Missing GNU archive extraction map')
    section = text.split('\n\n', 1)[1]
    result = {}
    pending = None
    for line in section.splitlines():
        if line.startswith(('Merging program properties', 'As-needed library included', 'Discarded input sections', 'Allocating common symbols')):
            break
        if not line.strip():
            continue
        if not line[0].isspace():
            parts = line.split(None, 1)
            if len(parts) == 2 and '(' in parts[1]:
                result[parts[0]] = parts[1].strip()
                pending = None
            else:
                pending = line.strip()
        elif pending and '(' in line:
            result[pending] = line.strip()
            pending = None
    return result


def strong_components(nodes, edges):
    """Tarjan SCCs over actual strong undefined-to-defined member dependencies."""
    graph = {node: set() for node in nodes}
    for source, target in edges:
        graph[source].add(target)
    indices, low, stack, active, components = {}, {}, [], set(), []

    def visit(node):
        indices[node] = low[node] = len(indices)
        stack.append(node)
        active.add(node)
        for target in sorted(graph[node]):
            if target not in indices:
                visit(target)
                low[node] = min(low[node], low[target])
            elif target in active:
                low[node] = min(low[node], indices[target])
        if low[node] == indices[node]:
            found = []
            while True:
                item = stack.pop()
                active.remove(item)
                found.append(item)
                if item == node:
                    break
            if len(found) > 1:
                components.append(sorted(found))

    for node in sorted(nodes):
        if node not in indices:
            visit(node)
    return sorted(components)


class Verify:
    def __init__(self, args):
        self.args = args
        self.output = args.output_dir.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.files = set()
        self.source_hashes = {}
        self.roots = json.loads((FIXTURES / 'roots.json').read_text())['roots']
        snapshot = args.link_commands / 'source-hashes-after.json'
        self.build_source_snapshot = json.loads(snapshot.read_text())
        self.report = {'task': '04.009', 'status': 'RUNNING',
                       'source_commit': checked(['git', 'rev-parse', 'HEAD'], ROOT).strip(),
                       'prior_build_source_snapshot_sha256': sha(snapshot),
                       'verified_at_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                       'production': [], 'controls': [], 'source_sha256': self.source_hashes,
                       'limitations': [
                           'GNU/Linux configured default and HTTP builds only; platform and later feature gates remain separate.',
                           'Default mode verifies historical link evidence and recorded identities, not current full dependency closure; rebuild and refresh after unbound header or source-list changes.',
                           'No GIMP executable, GUI, server, user profile, or lazy get_type is run.',
                           'Production evidence proves object/symbol retention and resolvable archive order, not feature behavior.',
                           'Explicit roots cover current implemented paths; unknown future registrations need their own root contracts.',
                           'Archive SCCs use retained members and strong symbols; they describe potential dependency cycles, not startup order.',
                           'The real archive dropout probe is a partial link and intentionally leaves unrelated references unresolved.',
                           'Constructor and A/B cycle fixtures illustrate linker failure modes; they are not production implementations.']}
        self.capture([Path(__file__), FIXTURES / 'roots.json', *FIXTURES.glob('*.c'), *FIXTURES.glob('*.py')])
        self.capture([ROOT / p for r in self.roots for p in (r['provider'], r['consumer'])])
        self.capture([ROOT / p for p in ('app/meson.build', 'app/core/meson.build', 'app/painter/meson.build',
                                       'app/paint/meson.build', 'app/tools/meson.build', 'app/widgets/meson.build',
                                       'app/httpd/meson.build', 'migration/contracts/module-layout.md')])

    def capture(self, paths):
        for path in paths:
            path = Path(path)
            self.source_hashes[str(path.relative_to(ROOT))] = sha(path)

    def save_file(self, name, data):
        path = self.output / name
        path.write_text(data)
        self.files.add(path)
        return path

    def command(self, name, argv, cwd, expected=0):
        self.save_file(name + '.command.json', json.dumps({'cwd': str(cwd), 'argv': list(map(str, argv))}, indent=2) + '\n')
        result = run(argv, cwd)
        log = self.save_file(name + '.log', result.stdout + result.stderr)
        demand(result.returncode == expected if expected is not None else result.returncode != 0,
               name + ': unexpected exit ' + str(result.returncode) + '\n' + result.stderr[-3000:])
        return result, {'name': name, 'exit_code': result.returncode, 'log_sha256': sha(log),
                        'command_sha256': sha(self.output / (name + '.command.json'))}

    def production(self, variant, build):
        build = build.resolve()
        info = json.loads((build / 'meson-info/meson-info.json').read_text())
        demand(Path(info['directories']['source']).resolve() == ROOT, 'Different build source tree')
        compiles = json.loads((build / 'compile_commands.json').read_text())
        build_records = json.loads((self.args.link_commands / (variant + '-verification.json')).read_text())['checks']
        by_source = {}
        for entry in compiles:
            source = (Path(entry['directory']) / entry['file']).resolve()
            if source.is_relative_to(ROOT):
                by_source.setdefault(str(source.relative_to(ROOT)), []).append(entry['output'])
        cache = {}
        for target in ('gimp-console-3.0', 'gimp-3.0'):
            prefix = variant + '-' + target
            captured = self.args.link_commands / (prefix + '-link-command.txt')
            argv = shlex.split(captured.read_text())
            demand(argv[0] == 'c++' and argv[argv.index('-o') + 1] == 'app/' + target, 'Unexpected final link')
            demand(argv.count('-Wl,--start-group') == argv.count('-Wl,--end-group') == 1, 'Expected one archive rescan group')
            start, end = argv.index('-Wl,--start-group'), argv.index('-Wl,--end-group')
            archives = [a for a in argv if a.endswith('.a') and a.startswith('app/')]
            demand(archives and all(start < argv.index(a) < end for a in archives), 'Production archive outside rescan group')
            for archive in archives:
                if archive not in cache:
                    cache[archive] = symbols(build / archive, build)
            members = {m: (a, s) for a in archives for m, s in cache[a].items()}
            before = {str(build / a): sha(build / a) for a in archives}
            # Thin archives carry paths, not bytes: capture member identities too.
            before.update({str(build / m): sha(build / m) for m in members})
            before[str(build / 'app' / target)] = sha(build / 'app' / target)
            prior = next(r for r in build_records if r['name'] == target)
            demand(before[str(build / 'app' / target)] == prior['executable_sha256'], 'Production binary differs from prior built/source checkpoint')
            # The preceding actual build retained the full source snapshot. Match
            # every translation unit used here, rather than label stale objects
            # with the hash of whatever source happens to be in the checkout now.
            linked_sources = [source for source, outputs in by_source.items()
                              if any(m in members or m in argv for m in outputs)]
            for source in linked_sources:
                demand(sha(ROOT / source) == self.build_source_snapshot.get(source),
                       'Linked source differs from prior actual build: ' + source)
            self.capture([ROOT / source for source in linked_sources])
            output = self.output / (prefix + '-reference')
            demand(not output.is_relative_to(build), 'Reference output must not alter build')
            mapfile = self.output / (prefix + '.map')
            replay = argv.copy()
            replay[replay.index('-o') + 1] = str(output)
            replay += ['-Wl,-Map,' + str(mapfile)]
            _, link_record = self.command(prefix + '-reference', replay, build)
            self.files.add(mapfile)
            included = extraction_map(mapfile.read_text())
            original_symbols, linked_symbols = final_symbols(build / 'app' / target), final_symbols(output)
            active = {m: v for m, v in members.items() if m in included}
            demand(active, 'No actual extracted archive members')
            root_records = []
            for root in self.roots:
                if not expected_root(root, variant, target):
                    demand(not set(root['required_symbols']) & original_symbols, 'Unexpected disabled target symbols: ' + root['name'])
                    continue
                demand(root['source_anchor'] in (ROOT / root['consumer']).read_text(), 'Source root anchor missing: ' + root['name'])
                providers = [m for m in by_source.get(root['provider'], []) if m in active]
                consumers = [m for m in by_source.get(root['consumer'], []) if m in active or m in argv]
                demand(len(providers) == 1 and consumers, 'Provider/consumer not retained: ' + root['name'])
                provider = providers[0]
                needed = set(root['required_symbols'])
                demand(needed <= set(active[provider][1]['defined']), 'Provider no longer defines required roots: ' + root['name'])
                demand(needed <= linked_symbols and needed <= original_symbols, 'Registration/type dropout: ' + root['name'])
                referrers = []
                for consumer in consumers:
                    s = members[consumer][1] if consumer in members else next(iter(symbols(build / consumer).values()))
                    if root['trigger_symbol'] in s['undefined']:
                        referrers.append(consumer)
                demand(referrers, 'No actual compiled explicit reference: ' + root['name'])
                root_records.append({'name': root['name'], 'provider': provider, 'archive': active[provider][0],
                                     'required_symbols': root['required_symbols'], 'trigger_symbol': root['trigger_symbol'],
                                     'consumer_members': referrers, 'extraction_reason': included[provider],
                                     'source': root['provider'], 'source_sha256': sha(ROOT / root['provider'])})
            definitions = {}
            for member, (archive, sym) in active.items():
                for symbol, kind in sym['defined'].items():
                    if kind not in ('W', 'V'):
                        definitions.setdefault(symbol, []).append((archive, member))
            edge_proofs = {}
            for member, (archive, sym) in active.items():
                for symbol in sorted(sym['undefined']):
                    for other, provider in definitions.get(symbol, []):
                        if archive != other:
                            edge_proofs.setdefault((archive, other), {'symbol': symbol, 'consumer': member, 'provider': provider})
            components = strong_components(archives, edge_proofs)
            demand(components, 'Expected actual app archive dependency cycles absent; review changed topology')
            edges = [{'consumer_archive': a, 'provider_archive': b, **proof} for (a, b), proof in sorted(edge_proofs.items())]
            self.save_file(prefix + '-dependencies.json', json.dumps({'sccs': components, 'edges': edges}, indent=2) + '\n')
            row = {'variant': variant, 'target': target, 'link': link_record, 'link_command_sha256': sha(captured),
                   'original_binary_sha256': before[str(build / 'app' / target)], 'reference_binary_sha256': sha(output),
                   'prior_built_binary_identity_matched': True, 'prior_build_source_matches': len(linked_sources),
                   'map_sha256': sha(mapfile), 'archives': archives, 'member_count': len(members),
                   'extracted_member_count': len(active), 'explicit_roots': root_records,
                   'strong_archive_dependency_edges': len(edges), 'cyclic_archive_components': components,
                   'input_sha256': before}
            if target == 'gimp-console-3.0':
                negative = [a.replace('-Wl,--as-needed', '-Wl,--no-as-needed') for a in argv
                            if a not in ('-Wl,--start-group', '-Wl,--end-group')]
                negative[negative.index('-o') + 1] = str(self.output / (prefix + '-no-group'))
                # Disable DSO as-needed only in both controls, so their one difference is archive rescanning.
                negative += ['-Wl,--no-demangle']
                failed, failure = self.command(prefix + '-no-group', negative, build, None)
                unresolved = sorted(set(re.findall(r"undefined reference to [`']([^']+)'", failed.stderr)))
                known = [s for s in unresolved if s in definitions]
                demand(known, 'Negative must fail on real app archive symbols, not missing DSOs/toolchain')
                corrected = negative.copy()
                corrected[corrected.index('-o') + 1] = str(self.output / (prefix + '-group-restored'))
                # Original group positions are valid except for the removed start/end tokens.
                corrected.insert(start, '-Wl,--start-group')
                corrected.insert(end, '-Wl,--end-group')
                _, fixed = self.command(prefix + '-group-restored', corrected, build)
                self.report['controls'].append({'kind': 'real-production-archive-order', 'variant': variant,
                    'negative': failure, 'corrected': fixed, 'static_unresolved_symbols': known,
                    'unresolved_app_providers': {s: definitions[s] for s in known},
                    'dso_as_needed_disabled_in_both_controls': True})
                (self.output / (prefix + '-group-restored')).unlink()
            demand(all(sha(path) == value for path, value in before.items()), 'Production input changed during replay')
            row['production_inputs_unchanged'] = True
            self.report['production'].append(row)
            output.unlink()
            print(prefix + ': actual roots, extraction map, cyclic dependencies PASS', flush=True)
        # Real current type archive, selected without/with an explicit required symbol.
        archive = build / 'app/core/libappcore.a'
        type_symbol = 'gimp_clone_layer_get_type'
        unrooted = self.output / (variant + '-unrooted.o')
        rooted = self.output / (variant + '-rooted.o')
        _, n = self.command(variant + '-real-unrooted', ['ld', '-r', '-o', unrooted, '--start-group', archive, '--end-group'], build)
        _, p = self.command(variant + '-real-rooted', ['ld', '-r', '-u', type_symbol, '-o', rooted, '--start-group', archive, '--end-group'], build)
        demand(type_symbol not in final_symbols(unrooted) and type_symbol in final_symbols(rooted), 'Actual archive root/drop control failed')
        self.report['controls'].append({'kind': 'real-type-archive-dropout', 'variant': variant, 'archive': str(archive),
            'archive_sha256': sha(archive), 'type_symbol': type_symbol, 'unrooted': n, 'explicit_root': p,
            'partial_link_only': True, 'unrooted_symbol_absent': True, 'rooted_symbol_present': True})
        unrooted.unlink()
        rooted.unlink()

    def fixtures(self):
        objects = {}
        for path in sorted(FIXTURES.glob('*.c')):
            output = self.output / (path.stem + '.o')
            self.command(path.stem + '-compile', ['cc', '-c', path, '-o', output], self.output)
            objects[path.stem] = output
        explicit = self.output / 'registration-explicit.o'
        self.command('registration-explicit-compile', ['cc', '-DEXPLICIT_ROOT', '-c', FIXTURES / 'registration-main.c', '-o', explicit], self.output)
        registration = self.output / 'libregistration.a'
        a, b = self.output / 'libcycle-a.a', self.output / 'libcycle-b.a'
        for path, items in ((registration, [objects['registration-only']]), (a, [objects['cycle-a'], objects['cycle-tail']]), (b, [objects['cycle-b']])):
            self.command(path.stem + '-archive', ['ar', 'rcs', path, *items], self.output)
        for name, main, suffix, expected in (
            ('registration-dropped', objects['registration-main'], [registration], 23),
            ('registration-group-still-dropped', objects['registration-main'], ['-Wl,--start-group', registration, '-Wl,--end-group'], 23),
            ('registration-explicit-root', explicit, [registration], 0),
            ('registration-whole-archive', objects['registration-main'], ['-Wl,--whole-archive', registration, '-Wl,--no-whole-archive'], 0)):
            exe = self.output / name
            _, link = self.command(name + '-link', ['cc', main, *suffix, '-o', exe], self.output)
            _, result = self.command(name + '-run', [exe], self.output, expected)
            present = 'fixture_registration_anchor' in final_symbols(exe)
            demand(present == (expected == 0), 'Fixture dropout symbol mismatch')
            self.report['controls'].append({'kind': 'illustrative-registration-fixture', 'name': name, 'link': link,
                                           'run': result, 'registration_member_present': present})
        for name, libs, succeeds in (
            ('cycle-ab-fails', [a, b], False), ('cycle-ba-fails', [b, a], False),
            ('cycle-group', ['-Wl,--start-group', a, b, '-Wl,--end-group'], True),
            ('cycle-repeat', [a, b, a], True)):
            exe = self.output / name
            failed, link = self.command(name + '-link', ['cc', objects['cycle-main'], *libs, '-o', exe], self.output, 0 if succeeds else None)
            row = {'kind': 'illustrative-cycle-fixture', 'name': name, 'link': link}
            if succeeds:
                _, row['run'] = self.command(name + '-run', [exe], self.output)
            else:
                demand('undefined reference' in failed.stderr, 'Fixture must fail on unresolved cycle')
            self.report['controls'].append(row)
        print('Real type dropout, constructor dropout and both archive-cycle orders: controls PASS', flush=True)

    def finish(self):
        demand(all(sha(ROOT / path) == digest for path, digest in self.source_hashes.items()), 'Source changed during verification')
        self.report['status'] = 'PASS'
        self.report['evidence_files'] = {p.name: sha(p) for p in sorted(self.files)}
        with tarfile.open(self.args.evidence, 'w:gz') as archive:
            for path in sorted(self.files):
                archive.add(path, arcname=path.name)
        self.report['evidence_archive_sha256'] = sha(self.args.evidence)
        self.args.report.write_text(json.dumps(self.report, indent=2) + '\n')


def check(report_path, evidence):
    data = json.loads(report_path.read_text())
    demand(data['task'] == '04.009' and data['status'] == 'PASS', 'Not passing archive-order evidence')
    demand(all(sha(ROOT / path) == digest for path, digest in data['source_sha256'].items()), 'Current source/evidence drift')
    demand(sha(evidence) == data['evidence_archive_sha256'], 'Evidence archive mismatch')
    with tarfile.open(evidence, 'r:gz') as archive:
        files = {m.name: hashlib.sha256(archive.extractfile(m).read()).hexdigest() for m in archive.getmembers() if m.isfile()}
    demand(files == data['evidence_files'], 'Evidence member mismatch')
    demand({(r['variant'], r['target']) for r in data['production']} == {(v, t) for v in ('default', 'http') for t in ('gimp-3.0', 'gimp-console-3.0')}, 'Four production targets required')
    demand(all(r['production_inputs_unchanged'] and r['explicit_roots'] and r['cyclic_archive_components'] for r in data['production']), 'Missing actual production evidence')
    roots = json.loads((FIXTURES / 'roots.json').read_text())['roots']
    for row in data['production']:
        expected = {r['name']: r for r in roots if expected_root(r, row['variant'], row['target'])}
        observed = {r['name']: r for r in row['explicit_roots']}
        demand(expected.keys() == observed.keys(), 'Recorded explicit root coverage changed')
        demand(row['link']['exit_code'] == 0, 'Reference link failed')
        for name, root in expected.items():
            record = observed[name]
            demand(record['required_symbols'] == root['required_symbols'] and record['consumer_members']
                   and record['trigger_symbol'] == root['trigger_symbol'] and record['extraction_reason'],
                   'Missing compiled root retention proof: ' + name)
    controls = data['controls']
    demand(len(controls) == 12 and sum(c['kind'] == 'real-production-archive-order' for c in controls) == 2 and sum(c['kind'] == 'real-type-archive-dropout' for c in controls) == 2, 'Missing controls')
    for control in controls:
        if control['kind'] == 'real-production-archive-order':
            demand(control['negative']['exit_code'] != 0 and control['corrected']['exit_code'] == 0
                   and control['static_unresolved_symbols'] and control['dso_as_needed_disabled_in_both_controls'],
                   'Real archive-order control failed')
        elif control['kind'] == 'real-type-archive-dropout':
            demand(control['unrooted']['exit_code'] == control['explicit_root']['exit_code'] == 0
                   and control['unrooted_symbol_absent'] and control['rooted_symbol_present'], 'Real type dropout control failed')
        elif control['kind'] == 'illustrative-registration-fixture':
            dropped = control['name'] in ('registration-dropped', 'registration-group-still-dropped')
            demand(control['link']['exit_code'] == 0 and control['run']['exit_code'] == (23 if dropped else 0)
                   and control['registration_member_present'] == (not dropped), 'Registration fixture failed')
        elif control['kind'] == 'illustrative-cycle-fixture':
            failed = control['name'].endswith('-fails')
            demand((control['link']['exit_code'] != 0 if failed else control['link']['exit_code'] == 0
                    and control['run']['exit_code'] == 0), 'Cycle fixture failed')
    print('04.009 historical archive-order checkpoint and recorded source identities PASS (four links, 12 controls; not a current full dependency-closure check)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, help='existing default build')
    parser.add_argument('--http-build', type=Path)
    parser.add_argument('--link-commands', type=Path, help='directory holding the four captured final link-command.txt files')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--report', type=Path, default=REPORT)
    parser.add_argument('--evidence', type=Path, default=EVIDENCE)
    args = parser.parse_args()
    if args.build:
        demand(args.http_build and args.link_commands and args.output_dir, 'Build mode needs --http-build, --link-commands and --output-dir')
        verifier = Verify(args)
        verifier.production('default', args.build)
        verifier.production('http', args.http_build)
        verifier.fixtures()
        verifier.finish()
    check(args.report, args.evidence)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.SubprocessError, ValueError, KeyError) as error:
        print('archive-order FAIL: ' + str(error), file=sys.stderr)
        sys.exit(1)
