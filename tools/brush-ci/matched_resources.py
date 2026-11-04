"""Read-only, bounded resource receipts; paths and environment never escape.

runtime_resources runs after all measurements: callers archive the exact native
BUILD output into profile/native-output, bind its hash to postflight evidence,
and independently prove freshness before collecting font evidence.
Catalog metadata is the existing staging helper's sibling <catalogs>.json.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import stat

PLUGINS = ('blinds', 'contrast-retinex', 'convolution-matrix', 'tile-small')
PROCEDURES = {'blinds': ['plug-in-blinds'],
              'contrast-retinex': ['plug-in-painter-retinex', 'plug-in-retinex'],
              'convolution-matrix': ['plug-in-painter-convmatrix'],
              'tile-small': ['plug-in-painter-small-tiles', 'plug-in-small-tiles']}
ALIASES = ('gimp30-gmo', 'gimp30-libgimp-gmo', 'gimp30-std-plug-ins-gmo',
           'gimp30-python-gmo', 'gimp30-script-fu-gmo',
           'org.gimp.extension.goat-exercises-gmo')
MAX_BYTES = 32 * 1024 * 1024
MAX_FILES = 50000

class ResourceError(ValueError):
    """Finite public code only; never interpolate an input path or value."""


def fail(code):
    raise ResourceError(code)


def read(path):
    try:
        path = Path(path)
        if not path.is_file() or path.stat().st_size > MAX_BYTES:
            fail('resource-file-invalid')
        data = path.read_bytes()
        if len(data) > MAX_BYTES:
            fail('resource-file-invalid')
        return data
    except OSError:
        fail('resource-read-failed')


def record(path):
    data = read(path)
    return {'sha256': hashlib.sha256(data).hexdigest(), 'size_bytes': len(data)}


def relative(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_.@+/-]+', value):
        fail('resource-relative-name-invalid')
    p = Path(value)
    if p.is_absolute() or any(x in ('', '.', '..') for x in value.split('/')):
        fail('resource-relative-name-invalid')
    return p


def under(root, name):
    path = root / relative(name)
    if not path.resolve().is_relative_to(root.resolve()):
        fail('resource-path-escape')
    return path


def json_read(path):
    def pairs(items):
        d = {}
        for k, v in items:
            if k in d:
                fail('resource-json-duplicate')
            d[k] = v
        return d
    try:
        return json.loads(read(path), object_pairs_hook=pairs)
    except (UnicodeError, json.JSONDecodeError):
        fail('resource-json-invalid')


# Four byte-pinned legacy brush assets exist in both frozen source arms.
# Do not broaden the relative-name grammar for other resources or providers.
LEGACY_SOURCE_BRUSHES = {'painter-mypaint-brushes/kaerhon/smudge_ink(0.7)_sm.myb': {'sha256': '55fda8ffc675b945481fe3af9a192dc1b28df82e071df02809022e90852a972b', 'size_bytes': 6281}, 'painter-mypaint-brushes/kaerhon/smudge_ink(0.7)_sm_prev.png': {'sha256': '9589553417cabe67f91255f36e1e0c772166db8bebf20ccd0281de6560793ca4', 'size_bytes': 5134}, 'painter-mypaint-brushes/ramon/100%_Opaque.myb': {'sha256': 'aaa0a74ddc9d1b809b93599f1cae6cc0c0a282d76af4661d6ba171275fd55b16', 'size_bytes': 4837}, 'painter-mypaint-brushes/ramon/100%_Opaque_prev.png': {'sha256': '0b65ff8aa934be6f5f6e3f5d0f4879da9736e10847451ff5a97f39abfe26df90', 'size_bytes': 6991}}


def inventory(root, *, legacy_source_brushes=False):
    """Hash files relative to a declared root; reject symlink escapes/specials."""
    root = Path(root)
    if not root.is_dir():
        fail('resource-directory-missing')
    result = {}
    try:
        paths = []
        for path in root.rglob('*'):
            paths.append(path)
            if len(paths) > MAX_FILES:
                fail('resource-inventory-limit')
        paths.sort()
        for path in paths:
            name = path.relative_to(root).as_posix()
            if '.git' in path.relative_to(root).parts:
                continue
            if legacy_source_brushes and name in LEGACY_SOURCE_BRUSHES:
                if path.is_symlink() or not path.is_file():
                    fail('resource-evidence-invalid')
                value = record(path)
                if value != LEGACY_SOURCE_BRUSHES[name]:
                    fail('resource-evidence-invalid')
                result[name] = value
                continue
            relative(name)
            if path.is_symlink():
                if not path.resolve().is_relative_to(root.resolve()) or not path.is_file():
                    fail('resource-symlink-invalid')
            if path.is_file():
                result[name] = record(path)
            elif not path.is_dir():
                fail('resource-special-file')
    except OSError:
        fail('resource-read-failed')
    if not result:
        fail('resource-inventory-empty')
    return result


def sexps(data):
    """GIMP config tokenization, including C-style quoted escapes and comments."""
    try:
        text = data.decode('utf-8')
    except UnicodeError:
        fail('plugin-cache-invalid')
    tokens = re.findall(r'\s+|\#[^\n]*|[()]|"(?:[^"\\]|\\.)*"|[^\s()"#]+', text)
    if ''.join(tokens) != text:
        fail('plugin-cache-invalid')
    stack, out = [], []
    count = 0
    for token in tokens:
        if token.isspace() or token.startswith('#'):
            continue
        count += 1
        if count > 200000:
            fail('plugin-cache-limit')
        if token == '(':
            if len(stack) >= 64:
                fail('plugin-cache-limit')
            stack.append([])
        elif token == ')':
            if not stack:
                fail('plugin-cache-invalid')
            node = stack.pop()
            (stack[-1] if stack else out).append(node)
        else:
            if not stack:
                fail('plugin-cache-invalid')
            if token.startswith('"'):
                # GLib config writer uses octal byte escapes in addition to JSON escapes.
                def unescape(m):
                    x = m.group(1)
                    if re.fullmatch('[0-7]{1,3}', x):
                        return chr(int(x, 8))
                    if x not in {'n','r','t','b','f','v','\\','"'}:
                        fail('plugin-cache-invalid')
                    return {'n':'\n','r':'\r','t':'\t','b':'\b','f':'\f','v':'\v','\\':'\\','"':'"'}[x]
                token = re.sub(r'\\([0-7]{1,3}|.)', unescape, token[1:-1])
            stack[-1].append(token)
    if stack:
        fail('plugin-cache-invalid')
    return out


def plugins(source, build, cache_directory):
    root = build / 'plug-ins/common'
    files = inventory(root)
    binaries = {}
    for name in files:
        path = root / name
        if path.stat().st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH):
            if name not in PLUGINS or path.is_symlink():
                fail('plugin-executable-inventory')
            binaries[name] = files[name]
    if set(binaries) != set(PLUGINS):
        fail('plugin-executable-inventory')
    cache = Path(cache_directory) / 'pluginrc'
    registered = {}
    for node in sexps(read(cache)):
        if not node or node[0] != 'plug-in-def':
            continue
        if len(node) < 4 or not isinstance(node[1], str) or not re.fullmatch(r'[0-9]+', str(node[2])):
            fail('plugin-registration-invalid')
        path = Path(node[1])
        if not path.is_absolute() or path.parent != root or path.name not in binaries:
            fail('plugin-registration-provider')
        if path.name in registered:
            fail('plugin-registration-duplicate')
        procs = []
        for child in node[3:]:
            if isinstance(child, list) and child and child[0] == 'proc-def':
                if len(child) < 3 or not isinstance(child[1], str) or not re.fullmatch(r'[A-Za-z0-9_-]+', child[1]):
                    fail('plugin-registration-invalid')
                procs.append(child[1])
        if sorted(procs) != PROCEDURES[path.name] or len(procs) != len(set(procs)):
            fail('plugin-registration-invalid')
        # Hash complete registration content, excluding cell path and file mtime.
        payload = json.dumps(node[3:], sort_keys=True, separators=(',', ':')).encode()
        registered[path.name] = {'procedures': sorted(procs), 'registration_sha256': hashlib.sha256(payload).hexdigest()}
    if set(registered) != set(PLUGINS):
        fail('plugin-registration-inventory')
    return {'executables': binaries, 'registrations': registered,
            'source_inputs': {name: record(source / 'plug-ins/common' / name)
                              for name in ['meson.build', 'convolution-matrix-kernel.h'] + [p + '.c' for p in PLUGINS]},
            'cache_location': 'profile/native-output/pluginrc',
            'cache_freshness_proven': False}


def locales(source, build, catalogs, evidence):
    manifest = json_read(catalogs.with_name(catalogs.name + '.json'))
    if manifest.get('helper_sha256') != record(source / 'tools/run-painter-frozen-aggregate.py')['sha256']:
        fail('catalog-helper-mismatch')
    expected_meta = ('config.h', 'meson-info/intro-buildoptions.json', 'meson-info/intro-install_plan.json',
                     'meson-info/intro-installed.json', 'meson-info/intro-targets.json')
    if set(manifest.get('metadata_sha256', {})) != set(expected_meta):
        fail('catalog-metadata-invalid')
    for name in expected_meta:
        if record(build / name)['sha256'] != manifest['metadata_sha256'][name]:
            fail('catalog-metadata-mismatch')
    targets = json_read(build / 'meson-info/intro-targets.json')
    aliases = {}
    try:
        for name in ALIASES:
            output = evidence['ninja_alias_queries'][name]
            if not isinstance(output, str) or len(output) > MAX_BYTES:
                fail('catalog-alias-unproven')
            lines = output.splitlines()
            if len(lines) < 3 or lines[:2] != [name + ':', '  input: phony']:
                fail('catalog-alias-unproven')
            try:
                end = lines.index('  outputs:')
            except ValueError:
                fail('catalog-alias-unproven')
            # Ninja ToolQuery labels implicit and order-only inputs with "| "
            # and "|| ". These are edge-kind syntax, never part of the path.
            dependencies = []
            for line in lines[2:end]:
                if not line.startswith('    '):
                    fail('catalog-alias-unproven')
                value = line[4:]
                if value.startswith('|| '):
                    value = value[3:]
                elif value.startswith('| '):
                    value = value[2:]
                dependencies.append(value)
            if not dependencies:
                fail('catalog-alias-unproven')
            normalized = []
            for value in dependencies:
                if value.startswith(str(build) + '/'):
                    value = Path(value).relative_to(build).as_posix()
                relative(value)
                normalized.append(value)
            aliases[name] = {'dependencies': sorted(normalized)}
    except ResourceError as error:
        # One finite location label only; no raw query, path or environment.
        error.substage = 'catalog-alias-inputs'
        raise
    observed = inventory(catalogs)
    records = manifest.get('catalogs', {})
    if len(records) != manifest.get('catalog_count') or set(observed) != set(records):
        fail('catalog-inventory-mismatch')
    for name, row in records.items():
        if observed[name] != {'sha256': row.get('sha256'), 'size_bytes': row.get('size_bytes')}:
            fail('catalog-content-mismatch')
        if record(under(build, row['source'])) != observed[name]:
            fail('catalog-source-mismatch')
        matches = [t for t in targets if t.get('id') == row.get('target_id') and
                   str(under(build, row['source'])) in t.get('filename', []) and t.get('installed') is True]
        if len(matches) != 1:
            fail('catalog-target-unproven')
    excluded = {name: record(under(build, name)) for name in manifest.get('excluded_extension_catalog_outputs', [])}
    if not excluded:
        fail('catalog-extension-unproven')
    return {'catalogs': observed, 'aliases': aliases, 'extension_catalogs': excluded,
            'staging_helper_sha256': manifest['helper_sha256']}


def font_schema_inputs(env, evidence):
    """Observe fontconfig's resolved available set, NOT live Pango font choice."""
    deps_value = env.get('GIMP_DEPS_ROOT')
    if not deps_value:
        fail('font-schema-evidence-missing')
    deps = Path(deps_value).resolve()
    roots = [('dependency', deps), ('system-fonts', Path('/usr/share/fonts')),
             ('system-local-fonts', Path('/usr/local/share/fonts')),
             ('system-fontconfig', Path('/etc/fonts')),
             ('system-fontconfig-share', Path('/usr/share/fontconfig')),
             ('system-schemas', Path('/usr/share/glib-2.0/schemas'))]
    def label(path):
        path = Path(path)
        if not path.is_absolute():
            fail('font-schema-path-invalid')
        resolved = path.resolve()
        for namespace, root in roots:
            if resolved == root:
                return namespace
            if resolved.is_relative_to(root):
                name = resolved.relative_to(root).as_posix()
                relative(name)
                return namespace + '/' + name
        fail('font-schema-path-unapproved')
    font_output = evidence['fc_list_stdout']
    config_output = evidence['fc_conflist_stdout']
    if any(not isinstance(x, str) or len(x) > MAX_BYTES for x in (font_output, config_output)):
        fail('font-schema-evidence-invalid')
    fonts = font_output.splitlines()
    if not fonts or len(fonts) > MAX_FILES or any(not f for f in fonts):
        fail('font-resolution-inventory-invalid')
    font_records = {label(f): record(Path(f)) for f in sorted(set(fonts))}
    config_records = {}
    config_order = []
    for line in config_output.splitlines():
        # fc-conflist emits enabled/disabled marker, absolute file, description.
        match = re.fullmatch(r'([+-]) (/.+?):(?: .*|)', line)
        if not match:
            fail('fontconfig-inventory-invalid')
        status, filename = match.groups()
        name = label(filename)
        value = dict(record(Path(filename)), enabled=status == '+')
        if name in config_records and config_records[name] != value:
            fail('fontconfig-inventory-invalid')
        config_records[name] = value
        config_order.append(name)
    if not config_records or not any(r['enabled'] for r in config_records.values()):
        fail('fontconfig-inventory-invalid')
    schema_paths = []
    if env.get('GSETTINGS_SCHEMA_DIR'):
        schema_paths.append(Path(env['GSETTINGS_SCHEMA_DIR']))
    data_dirs = env.get('XDG_DATA_DIRS')
    if not data_dirs or any(not p for p in data_dirs.split(':')):
        fail('schema-search-path-unproven')
    for entry in data_dirs.split(':'):
        p = Path(entry) / 'glib-2.0/schemas'
        if p.exists():
            schema_paths.append(p)
    schemas = {}
    schema_order = []
    for path in schema_paths:
        records = inventory(path)
        if 'gschemas.compiled' not in records:
            fail('schema-compiled-missing')
        schemas[label(path)] = records
        schema_order.append(label(path))
    if not schemas:
        fail('schema-search-path-unproven')
    return {'resolved_font_files': font_records, 'fontconfig_files': config_records,
            'schema_dirs': schemas, 'schema_search_order': schema_order,
            'fontconfig_order': config_order, 'font_resolution_inventory_proven': True,
            'live_pango_font_selection_proven': False,
            'scope': 'same-env-cwd-fontconfig-available-set-not-live-Pango-choice'}




def data_inventory(root):
    """Bounded logical tree expansion, including witnessed same-root symlinks."""
    root = Path(root).resolve()
    files, links = {}, {}
    visited_entries = 0
    def walk(directory, logical, ancestors, depth):
        nonlocal visited_entries
        if depth > 64:
            fail('data-directory-depth-limit')
        resolved = directory.resolve()
        if not resolved.is_relative_to(root):
            fail('resource-symlink-invalid')
        if resolved in ancestors:
            fail('data-directory-cycle')
        try:
            entries = []
            for path in directory.iterdir():
                visited_entries += 1
                if visited_entries > MAX_FILES:
                    fail('resource-inventory-limit')
                entries.append(path)
            for path in sorted(entries):
                name = (logical / path.name).as_posix()
                relative(name)
                if path.is_symlink():
                    try:
                        target = path.resolve(strict=True)
                    except (OSError, RuntimeError):
                        fail('resource-symlink-invalid')
                    if not target.is_relative_to(root):
                        fail('resource-symlink-invalid')
                    target_name = target.relative_to(root).as_posix()
                    links[name] = {'target_relative': target_name,
                                   'kind': 'directory' if target.is_dir() else 'file'}
                if path.is_dir():
                    walk(path, logical / path.name, ancestors | {resolved}, depth + 1)
                elif path.is_file():
                    files[name] = record(path)
                else:
                    fail('resource-special-file')
        except ResourceError:
            raise
        except (OSError, RuntimeError):
            fail('resource-read-failed')
    walk(root, Path(), set(), 0)
    if not files:
        fail('resource-inventory-empty')
    return {'files': files, 'links': links}

def selected_data_resources(env):
    """Ordered XDG fallback assets; never silently admit an unknown provider."""
    raw = env.get('XDG_DATA_DIRS')
    if not raw or any(not value for value in raw.split(':')) or not env.get('GIMP_DEPS_ROOT'):
        fail('data-search-path-unproven')
    deps_share = Path(env['GIMP_DEPS_ROOT']).resolve() / 'usr/share'
    prefix = env.get('GIMP_BUILD_PREFIX')
    prefix_share = Path(prefix).resolve() / 'share' if prefix else None
    known = {deps_share: 'dependency-share', Path('/usr/share'): 'system-share',
             Path('/usr/local/share'): 'system-local-share'}
    entries = []
    for value in raw.split(':'):
        root = Path(value)
        if not root.is_absolute():
            fail('data-search-path-unproven')
        resolved = root.resolve()
        namespace = known.get(resolved)
        is_prefix = prefix_share is not None and resolved == prefix_share
        if namespace is None:
            if not is_prefix:
                fail('data-provider-unapproved')
            namespace = 'cell-prefix-share'
        inputs = {}
        for name in ('icons', 'themes', 'mime'):
            path = root / name
            if not path.exists() and not path.is_symlink():
                inputs[name] = {'present': False}
                continue
            if is_prefix:
                fail('data-cell-prefix-provider-unapproved')
            if not path.is_dir() or path.is_symlink():
                fail('data-provider-invalid')
            inputs[name] = dict(data_inventory(path), present=True)
        entries.append({'provider': namespace, 'inputs': inputs})
    return entries

def runtime_resources(source, build, profile, catalogs, env, evidence, *, cache_directory):
    """Return a private-path-free receipt, or a finite ResourceError.

    Main supplies private outputs of fc-list, fc-conflist and six ninja queries
    collected with this exact env/build cwd, plus cache_absent_preflight from its
    fresh-output receipt. Do not accept user-asserted evidence. Eligibility covers
    these resource inputs, not timing/overall comparison or live Pango selection.
    """
    try:
        source, build, profile, catalogs = (Path(p).resolve() for p in (source, build, profile, catalogs))
        cache_directory = Path(cache_directory)
        if (cache_directory != profile / 'native-output' or cache_directory.is_symlink()
                or cache_directory.resolve() != profile / 'native-output'
                or not cache_directory.is_dir()):
            fail('plugin-cache-archive-binding-mismatch')
        for key, expected in [('GIMP_TESTING_ABS_TOP_SRCDIR', source),
                              ('GIMP_TESTING_ABS_TOP_BUILDDIR', build),
                              ('GIMP_TESTING_PLUGINDIRS', build / 'plug-ins/common'),
                              ('GIMP3_LOCALEDIR', catalogs), ('HOME', profile / 'home'),
                              ('XDG_CACHE_HOME', profile / 'cache')]:
            if not env.get(key) or Path(env[key]).resolve() != expected:
                fail('resource-runtime-binding-mismatch')
        if env.get('GIMP_TESTING_SESSIONRC_NAME') != 'sessionrc-native-oncanvas':
            fail('resource-session-mismatch')
        if env.get('GIMP3_DATADIR') or env.get('GIMP3_DIRECTORY'):
            fail('resource-fallback-override')
        if evidence.get('cache_absent_preflight') is not True:
            fail('plugin-cache-freshness-unproven')
        source_inputs = {name: inventory(source / name, legacy_source_brushes=(name == 'data')) for name in
                         ('data', 'themes', 'gimp-data', 'menus', 'app/tests/gimpdir')}
        built_inputs = {name: inventory(build / name) for name in
                        ('app/tests/gimp-test-icon-theme',)}
        built_inputs['menus'] = {name: record(build / 'menus' / name)
                                for name in ('image-menu.ui', 'dockable-menu.ui')}
        result = {'format': 1, 'plugins': plugins(source, build, cache_directory),
                'locales': locales(source, build, catalogs, evidence),
                'source_inputs': source_inputs, 'built_inputs': built_inputs,
                'font_schema_inputs': font_schema_inputs(env, evidence),
                'selected_data_resources': selected_data_resources(env),
                'comparison_eligible': True,
                'unproven': ['live-Pango-font-selection'],
                'raw_environment_exported': False}
        result['plugins']['cache_freshness_proven'] = True
        result['comparison_inventory'] = {
            'source_inputs': result['source_inputs'], 'built_inputs': result['built_inputs'],
            'locales': result['locales'], 'font_schema_inputs': result['font_schema_inputs'],
            'selected_data_resources': result['selected_data_resources'],
            'plugin_registrations': result['plugins']['registrations'],
            'plugin_source_inputs': result['plugins']['source_inputs']}
        # Binary hashes and mutable cache provenance stay cell-local evidence.
        result['plugin_binaries'] = result['plugins']['executables']
        return result
    except ResourceError:
        raise
    except (OSError, ValueError, TypeError, KeyError, AttributeError, RecursionError):
        fail('resource-evidence-invalid')
