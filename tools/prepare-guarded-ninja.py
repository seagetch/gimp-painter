#!/usr/bin/env python3
"""Prepare a fresh generator shadow and copied Ninja graph; never run either.

Only the pinned Meson 1.7.0 native executable-metadata schema is supported.
This helper does not make an untrusted build graph safe to execute.
"""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import io
import json
import os
from pathlib import Path
import pickle
import pickletools
import re
import stat
import struct
import subprocess


MAX_METADATA = 1024 * 1024
MAX_OPCODES = 50000
MODULE = "mesonbuild.utils.core"
MARKERS = ("meson-mkenums.py", "meson-enumgen.py", "meson-pdbgen.py", "meson-enumcode.py")
# No reducers, extension-cache lookups, persistent IDs, legacy object factories,
# out-of-band buffers, or arbitrary byte/numeric objects are needed here.
OPCODES = frozenset("PROTO FRAME STOP MARK NONE NEWTRUE NEWFALSE EMPTY_LIST APPEND "
                   "APPENDS EMPTY_DICT SETITEM SETITEMS EMPTY_TUPLE TUPLE TUPLE1 "
                   "TUPLE2 TUPLE3 EMPTY_SET ADDITEMS SHORT_BINUNICODE BINUNICODE "
                   "BINUNICODE8 MEMOIZE BINPUT LONG_BINPUT BINGET LONG_BINGET "
                   "STACK_GLOBAL GLOBAL NEWOBJ BUILD".split())


class MetadataError(ValueError):
    """Unsupported or invalid metadata: stop without publishing a graph."""


class _Record:
    __slots__ = ("state",)

    def __setstate__(self, state):
        if type(state) is not dict or hasattr(self, "state"):
            raise MetadataError("Expected one plain record state")
        self.state = state


class _Executable(_Record):
    __slots__ = ()


class _Environment(_Record):
    __slots__ = ()


# These are inert tokens, never the real Meson functions.
ENV_METHODS = {"EnvironmentVariables." + name: object()
               for name in ("_set", "_append", "_prepend")}
GLOBALS = {"ExecutableSerialisation": _Executable,
           "EnvironmentVariables": _Environment, **ENV_METHODS}


class _MetadataReader(pickle.Unpickler):
    def find_class(self, module, name):
        if module != MODULE or name not in GLOBALS:
            raise MetadataError(f"Unsupported metadata global: {module}.{name}")
        return GLOBALS[name]

    def persistent_load(self, pid):
        raise MetadataError("Persistent IDs are unsupported")


def _strings(value, container=list):
    return type(value) is container and all(type(v) is str and "\0" not in v for v in value)


def validate_metadata(data):
    if type(data) is not _Executable or not hasattr(data, "state"):
        raise MetadataError("Expected ExecutableSerialisation")
    fields = {"cmd_args", "env", "exe_wrapper", "workdir", "extra_paths", "capture",
              "feed", "tag", "verbose", "installdir_map", "pickled", "skip_if_destdir",
              "subproject", "dry_run"}
    s = data.state
    if set(s) != fields:
        raise MetadataError("Unsupported executable fields")
    if not _strings(s["cmd_args"]) or not s["cmd_args"]:
        raise MetadataError("Expected nonempty string command arguments")
    if s["exe_wrapper"] is not None:
        raise MetadataError("Executable wrappers are unsupported")
    for name in ("workdir", "capture", "feed", "tag"):
        if s[name] is not None and (type(s[name]) is not str or "\0" in s[name]):
            raise MetadataError(f"Invalid {name}")
    if type(s["subproject"]) is not str or "\0" in s["subproject"]:
        raise MetadataError("Invalid subproject")
    for name in ("verbose", "pickled", "skip_if_destdir", "dry_run"):
        if type(s[name]) is not bool:
            raise MetadataError(f"Invalid {name}")
    if s["extra_paths"] is not None and not _strings(s["extra_paths"]):
        raise MetadataError("Invalid extra_paths")
    mapping = s["installdir_map"]
    if mapping is not None and (type(mapping) is not dict or
                               not _strings(list(mapping)) or not _strings(list(mapping.values()))):
        raise MetadataError("Invalid installdir_map")
    env = s["env"]
    if env is None:
        _check_expanded_size(data)
        return
    if type(env) is not _Environment or not hasattr(env, "state"):
        raise MetadataError("Invalid environment record")
    e = env.state
    if set(e) != {"envvars", "varnames", "unset_vars", "can_use_env"}:
        raise MetadataError("Unsupported environment fields")
    if (not _strings(e["varnames"], set) or not _strings(e["unset_vars"], set)
            or type(e["can_use_env"]) is not bool or type(e["envvars"]) is not list):
        raise MetadataError("Invalid environment state")
    for operation in e["envvars"]:
        if (type(operation) is not tuple or len(operation) != 4
                or not any(operation[0] is token for token in ENV_METHODS.values())
                or not _strings([operation[1], operation[3]]) or not _strings(operation[2])):
            raise MetadataError("Unsupported environment operation")
    _check_expanded_size(data)


def _check_expanded_size(data):
    # Memo references can multiply a small input into huge argument/output data.
    # This walk is only reached after the shallow, acyclic schema is validated.
    remaining = MAX_METADATA
    pending = [data]
    while pending:
        value = pending.pop()
        remaining -= 64  # Conservative wire/container overhead per item.
        if type(value) is str:
            remaining -= len(value.encode("utf-8"))
        elif type(value) in (_Executable, _Environment):
            pending.append(value.state)
        elif type(value) is dict:
            pending.extend(value.keys())
            pending.extend(value.values())
        elif type(value) in (list, tuple, set):
            pending.extend(value)
        if remaining < 0:
            raise MetadataError("Expanded metadata exceeds size limit")


def _replacement_size(text, old, new):
    return len(text.encode("utf-8")) + text.count(old) * (len(new.encode("utf-8")) - len(old.encode("utf-8")))


def replace_source(text, old, new):
    """Refuse ambiguous prefix matches rather than rewriting adjacent paths."""
    for match in re.finditer(re.escape(old), text):
        before = text[match.start() - 1:match.start()] if match.start() else ""
        after = text[match.end():match.end() + 1]
        if (before and (before.isalnum() or before in "/_.-")) or (after and after not in "/ \t\r\n'\";"):
            raise MetadataError("Ambiguous source-path prefix in generator command")
    size = _replacement_size(text, old, new)
    if size > MAX_METADATA:
        raise MetadataError("Redirected argument exceeds size limit")
    return text.replace(old, new)


def redirect_arguments(args, source, shadow):
    if sum(64 + _replacement_size(arg, source, shadow) for arg in args) > MAX_METADATA:
        raise MetadataError("Expanded redirected arguments exceed size limit")
    return [replace_source(arg, source, shadow) for arg in args]


def load_metadata(raw):
    """Read bounded data into inert records, never importing a metadata global."""
    if not raw or len(raw) > MAX_METADATA:
        raise MetadataError("Empty or oversized executable metadata")
    try:
        count = 0
        for opcode, arg, position in pickletools.genops(raw):
            count += 1
            if count > MAX_OPCODES or opcode.name not in OPCODES:
                raise MetadataError("Unsupported opcode or metadata complexity")
            if count == 1 and (opcode.name != "PROTO" or arg not in (4, 5)):
                raise MetadataError("Only pickle protocols 4 and 5 are supported")
            if opcode.name == "PROTO" and count != 1:
                raise MetadataError("Unexpected protocol marker")
            if opcode.name in {"BINPUT", "LONG_BINPUT", "BINGET", "LONG_BINGET"} and arg >= MAX_OPCODES:
                raise MetadataError("Oversized memo index")
            if opcode.name == "FRAME" and arg > MAX_METADATA:
                raise MetadataError("Oversized frame")
        if opcode.name != "STOP" or position + 1 != len(raw):
            raise MetadataError("Missing stop or trailing metadata")
        data = _MetadataReader(io.BytesIO(raw)).load()
        validate_metadata(data)
        return data
    except (ValueError, TypeError, AttributeError, KeyError, IndexError,
            EOFError, pickle.UnpicklingError, OverflowError, RecursionError) as error:
        raise MetadataError(f"Rejected executable metadata: {error}") from error


def dump_metadata(data):
    """Emit only the validated schema, with fixed Meson names and no reducers."""
    validate_metadata(data)

    def encode(value):
        if value is None:
            return b"N"
        if type(value) is bool:
            return b"\x88" if value else b"\x89"
        if type(value) is str:
            encoded = value.encode("utf-8")
            return b"X" + struct.pack("<I", len(encoded)) + encoded
        if type(value) is dict:
            return b"}(" + b"".join(encode(k) + encode(v) for k, v in value.items()) + b"u"
        if type(value) is list:
            return b"](" + b"".join(map(encode, value)) + b"e"
        if type(value) is tuple:
            return b"(" + b"".join(map(encode, value)) + b"t"
        if type(value) is set:
            return b"\x8f(" + b"".join(map(encode, sorted(value))) + b"\x90"
        for name, token in ENV_METHODS.items():
            if value is token:
                return encode(MODULE) + encode(name) + b"\x93"
        name = "ExecutableSerialisation" if type(value) is _Executable else "EnvironmentVariables"
        return encode(MODULE) + encode(name) + b"\x93)\x81" + encode(value.state) + b"b"

    raw = b"\x80\x04" + encode(data) + b"."
    if len(raw) > MAX_METADATA:
        raise MetadataError("Redirected metadata exceeds size limit")
    return raw


@contextmanager
def open_directory(path):
    """Walk every component without following symlinks (Linux/POSIX only)."""
    path = Path(path)
    if not path.is_absolute() or ".." in path.parts:
        raise MetadataError("Expected an absolute path without parent traversal")
    fd = os.open("/", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
        yield fd
    finally:
        os.close(fd)


def read_regular(path, limit=MAX_METADATA):
    with open_directory(path.parent) as directory:
        fd = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
        with os.fdopen(fd, "rb") as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
                raise MetadataError(f"Not a bounded regular file: {path}")
            raw = stream.read(limit + 1)
            if len(raw) > limit:
                raise MetadataError(f"File grew beyond limit: {path}")
            return raw


def prepare(source, build, output, expected_recipes=17):
    # Ninja and shell escaping is not reimplemented by this pinned helper.
    if any(not re.fullmatch(r"/[A-Za-z0-9_./-]+", str(p)) for p in (source, build, output)):
        raise MetadataError("Use absolute simple paths without whitespace or shell/Ninja metacharacters")
    for path in (source, build, output.parent):
        with open_directory(path):
            pass
    if source == build or source in build.parents or build in source.parents:
        raise MetadataError("Source and build must be separate, non-nested trees")
    if any(output == p or p in output.parents or output in p.parents for p in (source, build)):
        raise MetadataError("Output must be separate from source and build trees")
    if output.exists() or output.is_symlink():
        raise MetadataError("Output directory must be new")
    if expected_recipes < 1:
        raise MetadataError("Expected recipe count must be positive")
    original = read_regular(build / "build.ninja", 64 * 1024 * 1024).decode()
    shadow = output / "generator-source-http"
    private = output / "guarded-meson-private"
    redirects, serialized, audit = {}, {}, []
    with open_directory(build / "meson-private") as directory:
        names = sorted(os.listdir(directory))
    for name in names:
        if not (name.startswith("meson_exe") and name.endswith(".dat")):
            continue
        path = build / "meson-private" / name
        data = load_metadata(read_regular(path))
        old_args = data.state["cmd_args"]
        if not any(Path(arg).name in MARKERS for arg in old_args):
            continue
        env = data.state["env"]
        if env is not None and any(env.state[key] for key in ("envvars", "varnames", "unset_vars")):
            raise MetadataError("Unexpected generator-specific environment")
        data.state["cmd_args"] = redirect_arguments(old_args, str(source), str(shadow))
        if data.state["cmd_args"] == old_args or any(str(source) in arg for arg in data.state["cmd_args"]):
            raise MetadataError("Generator source paths were not redirected")
        target = private / name
        serialized[name] = dump_metadata(data)
        redirects[str(path)] = str(target)
        audit.append({"original": str(path), "guarded": str(target),
                      "original_arguments": old_args, "guarded_arguments": data.state["cmd_args"]})
    rows, lines = [], []
    relative_source, relative_shadow = os.path.relpath(source, build), os.path.relpath(shadow, build)
    for line in original.splitlines(keepends=True):
        if line.lstrip().startswith("COMMAND =") and (any(m in line for m in MARKERS) or any(p in line for p in redirects)):
            changed = replace_source(line, str(source), str(shadow))
            changed = replace_source(changed, relative_source, relative_shadow)
            for path, target in redirects.items():
                changed = changed.replace(path, target)
            if changed == line or str(source) in changed or relative_source in changed:
                raise MetadataError("Command was not fully redirected")
            rows.append({"original": line.rstrip(), "guarded": changed.rstrip()})
            line = changed
        lines.append(line)
    if len(rows) != expected_recipes:
        raise MetadataError(f"Expected {expected_recipes} redirected recipes, found {len(rows)}")
    guarded = "".join(lines)
    if "build " + str(output / "http.ninja") + ":" in guarded:
        raise MetadataError("Unexpected regeneration edge for copied graph")
    source_names = subprocess.check_output(["git", "ls-files", "-z"], cwd=source).decode().split("\0")
    with open_directory(output.parent) as directory:
        os.mkdir(output.name, mode=0o700, dir_fd=directory)
    shadow.mkdir()
    private.mkdir()
    manifest = {}
    for name in source_names:
        if not name or name.startswith(("migration/", "po", "devel-docs/", "build/", "gimp-data")):
            continue
        if Path(name).is_absolute() or ".." in Path(name).parts:
            raise MetadataError("Unexpected tracked source path")
        src = source / name
        if src.is_dir() and not src.is_symlink():
            continue  # Git submodule directories are not generator inputs.
        raw = read_regular(src, 64 * 1024 * 1024)
        dst = shadow / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(raw)
        dst.chmod(stat.S_IMODE(src.stat(follow_symlinks=False).st_mode) & 0o777)
        manifest[name] = hashlib.sha256(raw).hexdigest()
    for name, raw in serialized.items():
        (private / name).write_bytes(raw)
    (output / "generator-shadow-inputs.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (output / "generator-command-audit.json").write_text(json.dumps({
        "source": str(source), "shadow": str(shadow), "build": str(build),
        "redirected_recipes": len(rows), "commands": rows, "redirected_pickles": audit,
        "source_ninja_sha256": hashlib.sha256(original.encode()).hexdigest(),
        "guarded_ninja_sha256": hashlib.sha256(guarded.encode()).hexdigest(),
        "copied_source_files": len(manifest),
    }, indent=2) + "\n")
    # Publish the graph last. A failed preparation leaves no runnable output graph.
    (output / "http.ninja").write_text(guarded)
    return len(rows), len(manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--expected-recipes", type=int, default=17)
    args = parser.parse_args()
    try:
        rows, files = prepare(args.source, args.build, args.output, args.expected_recipes)
    except (MetadataError, OSError, UnicodeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Preparation refused: {error}\n")
    print(f"Prepared {rows} redirected recipes and {files} generator inputs; no commands executed")


if __name__ == "__main__":
    main()
