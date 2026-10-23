#!/usr/bin/env python3
"""Small read-only C/C++ source readers for the 10.001 specification tests.

These are lexical readers, not C evaluators.  Expressions remain expressions;
the contracts identify the constructor/decoder that gives them meaning.
"""
from functools import lru_cache
from pathlib import Path
import re
import subprocess

LEGACY = 'afa43fae3e920210146abed514f136fd49f671b5'

@lru_cache(None)
def source(root, path, revision='native'):
    if revision == 'native':
        return (Path(root) / path).read_text()
    return subprocess.check_output(['git', '-C', str(root), 'show', LEGACY + ':' + path], text=True, stderr=subprocess.DEVNULL)

def clean(text):
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ('\n' * m[0].count('\n') if m[0].startswith(('/', '/*')) else m[0]), text, flags=re.S)

def norm(text):
    return re.sub(r'\s+', ' ', clean(text)).strip()

def balanced(text, start, left='(', right=')'):
    depth = 0
    quote = None
    escape = False
    for i in range(start, len(text)):
        c = text[i]
        if quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == quote: quote = None
            continue
        if c in '\"\'': quote = c
        elif c == left: depth += 1
        elif c == right:
            depth -= 1
            if depth == 0: return i
    raise AssertionError('unterminated source expression')

def split_args(text):
    result, start, stack, quote, escape = [], 0, [], None, False
    for i, c in enumerate(text):
        if quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == quote: quote = None
        elif c in '\"\'': quote = c
        elif c in '([{': stack.append(c)
        elif c in ')]}': stack.pop()
        elif c == ',' and not stack:
            result.append(norm(text[start:i])); start = i + 1
    result.append(norm(text[start:])); return result

def calls(text, pattern):
    text = clean(text)
    for match in re.finditer(r'\b(' + pattern + r')\s*\(', text):
        start = text.index('(', match.start()); end = balanced(text, start)
        yield match[1], split_args(text[start+1:end]), norm(text[match.start():end+1]), match.start()

def function(text, name):
    text = clean(text)
    for match in re.finditer(r'\b' + re.escape(name) + r'\s*\(', text):
        start = text.index('(', match.start()); end = balanced(text, start)
        pos = end + 1
        while pos < len(text) and text[pos].isspace(): pos += 1
        qualifier=re.match(r'(?:noexcept|const)\b\s*',text[pos:])
        if qualifier: pos+=qualifier.end()
        if pos < len(text) and text[pos] == '{':
            return text[pos:balanced(text, pos, '{', '}')+1]
    raise AssertionError('missing function ' + name)

def property_blocks(text, name):
    body = function(text, name)
    matches = list(re.finditer(r'case\s+(PROP_[A-Z0-9_]+)\s*:', body))
    return {m[1]: body[m.end():matches[i+1].start() if i+1 < len(matches) else len(body)]
            for i, m in enumerate(matches)}

def config_members(text):
    result = {}
    for macro, args, expression, _ in calls(text, r'GIMP_CONFIG_(?:INSTALL_)?PROP_[A-Z0-9_]+'):
        if len(args) < 6 or not args[2].startswith('"'): continue
        name = args[2].strip('"'); typ = macro.split('PROP_')[1]
        default = 'NULL' if typ in ('OBJECT', 'FONT') else args[-2]
        result[name] = dict(type=typ, default=default, flags=args[-1], expression=expression,
                            arguments=args, macro=macro)
    return result

def function_ranges(text):
    """Definition ranges for the ordinary C files in the syntax census."""
    stripped=clean(text); result=[]
    for match in re.finditer(r'^([a-zA-Z_][a-zA-Z_0-9]*)\s*\(', stripped, re.M):
        start=stripped.index('(',match.start()); end=balanced(stripped,start)
        pos=end+1
        while pos<len(stripped) and stripped[pos].isspace(): pos+=1
        if stripped[pos:pos+1]!='{': continue
        finish=balanced(stripped,pos,'{','}')
        result.append((stripped.count('\n',0,match.start())+1,stripped.count('\n',0,finish)+1,match[1]))
    return result

def exact(actual, expected, label):
    if actual != expected:
        raise AssertionError(f'{label}: expected {expected!r}; got {actual!r}')
