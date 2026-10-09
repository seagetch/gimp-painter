#!/usr/bin/env python3
"""Detect legacy Painter bridge/pixel APIs and unaudited object-data sites.

This is a conservative source gate, not C++ name resolution or a proof that
all feature implementations and runtime contracts are complete.
"""
import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
POLICY = 'migration/contracts/legacy-adapter-removal.json'
SUFFIXES = {'.c', '.h', '.cpp', '.cc', '.cxx', '.hpp', '.hh', '.hxx', '.inc', '.h++', '.C'}
OLD_NAMES = {'NewGClass', 'GClassWrapper', 'IGClass', 'IWithClass', 'WithClass',
             'InvalidClass', 'IObject', 'BoundMethod', 'CopyValue', 'IValue',
             'ImplBase', 'DerivedFrom', 'UseCStructs',
             'G_TYPE_INSTANCE_GET_PRIVATE', 'g_type_class_add_private',
             'g_object_set_cxx_object', 'g_object_get_cxx_object', 'TileManager', 'PixelRegion'}
DATA = re.compile(r'g_object_(?:get|set|steal|replace|dup)_(?:qdata|data)(?:_full)?$')
OLD_INCLUDE = re.compile(r'(?:^|/)(?:glib-cxx-[^/]+|gtk-cxx-[^/]+|pdb-cxx-utils|delegators)\.(?:h|hpp)$|(?:^|/)(?:tile-manager|pixel-region)\.h$')
RAW = re.compile(r'(?:u8|u|U|L)?R"([^ ()\\\t\n]{0,16})\(')
QUOTED = re.compile(r'(?:u8|u|U|L)?(["\'])')
WORD = re.compile(r'[A-Za-z_]\w*|[0-9][A-Za-z_0-9.\']*|::|##|->|&&|\|\|')


@dataclass(frozen=True)
class Token:
    value: str
    kind: str
    line: int


def lex(source):
    # C translation phase 2: a split identifier is still the same token.
    source = source.replace('\r\n', '\n')
    chars, lines, line = [], [], 1
    index = 0
    while index < len(source):
        if source.startswith('\\\n', index):
            index += 2; line += 1; continue
        chars.append(source[index]); lines.append(line)
        if source[index] == '\n': line += 1
        index += 1
    text = ''.join(chars); result = []; index = 0
    while index < len(text):
        char = text[index]; start = index
        if char.isspace():
            if char == '\n': result.append(Token('\n', 'newline', lines[index]))
            index += 1; continue
        if text.startswith('//', index):
            end = text.find('\n', index); index = len(text) if end < 0 else end; continue
        if text.startswith('/*', index):
            end = text.find('*/', index + 2)
            if end < 0: raise ValueError('unterminated block comment')
            row_start = len(result) - 1
            while row_start >= 0 and result[row_start].kind != 'newline': row_start -= 1
            directive = row_start + 1 < len(result) and result[row_start + 1].value == '#'
            # A block comment is whitespace within a preprocessing directive;
            # its embedded newline must not turn '#if 0 /*...*/ || PLATFORM'
            # into an incorrectly proven inactive branch.
            if not directive:
                for pos in range(index, end + 2):
                    if text[pos] == '\n': result.append(Token('\n', 'newline', lines[pos]))
            index = end + 2; continue
        raw = RAW.match(text, index)
        if raw:
            ending = ')' + raw[1] + '"'; end = text.find(ending, raw.end())
            if end < 0: raise ValueError('unterminated raw string')
            index = end + len(ending); result.append(Token(text[start:index], 'string', lines[start])); continue
        quoted = QUOTED.match(text, index)
        if quoted:
            quote = quoted[1]; index = quoted.end()
            while index < len(text):
                if text[index] == '\\': index += 2; continue
                if text[index] == quote: index += 1; break
                if text[index] == '\n': raise ValueError('newline in quoted literal')
                index += 1
            else: raise ValueError('unterminated quoted literal')
            result.append(Token(text[start:index], 'string', lines[start])); continue
        match = WORD.match(text, index)
        if match:
            index = match.end(); value = match[0]
            result.append(Token(value, 'id' if re.fullmatch(r'[A-Za-z_]\w*', value) else 'punct', lines[start])); continue
        index += 1; result.append(Token(char, 'punct', lines[start]))
    return result


def constant(tokens):
    value = ''.join(t.value for t in tokens).strip('()')
    if re.fullmatch(r'0+[uUlL]*', value): return False
    if re.fullmatch(r'[1-9][0-9]*[uUlL]*', value): return True
    return None  # Scan every possibly active platform/configuration branch.


def branches(tokens):
    active, stack, live, dormant = True, [], [], []
    row = []
    for token in [*tokens, Token('\n', 'newline', 0)]:
        if token.kind != 'newline': row.append(token); continue
        if row and row[0].value == '#':
            directive = row[1].value if len(row) > 1 else ''
            if directive in ('if', 'ifdef', 'ifndef'):
                cond = constant(row[2:]) if directive == 'if' else None
                stack.append([active, active and cond is not True, False])
                active = active and cond is not False
            elif directive in ('elif', 'elifdef', 'elifndef', 'else'):
                if not stack or stack[-1][2]: raise ValueError('unmatched/repeated conditional branch')
                cond = constant(row[2:]) if directive == 'elif' else True if directive == 'else' else None
                active = stack[-1][1] and cond is not False
                stack[-1][1] = stack[-1][1] and cond is not True
                if directive == 'else': stack[-1][2] = True
            elif directive == 'endif':
                if not stack: raise ValueError('unmatched endif')
                active = stack.pop()[0]
            else:
                if directive in ('define', 'include', 'include_next'):
                    row = [Token(t.value, 'legacy-header', t.line)
                           if t.kind == 'string' and OLD_INCLUDE.search(t.value.strip('"')) else t for t in row]
                (live if active else dormant).extend(row)
        else:
            (live if active else dormant).extend(row)
        row = []
    if stack: raise ValueError('unterminated conditional')
    return live, dormant


def pasted(tokens):
    result = []
    for token in tokens:
        if len(result) >= 2 and result[-1].value == '##' and result[-2].kind == token.kind == 'id':
            left = result[-2]; result[-2:] = [Token(left.value + token.value, 'id', left.line)]
        else: result.append(token)
    return result


def call_tokens(tokens, start):
    # Include literal arguments in the fingerprint, not just stripped source.
    if start + 1 == len(tokens) or tokens[start + 1].value != '(':
        return tokens[start:start + 1], start + 1
    depth = 0
    for end in range(start + 1, len(tokens)):
        if tokens[end].kind == 'string': continue
        if tokens[end].value == '(': depth += 1
        elif tokens[end].value == ')':
            depth -= 1
            if depth == 0: return tokens[start:end + 1], end + 1
    raise ValueError('unterminated call at line ' + str(tokens[start].line))


def fingerprint(tokens):
    return hashlib.sha256(json.dumps([(t.kind, t.value) for t in tokens], separators=(',', ':')).encode()).hexdigest()


def inspect_tokens(tokens, path):
    tokens = pasted(tokens); findings, sites = [], []
    for index, token in enumerate(tokens):
        value = token.value
        next_values = [t.value for t in tokens[index + 1:index + 4]]
        reason = None
        if token.kind == 'legacy-header': reason = 'obsolete bridge/pixel header'
        if value == '#' and len(next_values)>=2 and next_values[0] in ('include', 'include_next') and next_values[1]=='<':
            end = index + 3
            while end < len(tokens) and tokens[end].value != '>': end += 1
            if end == len(tokens): raise ValueError('unterminated angle include')
            header = ''.join(t.value for t in tokens[index + 3:end])
            if OLD_INCLUDE.search(header): reason = 'obsolete bridge/pixel header'
        if token.kind == 'string':
            if index >= 2 and tokens[index - 2].value == '#' and tokens[index - 1].value in ('include', 'include_next'):
                if OLD_INCLUDE.search(value.strip('"')): reason = 'obsolete bridge/pixel header'
        elif value in OLD_NAMES or re.fullmatch(r'__(?:DECLARE_GTK_(?:CLASS|CAST|IFACE)|DECLARE_GIMP_INTERFACE)__', value):
            reason = 'obsolete bridge/pixel identifier'
        elif value.startswith(('tile_manager_', 'pixel_region_', 'pixel_regions_')):
            reason = 'obsolete pixel API'
        elif value == 'GLib':
            reason = 'obsolete GLib C++ bridge namespace'
        elif value.endswith('Interface') and next_values[:2] == ['::', 'cast']:
            reason = 'obsolete interface lookup'
        elif value == 'ref' and not (index >= 2 and [t.value for t in tokens[index-2:index]] == ['std', '::']):
            # Legitimate variable construction with this spelling is reviewed
            # with its declaration context, not just the identical ref(args).
            end = index + 1
            while end < len(tokens) and tokens[end].value == ')': end += 1
            before = index - 1
            while before >= 0 and tokens[before].value == '(': before -= 1
            alias = before >= 0 and tokens[before].value == '='
            if before > 0 and tokens[before].value == '&':
                alias = tokens[before - 1].value == '='
            alias = alias and end < len(tokens) and tokens[end].value in (';', ',')
            if (end < len(tokens) and tokens[end].value == '(') or alias:
                reason = 'legacy ref acquisition or reviewed local construction'
        if reason:
            expression, _ = call_tokens(tokens, index)
            if value == 'ref':
                start = index - 1
                while start >= 0 and tokens[start].value not in (';', '{', '}'): start -= 1
                expression = tokens[start + 1:index] + expression
            findings.append({'path':path, 'line':token.line, 'kind':reason, 'symbol':value, 'sha256':fingerprint(expression)})
        if token.kind == 'id' and DATA.fullmatch(value):
            expression, _ = call_tokens(tokens, index)
            sites.append({'path':path, 'line':token.line, 'operation':value, 'sha256':fingerprint(expression),
                          'tokens':[(t.kind,t.value) for t in expression], 'indirect':len(expression)==1})
    return findings, sites


def inspect_source(text, path):
    live, dormant = branches(lex(text))
    findings, sites = inspect_tokens(live, path)
    old, inactive_sites = inspect_tokens(dormant, path)
    return {'findings':findings, 'data_sites':sites, 'dormant_findings':old, 'dormant_data_sites':inactive_sites}


def source_paths(root):
    for path in sorted((root / 'app').rglob('*')):
        source = path.suffix in SUFFIXES or path.name.endswith(('.h.in', '.c.in', '.cpp.in', '.hpp.in', '.cc.in'))
        if not source or 'tests' in path.relative_to(root).parts: continue
        if path.is_symlink(): raise ValueError('source symlink requires a reviewed scope: ' + str(path))
        if path.is_file(): yield path


def scan(root, policy):
    errors, findings, data, dormant, sources = [], [], [], [], []
    for path in source_paths(root):
        relative = path.relative_to(root).as_posix(); raw = path.read_bytes()
        try: result = inspect_source(raw.decode('utf-8'), relative)
        except (ValueError, UnicodeError) as error:
            errors.append(relative + ': ' + str(error)); continue
        sources.append({'path':relative, 'sha256':hashlib.sha256(raw).hexdigest()})
        findings.extend(result['findings']);data.extend(result['data_sites']);dormant.extend(result['dormant_findings'])
    if not sources: errors.append('no application sources scanned')
    exceptions = policy['syntax_exceptions']
    for row in exceptions:
        if not row['reason'] or not row['provenance']: raise ValueError('incomplete syntax exception')
    known = Counter((r['path'], r['symbol'], r['sha256']) for r in exceptions)
    if any(count != 1 for count in known.values()): raise ValueError('duplicate syntax exception')
    seen = Counter()
    for row in findings:
        key = row['path'], row['symbol'], row['sha256']
        seen[key] += 1
        if seen[key] > known[key]: errors.append(f"{row['path']}:{row['line']}: {row['kind']}: {row['symbol']}")
    for key in known:
        if seen[key] != known[key]: errors.append('stale syntax exception: ' + str(key))
    dormant_policy = policy.get('dormant_exceptions', [])
    approved_dormant = Counter((r['path'], r['symbol'], r['sha256']) for r in dormant_policy)
    if any(n != 1 for n in approved_dormant.values()) or any(not r['provenance'] or not r['reason'] for r in dormant_policy):
        raise ValueError('invalid dormant exception')
    actual_dormant = Counter((r['path'], r['symbol'], r['sha256']) for r in dormant)
    if actual_dormant != approved_dormant:
        errors.append('dormant legacy sites differ from pinned upstream exceptions')
    approved = Counter()
    for row in policy['data_sites']:
        key = row['path'], row['operation'], row['sha256']
        if key in approved: raise ValueError('duplicate data-site policy entry: ' + str(key))
        if not DATA.fullmatch(row['operation']) or type(row['count']) is not int or row['count'] < 1 or not row['reason'] or not row['provenance']:
            raise ValueError('incomplete data-site review')
        if row['provenance'] == 'upstream':
            if row['path'] not in policy.get('upstream_sources', {}): raise ValueError('missing upstream source provenance')
        elif row['provenance'] not in policy.get('reviews', {}):
            raise ValueError('unknown explicit data-site review')
        elif row['path'] not in policy['reviews'][row['provenance']]['paths']:
            raise ValueError('data-site review does not cover this source path')
        approved[key] = row['count']
    observed = Counter((r['path'], r['operation'], r['sha256']) for r in data)
    for key, count in observed.items():
        if count > approved[key]: errors.append('unaudited object data site: ' + str(key))
    for key, count in approved.items():
        if observed[key] != count: errors.append('stale or changed object data review: ' + str(key))
    observed_paths = {r['path'] for r in sources}
    for path in policy.get('registered_cpp_paths', []):
        if path not in observed_paths: errors.append('registered C++ source missing from scan: ' + path)
    return {'status':'FAIL' if errors else 'PASS','sources':sources,'errors':errors,'active_findings':findings,
            'data_sites':data,'dormant_findings':dormant,'scope':'All app C/C++ source/header/include files, every potentially active configuration; tests excluded'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--policy', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    policy = json.loads((args.policy or args.root / POLICY).read_text())
    if policy['task'] != '05.014' or policy['version'] != 1: raise ValueError('unexpected policy')
    result = scan(args.root, policy)
    if args.report: args.report.write_text(json.dumps(result,indent=2)+'\n')
    print(f"{result['status']}: {len(result['sources'])} sources; {len(result['data_sites'])} reviewed data sites; {len(result['dormant_findings'])} dormant legacy findings")
    for error in result['errors']: print(error)
    raise SystemExit(bool(result['errors']))


if __name__ == '__main__':
    main()
