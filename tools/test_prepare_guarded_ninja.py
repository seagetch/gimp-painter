#!/usr/bin/env python3
"""Non-exploit unit fixtures only: no Meson installation or native build."""

import contextlib
import importlib.util
import json
import os
from pathlib import Path
import pickle
import struct
import sys
import tempfile
import types
import unittest
from unittest import mock


SPEC = importlib.util.spec_from_file_location("guarded_ninja", Path(__file__).with_name("prepare-guarded-ninja.py"))
guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(guard)


@contextlib.contextmanager
def meson_schema_fixture():
    """Locally authored inert classes matching Meson 1.7.0 instance state.

    They are deliberately not imported from a restored/downloaded package.
    Ordinary pickle.dump/load with these classes checks wire compatibility.
    """
    modules = {name: types.ModuleType(name) for name in ("mesonbuild", "mesonbuild.utils", guard.MODULE)}
    core = modules[guard.MODULE]
    executable = type("ExecutableSerialisation", (), {"__module__": guard.MODULE})
    environment = type("EnvironmentVariables", (), {"__module__": guard.MODULE})
    def set_fixture(*args):
        return None
    set_fixture.__module__ = guard.MODULE
    set_fixture.__qualname__ = "EnvironmentVariables._set"
    environment._set = staticmethod(set_fixture)
    core.ExecutableSerialisation = executable
    core.EnvironmentVariables = environment
    modules["mesonbuild"].utils = modules["mesonbuild.utils"]
    modules["mesonbuild.utils"].core = core
    with mock.patch.dict(sys.modules, modules):
        yield executable, environment


def fixture(args=None, env=True, **changes):
    with meson_schema_fixture() as (executable, environment):
        data = executable()
        data.__dict__.update(cmd_args=args or ["/usr/bin/python3", "/source/tools/meson-mkenums.py", "#include <gio/gio.h>\n"],
                             env=None, exe_wrapper=None, workdir="/build", extra_paths=[], capture="output.h",
                             feed=None, tag=None, verbose=False, installdir_map=None, pickled=False,
                             skip_if_destdir=False, subproject="", dry_run=False)
        if env:
            data.env = environment()
            data.env.__dict__.update(envvars=[], varnames=set(), unset_vars=set(), can_use_env=True)
        data.__dict__.update(changes)
        return data


def dumps(data, protocol=4):
    # Restore exactly these fixture class identities while serializing.
    core = types.ModuleType(guard.MODULE)
    core.ExecutableSerialisation = type(data)
    if data.env is not None and type(data.env).__name__ == "EnvironmentVariables":
        core.EnvironmentVariables = type(data.env)
    with meson_schema_fixture(), mock.patch.dict(sys.modules, {guard.MODULE: core}):
        return pickle.dumps(data, protocol=protocol)


class MetadataTests(unittest.TestCase):
    def test_protocol_four_and_five_round_trip(self):
        for protocol in (4, 5):
            with self.subTest(protocol=protocol):
                original = fixture(installdir_map={"prefix": "/opt/fixture"}, feed="input.txt", tag="devel", verbose=True)
                raw = guard.dump_metadata(guard.load_metadata(dumps(original, protocol)))
                with meson_schema_fixture():
                    # Only helper-generated, validated bytes are unpickled here.
                    result = pickle.loads(raw)
                self.assertEqual(result.env.__dict__, original.env.__dict__)
                result.env = original.env
                self.assertEqual(result.__dict__, original.__dict__)

    def test_none_environment_and_optional_fields(self):
        data = guard.load_metadata(dumps(fixture(env=False, workdir=None, extra_paths=None, capture=None)))
        self.assertIsNone(data.state["env"])

    def test_ordinary_environment_operation_round_trip(self):
        original = fixture()
        original.env.envvars = [(original.env._set, "FIXTURE", ["value"], ":")]
        original.env.varnames = {"FIXTURE"}
        raw = guard.dump_metadata(guard.load_metadata(dumps(original)))
        with meson_schema_fixture():
            result = pickle.loads(raw)
        self.assertEqual(result.env.envvars[0][1:], ("FIXTURE", ["value"], ":"))
        self.assertEqual(result.env.varnames, {"FIXTURE"})

    def test_all_retained_generator_argument_arrays(self):
        path = Path(__file__).resolve().parent.parent / "migration/tests/guarded-ninja/arguments.json"
        arrays = json.loads(path.read_text())
        self.assertEqual(len(arrays), 14)
        self.assertEqual(sum("\n" in arg for args in arrays for arg in args), 17)
        for protocol in (4, 5):
            for index, args in enumerate(arrays):
                with self.subTest(protocol=protocol, generator=index):
                    data = guard.load_metadata(dumps(fixture(args=args), protocol))
                    data.state["cmd_args"] = guard.redirect_arguments(data.state["cmd_args"], "/fixture/source", "/fixture/shadow")
                    with meson_schema_fixture():
                        decoded = pickle.loads(guard.dump_metadata(data))
                    self.assertEqual(decoded.cmd_args, [arg.replace("/fixture/source", "/fixture/shadow") for arg in args])

    def test_memo_expansion_and_replacement_budgets(self):
        data = fixture(args=["ordinary repeated value " * 30] * 100)
        raw = dumps(data)
        self.assertLess(len(raw), 4096)
        with mock.patch.object(guard, "MAX_METADATA", 4096), self.assertRaisesRegex(guard.MetadataError, "Expanded metadata"):
            guard.load_metadata(raw)
        with mock.patch.object(guard, "MAX_METADATA", 256):
            with self.assertRaisesRegex(guard.MetadataError, "Expanded redirected"):
                guard.redirect_arguments(["/s/file"] * 3, "/s", "/" + "long" * 20)
            with self.assertRaisesRegex(guard.MetadataError, "Redirected argument"):
                guard.replace_source("/s/file", "/s", "/" + "long" * 100)

    def test_source_prefix_collision(self):
        for value in ("/source-other/file", "/prefix/source/file", "../source-other/file"):
            old = "../source" if value.startswith("..") else "/source"
            with self.subTest(value=value), self.assertRaisesRegex(guard.MetadataError, "Ambiguous source-path"):
                guard.replace_source(value, old, "/shadow")

    def test_unknown_global_is_rejected_without_import(self):
        raw = b"\x80\x04cfixture_missing_module\nUnexpectedRecord\n."
        with mock.patch("builtins.__import__", side_effect=AssertionError("No imports allowed")):
            with self.assertRaisesRegex(guard.MetadataError, "Unsupported metadata global"):
                guard.load_metadata(raw)

    def test_forbidden_opcodes_rejected_without_loading(self):
        # Single malformed opcode records, not executable payloads.
        samples = {"REDUCE": b"R", "EXT1": b"\x82\x01", "EXT2": b"\x83\x01\x00",
                   "EXT4": b"\x84\x01\x00\x00\x00", "PERSID": b"Pfixture\n",
                   "BINPERSID": b"Q", "NEXT_BUFFER": b"\x97", "NEWOBJ_EX": b"\x92",
                   "INST": b"ifixture\nFixture\n", "OBJ": b"o"}
        for name, body in samples.items():
            with self.subTest(opcode=name), mock.patch.object(guard, "_MetadataReader", side_effect=AssertionError("Reader reached")):
                with self.assertRaisesRegex(guard.MetadataError, "Unsupported opcode"):
                    guard.load_metadata(b"\x80\x04" + body + b".")

    def test_wrong_root_type(self):
        with self.assertRaisesRegex(guard.MetadataError, "Expected Executable"):
            guard.load_metadata(pickle.dumps({"cmd_args": ["ordinary"]}, protocol=4))

    def test_unknown_or_missing_fields(self):
        for kind in ("unknown", "missing"):
            data = fixture()
            if kind == "unknown":
                data.extra_field = "unexpected"
            else:
                del data.workdir
            with self.subTest(kind=kind), self.assertRaisesRegex(guard.MetadataError, "Unsupported executable fields"):
                guard.load_metadata(dumps(data))

    def test_invalid_state_types(self):
        for changes in ({"cmd_args": "not a list"}, {"cmd_args": []}, {"cmd_args": [None]},
                        {"verbose": "yes"}, {"extra_paths": [False]}, {"workdir": []},
                        {"installdir_map": {"prefix": []}}, {"exe_wrapper": "wrapper"},
                        {"cmd_args": ["nul\0byte"]}):
            with self.subTest(changes=changes), self.assertRaises(guard.MetadataError):
                guard.load_metadata(dumps(fixture(**changes)))

    def test_cyclic_container_is_rejected(self):
        data = fixture()
        data.cmd_args.append(data.cmd_args)
        with self.assertRaises(guard.MetadataError):
            guard.load_metadata(dumps(data))

    def test_environment_schema_rejected(self):
        for field, value in (("envvars", "not a list"), ("varnames", []), ("unset_vars", {None}),
                             ("can_use_env", "yes"), ("extra_field", None), ("envvars", [(None, "A", [], ":")])):
            data = fixture()
            setattr(data.env, field, value)
            with self.subTest(field=field), self.assertRaises(guard.MetadataError):
                guard.load_metadata(dumps(data))

    def test_truncation_trailing_bytes_and_old_protocol(self):
        raw = dumps(fixture())
        for sample in (raw[:-1], raw + b"trailing", raw + raw, b"", dumps(fixture(env=False), 3)):
            with self.subTest(size=len(sample)), self.assertRaises(guard.MetadataError):
                guard.load_metadata(sample)

    def test_limits_checked_before_loading(self):
        raw = dumps(fixture())
        with mock.patch.object(guard, "MAX_METADATA", len(raw) - 1), self.assertRaises(guard.MetadataError):
            guard.load_metadata(raw)
        with mock.patch.object(guard, "MAX_OPCODES", 3), self.assertRaises(guard.MetadataError):
            guard.load_metadata(raw)
        for opcode in (b"r", b"j"):
            with self.assertRaisesRegex(guard.MetadataError, "memo index"):
                guard.load_metadata(b"\x80\x04" + opcode + struct.pack("<I", 50001) + b".")
        with self.assertRaisesRegex(guard.MetadataError, "Oversized frame"):
            guard.load_metadata(b"\x80\x04\x95" + struct.pack("<Q", guard.MAX_METADATA + 1) + b"N.")


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source, self.build, self.output = (self.root / name for name in ("source", "build", "output"))
        self.source.mkdir()
        (self.source / "tools").mkdir()
        (self.source / "tools/meson-mkenums.py").write_text("# inert source fixture\n")
        self.build.mkdir()
        self.private = self.build / "meson-private"
        self.private.mkdir()
        self.metadata = self.private / "meson_exe_fixture.dat"
        self.original = fixture(args=["/usr/bin/python3", str(self.source / "tools/meson-mkenums.py"),
                                      str(self.source), '#include "fixture.h"\n'], workdir=str(self.build))
        self.metadata.write_bytes(dumps(self.original))
        self.ninja = self.build / "build.ninja"
        self.ninja.write_text(f"rule fixture\n COMMAND = meson --internal exe --unpickle {self.metadata}\n")
        self.git = mock.patch.object(guard.subprocess, "check_output", return_value=b"tools/meson-mkenums.py\0")
        self.git_mock = self.git.start()
        self.addCleanup(self.git.stop)

    def prepare(self):
        return guard.prepare(self.source, self.build, self.output, expected_recipes=1)

    def test_prepare_only_redirects_arguments_and_preserves_inputs(self):
        before_metadata, before_ninja = self.metadata.read_bytes(), self.ninja.read_bytes()
        self.assertEqual(self.prepare(), (1, 1))
        data = guard.load_metadata((self.output / "guarded-meson-private" / self.metadata.name).read_bytes())
        self.assertEqual(data.state["cmd_args"][-1], '#include "fixture.h"\n')
        self.assertIn(str(self.output / "generator-source-http"), data.state["cmd_args"])
        self.assertEqual(data.state["workdir"], str(self.build))
        self.assertEqual(data.state["capture"], self.original.capture)
        self.assertEqual(self.metadata.read_bytes(), before_metadata)
        self.assertEqual(self.ninja.read_bytes(), before_ninja)
        self.assertIn(str(self.output / "guarded-meson-private"), (self.output / "http.ninja").read_text())
        self.git_mock.assert_called_once_with(["git", "ls-files", "-z"], cwd=self.source)

    def test_direct_command_and_unselected_metadata(self):
        self.metadata.write_bytes(dumps(fixture(args=["ordinary-generator"], env=False)))
        self.ninja.write_text(f" COMMAND = python3 {self.source}/pdb/meson-pdbgen.py {self.source}\n")
        self.assertEqual(self.prepare(), (1, 1))
        self.assertEqual(list((self.output / "guarded-meson-private").iterdir()), [])
        self.assertIn(str(self.output / "generator-source-http"), (self.output / "http.ninja").read_text())

    def test_unselected_invalid_metadata_is_rejected(self):
        (self.private / "meson_exe_unselected.dat").write_bytes(pickle.dumps(["ordinary"], protocol=4))
        with self.assertRaises(guard.MetadataError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_generator_environment_mutations_are_rejected(self):
        self.original.env.unset_vars = {"ORDINARY_TEST_VARIABLE"}
        self.metadata.write_bytes(dumps(self.original))
        with self.assertRaisesRegex(guard.MetadataError, "generator-specific environment"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_metadata_symlink_is_rejected(self):
        other = self.root / "ordinary.dat"
        other.write_bytes(self.metadata.read_bytes())
        self.metadata.unlink()
        self.metadata.symlink_to(other)
        with self.assertRaises(OSError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_private_directory_symlink_is_rejected(self):
        moved = self.root / "private"
        self.private.rename(moved)
        self.private.symlink_to(moved, target_is_directory=True)
        with self.assertRaises(OSError):
            self.prepare()

    def test_ancestor_symlink_is_rejected(self):
        alias = self.root / "alias"
        alias.symlink_to(self.build, target_is_directory=True)
        with self.assertRaises(OSError):
            guard.read_regular(alias / "meson-private" / self.metadata.name)

    def test_metadata_directory_and_fifo_are_rejected(self):
        self.metadata.unlink()
        self.metadata.mkdir()
        with self.assertRaises((OSError, guard.MetadataError)):
            self.prepare()
        self.metadata.rmdir()
        os.mkfifo(self.metadata)
        with self.assertRaises(guard.MetadataError):
            self.prepare()

    def test_output_symlink_existing_directory_and_nested_output(self):
        self.output.symlink_to(self.source, target_is_directory=True)
        with self.assertRaises(guard.MetadataError):
            self.prepare()
        self.output.unlink()
        self.output.mkdir()
        with self.assertRaises(guard.MetadataError):
            self.prepare()
        with self.assertRaises(guard.MetadataError):
            guard.prepare(self.source, self.build, self.source / "nested", 1)

    def test_output_metacharacters_are_rejected(self):
        for name in ("space name", "with$dollar", "with;semicolon", "with'quote"):
            with self.subTest(name=name), self.assertRaises(guard.MetadataError):
                guard.prepare(self.source, self.build, self.root / name, 1)

    def test_nested_source_build_trees_are_rejected(self):
        nested = self.source / "build"
        nested.mkdir()
        with self.assertRaisesRegex(guard.MetadataError, "non-nested"):
            guard.prepare(self.source, nested, self.output, 1)

    def test_source_symlink_cannot_publish_graph(self):
        src = self.source / "tools/meson-mkenums.py"
        src.unlink()
        src.symlink_to(self.ninja)
        with self.assertRaises(OSError):
            self.prepare()
        self.assertFalse((self.output / "http.ninja").exists())

    def test_path_traversal_relative_path_and_unexpected_count(self):
        with self.assertRaises(guard.MetadataError):
            with guard.open_directory(self.root / "build/../source"):
                pass
        with self.assertRaises(guard.MetadataError):
            with guard.open_directory(Path("relative")):
                pass
        with self.assertRaisesRegex(guard.MetadataError, "Expected 17"):
            guard.prepare(self.source, self.build, self.output)
        self.assertFalse(self.output.exists())

    def test_large_input_and_unexpected_source_name(self):
        with mock.patch.object(guard, "MAX_METADATA", 16), self.assertRaises(guard.MetadataError):
            self.prepare()
        self.git_mock.return_value = b"../outside\0"
        with self.assertRaises(guard.MetadataError):
            self.prepare()
        self.assertFalse((self.output / "http.ninja").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
