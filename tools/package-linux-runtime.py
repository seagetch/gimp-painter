#!/usr/bin/env python3
"""Stage a Debian 13 runtime from a completed build, without rebuilding/installing system files.

Use the shared build lock around this command. This is a prototype by default.
Final candidates require a clean, committed source and an explicit aggregate gate.
Only Meson-installed files and allowlisted dependency runtime assets enter the bundle.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import urllib.parse

REPO = Path(__file__).resolve().parent.parent
TRIPLET = 'x86_64-linux-gnu'
BASE_ABI = {'ld-linux-x86-64.so.2', 'libc.so.6', 'libm.so.6', 'libmvec.so.1',
            'libpthread.so.0', 'librt.so.1', 'libdl.so.2', 'libutil.so.1',
            'libresolv.so.2'}
# Runtime assets, never build caches, user profiles, credentials, or environment dumps.
DEPENDENCY_TREES = [f'usr/lib/{TRIPLET}/{x}' for x in
    ('babl-0.1', 'gegl-0.4', 'girepository-1.0', 'gdk-pixbuf-2.0', 'gtk-3.0', 'gio', 'blas', 'lapack')]
DEPENDENCY_TREES += ['usr/lib/python3/dist-packages/gi',
    'usr/share/mypaint-data', 'usr/share/icons/Adwaita',
    'usr/share/icons/hicolor', 'usr/share/glib-2.0/schemas', 'usr/share/gtk-3.0',
    f'usr/lib/{TRIPLET}/libgtk-3-0t64/gtk-query-immodules-3.0']


def command(argv, **kwargs):
    return subprocess.check_output([str(a) for a in argv], text=True, **kwargs).strip()


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


def json_write(path, obj):
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False) + '\n')


def elf(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open('rb') as stream:
        return stream.read(4) == b'\x7fELF'


def parse_ldd_line(line):
    match = re.search(r'^\s*(\S+) => (/.+?) \(', line)
    return match.groups() if match else None


def require_base_abi(soname):
    if soname not in BASE_ABI:
        raise RuntimeError(f'Non-baseline host dependency must be bundled: {soname}')


def source_inventory(repository, source_only=True):
    suffixes = {'.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.py', '.sh', '.in', '.vala'}
    def selected(name):
        return not source_only or Path(name).suffix in suffixes or Path(name).name in {'meson.build', 'meson_options.txt'}
    tracked = sorted(name for name in command(['git','ls-files'],cwd=repository).splitlines() if selected(name))
    untracked = sorted(name for name in command(['git','ls-files','--others','--exclude-standard'],cwd=repository).splitlines() if selected(name))
    hashes = {name:sha(repository/name) for name in sorted(set(tracked+untracked)) if (repository/name).is_file()}
    return {'tracked':tracked, 'untracked':untracked, 'files_sha256':hashes}


def data_state(repository):
    return {'commit':command(['git','rev-parse','HEAD'],cwd=repository),
            'dirty':bool(command(['git','status','--porcelain','--untracked-files=no'],cwd=repository)),
            'inventory':source_inventory(repository,source_only=False)}


def assert_snapshot_unchanged(sources_before, sources_after, data_before, data_after):
    if sources_before != sources_after:
        raise RuntimeError('Source inventory/content changed during candidate staging')
    if data_before != data_after:
        raise RuntimeError('Pinned gimp-data commit/content changed during candidate staging')


def copy_runtime(source, target):
    if source.is_dir():
        shutil.copytree(source, target, symlinks=True, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.a', '*.la', '*.pc'))
    elif source.is_symlink():
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() and not target.is_symlink():
            target.symlink_to(os.readlink(source))
    elif source.is_file():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def relative_symlinks(root):
    for path in root.rglob('*'):
        if not path.is_symlink():
            continue
        link = os.readlink(path)
        if link.startswith('/usr/'):
            target = root / link.lstrip('/')
            if not target.exists():
                raise RuntimeError(f'Unbundled absolute symlink: {path.relative_to(root)} -> {link}')
            path.unlink()
            path.symlink_to(os.path.relpath(target, path.parent))
        if not path.exists():
            raise RuntimeError(f'Dangling symlink in bundle: {path.relative_to(root)}')
        if not path.resolve().is_relative_to(root.resolve()):
            raise RuntimeError(f'Symlink escapes bundle: {path.relative_to(root)}')


def inventory(root):
    result = {}
    for path in sorted(root.rglob('*')):
        rel = str(path.relative_to(root))
        if path.is_symlink():
            result[rel] = {'symlink': os.readlink(path)}
        elif path.is_file():
            result[rel] = {'bytes': path.stat().st_size, 'sha256': sha(path),
                           'mode': oct(path.stat().st_mode & 0o777)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=REPO/'build-debian13')
    parser.add_argument('--deps', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--candidate', action='store_true')
    parser.add_argument('--aggregate-gate', type=Path)
    parser.add_argument('--archive', action='store_true')
    parser.add_argument('--source-archive', action='store_true',
                        help='Only after review confirms committed evidence contains no private data')
    args = parser.parse_args()
    build, deps, output = args.build.resolve(), args.deps.resolve(), args.output.resolve()
    if output.exists():
        parser.error('Output must not exist; choose a new destination to preserve evidence')
    options = {o['name']: o['value'] for o in json.loads((build/'meson-info/intro-buildoptions.json').read_text())}
    prefix = Path(options['prefix'])
    if options['libdir'] != 'lib/'+TRIPLET:
        parser.error('This recipe targets Debian amd64 multiarch only')
    dirty = command(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=REPO)
    commit = command(['git', 'rev-parse', 'HEAD'], cwd=REPO)
    initial_sources = source_inventory(REPO)
    initial_data = data_state(REPO/'gimp-data')
    data_commit = initial_data['commit']
    untracked_sources = initial_sources['untracked']
    source_hashes = initial_sources['files_sha256']
    if args.candidate:
        if dirty or untracked_sources or initial_data['dirty'] or initial_data['inventory']['untracked'] or not args.aggregate_gate or not args.source_archive:
            parser.error('Candidate requires clean committed source, --aggregate-gate and reviewed --source-archive')
        gate = json.loads(args.aggregate_gate.read_text())
        if gate.get('source_commit') != commit or gate.get('all_passed') is not True:
            parser.error('Aggregate gate must pass and identify this exact source commit')
        for rel in ['app/gimp-3.0', 'app/gimp-console-3.0']:
            tested = gate.get('build_inputs', {}).get(rel, {})
            if tested.get('sha256') != sha(build/rel):
                parser.error(f'Aggregate gate must seal the exact tested executable: {rel}')
        freshness = subprocess.run(['ninja', '-C', str(build), '-n'], capture_output=True, text=True)
        if freshness.returncode or 'no work to do.' not in freshness.stdout:
            parser.error('Candidate build has pending default-target work; compile/retest before packaging')
    output.mkdir(parents=True)
    stage, bundle = output/'destdir', output/'gimp-painter-linux-x86_64'
    logs = output/'evidence'
    logs.mkdir()
    recipe = output/'recipe'
    recipe.mkdir()
    for rel in ['tools/package-linux-runtime.py', 'tools/test-linux-runtime.py',
                'migration/packaging/AppRun', 'migration/packaging/README-linux.txt']:
        copy_runtime(REPO/rel, recipe/rel)
    json_write(recipe/'recipe-files.json', inventory(recipe))
    # Capture only this explicit command output, never full process environments.
    with (logs/'install.log').open('w') as log:
        subprocess.run(['meson', 'install', '-C', str(build), '--no-rebuild',
                        '--destdir', str(stage)], stdout=log, stderr=subprocess.STDOUT, check=True)
    installed = stage/prefix.relative_to('/')
    bundle.mkdir()
    shutil.move(installed, bundle/'usr')
    runtime = bundle/'deps'
    runtime.mkdir()
    dep_root = deps/'root'
    for rel in DEPENDENCY_TREES:
        source = dep_root/rel
        if source.exists():
            copy_runtime(source, runtime/rel)
    # Inspect every installed executable/shared library plus all dlopen module roots.
    env = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C',
           'LD_LIBRARY_PATH': f'{bundle}/usr/lib/{TRIPLET}:{dep_root}/usr/lib/{TRIPLET}:{dep_root}/usr/lib/{TRIPLET}/blas:{dep_root}/usr/lib/{TRIPLET}/lapack'}
    queue = [p for top in (bundle/'usr', runtime) for p in top.rglob('*') if elf(p)]
    inspected, edges, host = set(), {}, {}
    while queue:
        binary = queue.pop()
        key = str(binary.resolve())
        if key in inspected:
            continue
        inspected.add(key)
        result = subprocess.run(['ldd', str(binary)], env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f'ldd failed on {binary}: {result.stderr}')
        if 'not found' in result.stdout:
            raise RuntimeError(f'Missing runtime dependency: {binary}\n{result.stdout}')
        rows = []
        for line in result.stdout.splitlines():
            entry = parse_ldd_line(line)
            if not entry:
                continue
            soname, resolved = entry
            source = Path(resolved)
            row = {'soname': soname}
            if source.is_relative_to(bundle):
                row['bundled'] = str(source.relative_to(bundle))
            elif source.is_relative_to(dep_root) and soname not in BASE_ABI:
                rel = source.relative_to(dep_root)
                # Dereference version links and install the requested SONAME as a regular file.
                # This is intentional: no library symlink can escape the relocatable bundle.
                target = runtime/rel
                target.parent.mkdir(parents=True, exist_ok=True)
                if not target.exists():
                    shutil.copy2(source.resolve(), target)
                    queue.append(target)
                row['bundled'] = str(target.relative_to(bundle))
                row['archive_path'] = str(source.resolve().relative_to(dep_root))
            else:
                require_base_abi(soname)
                system_path = Path('/usr/lib')/TRIPLET/soname
                if not system_path.exists():
                    raise RuntimeError(f'Host ABI library missing: {soname}')
                row['system'] = str(system_path)
                host[soname] = {'path': str(system_path), 'sha256': sha(system_path.resolve())}
            rows.append(row)
        edges[str(binary.relative_to(bundle))] = rows
    relative_symlinks(runtime)
    # GLib schemas are relocatable bytecode; generate them from the pinned runtime XML.
    schemas = runtime/'usr/share/glib-2.0/schemas'
    subprocess.run([str(dep_root/'usr/bin/glib-compile-schemas'), str(schemas)], env=env, check=True)
    # This baseline was not configured with relocatable-bundle=yes. Its standard
    # MyPaint path is compile-time absolute, so override only that search path in
    # the bundled system config using GIMP's supported environment expansion.
    gimprc = bundle/'usr/etc/gimp/3.0/gimprc'
    with gimprc.open('a') as config:
        config.write('\n# Relocatable package resource override.\n'
                     '(mypaint-brush-path "${GIMP_PAINTER_BUNDLE}/deps/usr/share/mypaint-data/1.0/brushes:${gimp_dir}/mybrushes")\n')
    # Exact runtime-owned Debian package metadata and notices, resolved from locked archives.
    locks = [REPO/'migration/baseline/debian13-package-lock.json',
             REPO/'migration/baseline/httpd-debian13-package-lock.json']
    packages = {}
    for lock in locks:
        if lock.exists():
            for item in json.loads(lock.read_text())['packages']:
                packages[item['filename']] = item
    bundled_paths = {str(p.relative_to(runtime)) for p in runtime.rglob('*') if p.is_file() or p.is_symlink()}
    # SONAMEs can be links to versioned files; add original targets for package ownership lookup.
    bundled_paths |= {r['archive_path'] for rows in edges.values() for r in rows if 'archive_path' in r}
    owners, notices = {}, bundle/'licenses'
    notices.mkdir()
    for filename, item in sorted(packages.items()):
        archive = deps/'apt/archives'/filename
        if not archive.exists():
            raise RuntimeError(f'Locked archive missing: {filename}')
        proc = subprocess.Popen(['dpkg-deb', '--fsys-tarfile', str(archive)], stdout=subprocess.PIPE)
        paths = set()
        with tarfile.open(fileobj=proc.stdout, mode='r|') as tar:
            for member in tar:
                paths.add(member.name.removeprefix('./'))
        proc.stdout.close()
        if proc.wait() != 0:
            raise RuntimeError(f'Could not inspect archive: {filename}')
        used = sorted(paths & bundled_paths)
        if not used:
            continue
        if sha(archive) != item['sha256']:
            raise RuntimeError(f'Locked archive checksum mismatch: {filename}')
        source_field = command(['dpkg-deb', '-f', archive, 'Source'])
        source_name = (source_field or item['package']).split()[0]
        source_version = re.search(r'\((.*?)\)', source_field)
        source_version = source_version.group(1) if source_version else item['version']
        doc = dep_root/'usr/share/doc'/item['package']/'copyright'
        if not doc.exists():
            raise RuntimeError(f'License notice missing for {item["package"]}')
        copyright_dest = notices/(item['package']+'.copyright')
        shutil.copy2(doc.resolve(), copyright_dest)
        owners[item['package']] = {**item, 'source_package': source_name,
            'source_version': source_version,
            'source_index': 'https://snapshot.debian.org/package/'+urllib.parse.quote(source_name)+'/'+urllib.parse.quote(source_version, safe='')+'/',
            'copyright': str(copyright_dest.relative_to(bundle)),
            'copyright_sha256': sha(copyright_dest), 'bundled_files': used}
    source_notices = {}
    # Meson installs artwork/plug-ins without every accompanying source notice.
    # Preserve every tracked explicit notice from both pinned source trees.
    for repository, label in [(REPO, 'gimp'), (REPO/'gimp-data', 'gimp-data')]:
        paths = command(['git','-C',repository,'ls-files']).splitlines()
        chosen = [rel for rel in paths if Path(rel).name.upper().startswith(('COPYING','LICENSE'))]
        if label == 'gimp-data':
            chosen += [rel for rel in ['icons/README.md', 'images/README.md',
                       'images/splash-log.md', 'images/logo/README.md', 'images/logo-log.md'] if rel in paths]
        for rel in sorted(set(chosen)):
            source = repository/rel
            target = notices/'source-notices'/label/rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            source_notices[label+'/'+rel] = {'sha256':sha(source),'bundled':str(target.relative_to(bundle))}
    common = dep_root/'usr/share/common-licenses'
    if not common.exists():
        common = Path('/usr/share/common-licenses')
    shutil.copytree(common, notices/'common-licenses', symlinks=False)
    launcher = REPO/'migration/packaging/AppRun'
    shutil.copy2(launcher, bundle/'AppRun')
    (bundle/'AppRun').chmod(0o755)
    shutil.copy2(REPO/'migration/packaging/README-linux.txt', bundle/'README.txt')
    # A checked-in archive excludes credentials, .git, build outputs, profiles and caches.
    # A dirty prototype's source archive is deliberately labelled incomplete.
    source_dir = output/'source'
    source_dir.mkdir()
    source_archives = [(REPO, commit, 'gimp-painter-source.tar'),
                       (REPO/'gimp-data', data_commit, 'gimp-data-source.tar')] if args.source_archive else []
    for repository, rev, name in source_archives:
        with (source_dir/name).open('wb') as stream:
            subprocess.run(['git', '-C', str(repository), 'archive', '--format=tar', rev], stdout=stream, check=True)
    safe_options = {name: options[name] for name in ['buildtype', 'auto_features', 'libunwind',
        'relocatable-bundle', 'enable-console-bin', 'painter-http'] if name in options}
    manifest = {'format': 1, 'status': 'candidate' if args.candidate else 'prototype-not-a-release',
        'target': 'Debian 13 x86_64', 'source_commit': commit, 'gimp_data_commit': data_commit,
        'tracked_source_dirty': bool(dirty), 'untracked_source_files': untracked_sources,
        'source_archive_complete': bool(args.source_archive and not dirty and not untracked_sources and not initial_data['dirty'] and not initial_data['inventory']['untracked']),
        'source_files_sha256': source_hashes, 'gimp_data_files_sha256': initial_data['inventory']['files_sha256'],
        'source_build_correspondence': 'aggregate-sealed' if args.candidate else 'not established: observed source hashes and binary hashes are separate',
        'build_options': safe_options, 'build_inputs': {},
        'package_recipe_sha256': sha(Path(__file__)), 'launcher_sha256': sha(bundle/'AppRun'),
        'source_notices': source_notices, 'runtime_debian_packages': owners, 'host_runtime': host, 'elf_dependency_graph': edges,
        'packaging_adaptations': ['bundle-local standard MyPaint resource override in system gimprc',
            'generated GLib schema cache', 'launcher-regenerated pixbuf/GTK IM module caches'],
        'platform_gates': {'linux_x86_64': 'runtime smoke pending',
            'linux_wayland': 'not run', 'windows_x86_64': 'not run: no Windows runtime',
            'macos_arm64': 'not run: no macOS runtime', 'macos_x86_64': 'not run: no macOS runtime',
            'physical_tablet': 'not run: no tablet device'},
        'source_delivery': {'source_archives_created': bool(args.source_archive),
            'dependencies': 'source indexes recorded, not bundled or availability-verified'},
        'release_gate': 'No whole-port or release claim; see tasks.md sections 32–38'}
    for rel in ['app/gimp-3.0','app/gimp-console-3.0','config.h','build.ninja']:
        path = build/rel
        manifest['build_inputs'][rel] = {'sha256': sha(path), 'bytes': path.stat().st_size}
    for rel in ['share/gimp/3.0/painter-mypaint-brushes', 'share/gimp/3.0/layer-presets']:
        path = bundle/'usr'/rel
        if not path.is_dir():
            raise RuntimeError(f'Required painter assets not installed: {rel}')
    manifest['assets'] = {'painter_myb_count': len(list((bundle/'usr/share/gimp/3.0/painter-mypaint-brushes').rglob('*.myb'))),
        'layer_preset_json_count': len(list((bundle/'usr/share/gimp/3.0/layer-presets').rglob('*.json')))}
    if manifest['assets']['painter_myb_count'] != 177:
        raise RuntimeError('Expected exact 177 painter brush resources')
    if args.candidate:
        if command(['git','rev-parse','HEAD'],cwd=REPO)!=commit or command(['git','status','--porcelain','--untracked-files=no'],cwd=REPO):
            raise RuntimeError('Source changed during candidate staging')
        assert_snapshot_unchanged(initial_sources, source_inventory(REPO),
                                  initial_data, data_state(REPO/'gimp-data'))
        for rel in ['app/gimp-3.0','app/gimp-console-3.0']:
            if gate['build_inputs'][rel]['sha256']!=manifest['build_inputs'][rel]['sha256']:
                raise RuntimeError('Tested binary changed during candidate staging')
    forbidden_names = {'.git', '.aws', '.codex', '.agents', '__pycache__', '.env'}
    for path in bundle.rglob('*'):
        if any(part in forbidden_names for part in path.relative_to(bundle).parts) or path.suffix=='.pyc':
            raise RuntimeError(f'Private/build cache path must not enter bundle: {path.relative_to(bundle)}')
    json_write(bundle/'build-manifest.json', manifest)
    json_write(bundle/'file-manifest.json', inventory(bundle))
    json_write(logs/'source-archives.json', inventory(source_dir))
    if args.archive:
        archive = output/'gimp-painter-linux-x86_64.tar.zst'
        epoch = command(['git','show','-s','--format=%ct',commit], cwd=REPO)
        subprocess.run(['tar','--sort=name',f'--mtime=@{epoch}','--owner=0','--group=0','--numeric-owner',
                        '-I','zstd -T2 -10','-cf',str(archive),'-C',str(output),bundle.name], check=True)
        (output/'SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n')
    print(json.dumps({'bundle':str(bundle),'status':manifest['status'], 'elf_objects':len(edges),
                      'debian_packages':len(owners),'host_libraries':len(host),'assets':manifest['assets']}))


if __name__ == '__main__':
    main()
