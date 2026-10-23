#!/usr/bin/env python3
"""Verify header-owned C linkage against the real native application archives.

Run in the configured build environment. Output is external to the source tree:
  python3 tools/check_painter_header_linkage.py --build-dir BUILD \
    --http-build-dir HTTP_BUILD --output-dir OUTPUT --report REPORT.json

No production declarations are wrapped by a probe-side extern-C adapter. The
four named upstream declaration-only APIs are checked by C/C++ object symbols,
but cannot be linked without inventing implementations. Every other API must
resolve from production objects. This is a native ABI contract, not GUI tests.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "migration/tests/header-linkage"
BASELINE = "ec2035a7302ff074cb8df9cc396e450826c4020d"
NAVIGATION = "app/display/gimppainternavigation"
DECLARATION_ONLY = {
    "gimp_get_display_by_id": "app/core/gimp-gui.h",
    "gimp_get_display_id": "app/core/gimp-gui.h",
    "gimp_item_resize_to_image": "app/core/gimpitem.h",
    "gimp_editor_popup_menu": "app/widgets/gimpeditor.h",
}
SDK = ["gimp_color_profile_get_type", "gimp_color_selector_get_type",
       "gimp_frame_get_type", "gimp_int_combo_box_get_type"]
XCF = ["app/xcf/painter-xcf-load.h", "app/xcf/painter-xcf-preserve.h"]


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def include(path):
    return '#include "' + path.removeprefix("app/") + '"\n'


class Verification:
    def __init__(self, args):
        self.args = args
        self.output = args.output_dir.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.report = {"status": "RUNNING", "scope": "native C11/C++14 header-owned linkage",
                       "commands": [], "results": {}, "limitations": [
                           "Four unchanged upstream declarations have no implementation; their C/C++ declaration ABI is checked, but they are excluded from the production link.",
                           "Native platform only; no GUI, listener or token-reader calls, and no feature-completion claim.",
                           "Upstream ignored scalar-return qualifier warnings remain visible via -Wno-error=ignored-qualifiers."]}
        self.input_hashes = {}
        self.flags = ["-Wall", "-Wextra", "-Werror", "-Wno-error=ignored-qualifiers",
                      "-O0", "-I" + str(self.output), "-I" + str(FIXTURES),
                      "-I" + str(args.build_dir.resolve()), "-I" + str(ROOT),
                      "-I" + str(ROOT / "app")]
        self.flags += shlex.split(self.run("pkg-cflags", ["pkg-config", "--cflags",
                                  "gegl-0.4", "gtk+-3.0", "gexiv2", "libsoup-3.0"]).stdout)

    def save(self):
        self.args.report.parent.mkdir(parents=True, exist_ok=True)
        self.args.report.write_text(json.dumps(self.report, indent=2) + "\n")

    def snapshot(self, paths):
        """Bind results to the bytes actually consumed, including the running tool."""
        for path in paths:
            path = Path(path).resolve()
            digest = sha256(path)
            if path in self.input_hashes and self.input_hashes[path] != digest:
                raise RuntimeError("Input changed during verification: " + str(path))
            self.input_hashes[path] = digest
        self.report["input_sha256"] = {
            str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path): digest
            for path, digest in sorted(self.input_hashes.items())}

    def verify_unchanged_inputs(self):
        changed = [str(path) for path, digest in self.input_hashes.items()
                   if not path.is_file() or sha256(path) != digest]
        if changed:
            raise RuntimeError("Inputs changed during verification: " + ", ".join(changed))
        self.report["inputs_unchanged_at_end"] = True

    def run(self, label, command, cwd=ROOT, expected_failure=False, env=None):
        started = time.monotonic()
        result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        log = self.output / (label + ".log")
        log.write_text(result.stdout + result.stderr)
        self.report["commands"].append({"label": label, "argv": [str(x) for x in command],
            "cwd": str(cwd), "exit_code": result.returncode,
            "seconds": round(time.monotonic() - started, 3),
            "output_sha256": sha256(log), "log": log.name})
        if (result.returncode == 0) == expected_failure:
            raise RuntimeError(label + " unexpected exit " + str(result.returncode) +
                               "\n" + result.stderr[-12000:])
        return result

    def compiler(self, cpp):
        return shlex.split(os.environ.get("CXX" if cpp else "CC", "c++" if cpp else "cc"))

    def compile(self, label, body, cpp=False, mode="object", extra=None):
        source = self.output / (label + (".cpp" if cpp else ".c"))
        source.write_text(body)
        command = self.compiler(cpp) + ["-std=c++14" if cpp else "-std=c11"]
        command += (extra or []) + self.flags
        target = self.output / (label + ".o")
        if mode == "syntax":
            command += ["-fsyntax-only", source]
        elif mode == "preprocess":
            command += ["-E", "-P", source]
        else:
            command += ["-c", source, "-o", target]
        result = self.run(label, command)
        self.report["commands"][-1]["source_sha256"] = sha256(source)
        return result.stdout if mode == "preprocess" else target

    def undefined(self, path, label):
        output = self.run(label, ["nm", "--undefined-only", "--format=posix", path]).stdout
        return {line.split()[0] for line in output.splitlines() if line.strip()}

    def verify_scope(self):
        self.snapshot([__file__, self.args.scope,
                       ROOT / "migration/tests/painter-headers/prologue.h",
                       ROOT / "migration/tests/painter-headers/config-macros.c"] +
                      [p for p in FIXTURES.iterdir() if p.is_file()])
        self.scope = json.loads(self.args.scope.read_text())
        scope = self.scope
        if scope["baseline_commit"] != BASELINE:
            raise RuntimeError("Unexpected immutable baseline")
        counts = scope["counts"]
        header_paths = {ROOT / h["path"] for h in scope["headers"]}
        header_paths.update(ROOT / r["target"] for r in scope["routes"])
        header_paths.update(ROOT / path for path in (
            "app/xcf/xcf-private.h", "libgimpbase/gimpbase.h", "libgimpmath/gimpmath.h",
            "libgimpcolor/gimpcolor.h", "libgimpconfig/gimpconfig.h", "libgimpwidgets/gimpwidgets.h"))
        self.snapshot(header_paths | {self.args.build_dir / "config.h",
                                     self.args.build_dir / "libgimpbase/gimpversion.h",
                                     self.args.http_build_dir / "config.h"})
        required = {"routes": 103, "unique_route_targets": 101,
                    "app_closure_headers": 134, "function_candidates": 1762,
                    "new_production_headers": 42, "excluded_test_headers": 7,
                    "modified_existing_headers": 33}
        if any(counts[key] != value for key, value in required.items()):
            raise RuntimeError("Scope count mismatch: " + str(counts))
        self.symbols = sorted({f["symbol"] for h in scope["headers"] for f in h["functions"]})
        if len(self.symbols) != 1762 or {"gpointer", "guint32"} & set(self.symbols):
            raise RuntimeError("Function candidate inventory contains duplicates or parser false positives")
        headers = {h["path"]: h for h in scope["headers"]}
        baseline_paths = sorted(set(DECLARATION_ONLY.values()))
        baseline_blobs = self.run("baseline-exception-blobs", ["git", "rev-parse"] +
            [BASELINE + ":" + path for path in baseline_paths]).stdout.splitlines()
        if baseline_blobs != [headers[path]["baseline_blob"] for path in baseline_paths]:
            raise RuntimeError("Exception inventory does not match immutable Git objects")
        exceptions = []
        for symbol, path in DECLARATION_ONLY.items():
            entry = headers[path]
            if entry["baseline_blob"] != entry["upstream_blob"] or symbol not in self.symbols:
                raise RuntimeError("Declaration-only exception is not unchanged pinned upstream: " + symbol)
            exceptions.append({"symbol": symbol, "header": path,
                               "baseline_and_upstream_blob": entry["baseline_blob"]})
        self.report["inventory"] = {"sha256": sha256(self.args.scope), "counts": required,
                                    "baseline_commit": BASELINE,
                                    "upstream_commit": scope["upstream_commit"]}
        self.report["declaration_only_baseline_limitations"] = exceptions
        original = (ROOT / "migration/tests/painter-headers/prologue.h").read_text()
        wrapper = '#ifdef __cplusplus\nextern "C" {\n#endif\n'
        if original.count(wrapper) != 1:
            raise RuntimeError("Expected one known historical prelude adapter")
        self.prelude = original.replace(wrapper, "")
        if 'extern "C"' in self.prelude or "G_BEGIN_DECLS" in self.prelude:
            raise RuntimeError("Prelude still contains a linkage adapter")
        (self.output / "prelude.h").write_text(self.prelude)
        self.report["prelude"] = {"source_sha256": hashlib.sha256(original.encode()).hexdigest(),
                                  "unwrapped_sha256": sha256(self.output / "prelude.h")}
        for cpp in (False, True):
            self.run("compiler-version-" + ("cpp" if cpp else "c"), self.compiler(cpp) + ["--version"])

    def syntax(self):
        targets = sorted({r["target"] for r in self.scope["routes"]})
        for i, target in enumerate(targets):
            body = self.prelude + include(target) * 2
            for cpp in (False, True):
                self.compile("route-%03d-%s" % (i, "cpp" if cpp else "c"), body, cpp, "syntax")
            print("double include OK:", target, flush=True)
        for i, target in enumerate(XCF):
            body = self.prelude + include("app/xcf/xcf-private.h") + include(target) * 2
            for cpp in (False, True):
                self.compile("xcf-internal-%d-%s" % (i, "cpp" if cpp else "c"), body, cpp, "syntax")
        macros = (ROOT / "migration/tests/painter-headers/config-macros.c").read_text()
        if len(set(re.findall(r"\bGIMP_CONFIG_PROP_\w+\s*\(", macros))) != 18:
            raise RuntimeError("Configuration macro fixture no longer covers all 18 macros")
        for cpp in (False, True):
            self.compile("config-macros-" + ("cpp" if cpp else "c"), self.prelude + macros,
                         cpp, "syntax")
        self.report["results"]["double_include"] = {
            "routes": len(self.scope["routes"]), "unique_targets": len(targets),
            "language_compiles": 2 * len(targets), "xcf_internal_compiles": 4,
            "configuration_macros_per_language": 18, "outer_linkage_adapter": False}

    def references(self):
        # All real application types and all route headers; XCF private types must
        # precede the conditional declarations. Public-only routes ran above.
        body = self.prelude + include("app/xcf/xcf-private.h")
        body += "".join(include(p) for p in sorted({r["target"] for r in self.scope["routes"]}))
        expected = set(self.symbols + SDK)
        objects = []
        for cpp in (False, True):
            language = "cpp" if cpp else "c"
            active = self.compile("active-" + language, body, cpp, "preprocess")
            # One regex pass over each preprocessed stream, never one full-text
            # scan per candidate (the 1,762-name inventory is a set lookup).
            active_names = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", active))
            missing = expected - active_names
            if missing:
                raise RuntimeError("Inactive/missing declarations: " + ", ".join(sorted(missing)))
            refs = body + '\n#include "probe-api.h"\n'
            refs += "PainterLinkageFunction volatile painter_" + language + "_references[] = {\n"
            refs += "".join("  (PainterLinkageFunction) &" + name + ",\n" for name in sorted(expected))
            refs += "};\n"
            obj = self.compile("declaration-refs-" + language, refs, cpp)
            undefined = self.undefined(obj, "declaration-nm-" + language)
            if not expected <= undefined:
                raise RuntimeError("Missing unmangled declaration symbols in " + language + ": " +
                                   ", ".join(sorted(expected - undefined)))
            if any(name.startswith("_Z") for name in undefined):
                raise RuntimeError("Unexpected mangled declaration symbol in " + language)
            native = expected - DECLARATION_ONLY.keys()
            native_refs = body + '\n#include "probe-api.h"\n'
            native_refs += "PainterLinkageFunction volatile painter_" + language + "_references[] = {\n"
            native_refs += "".join("  (PainterLinkageFunction) &" + name + ",\n" for name in sorted(native))
            native_refs += "};\n"
            if not cpp:
                native_refs += "const size_t painter_reference_count = " + str(len(native)) + ";\n"
            objects.append(self.compile("native-refs-" + language, native_refs, cpp))
        self.report["results"]["declaration_abi"] = {"application_functions_per_language": 1762,
            "sdk_representatives_per_language": SDK, "all_symbols_unmangled": True,
            "native_application_functions": 1758, "native_total_reference_addresses": 1762}
        return objects

    def negative_control(self):
        negative = self.output / "negative-control"
        negative.mkdir(exist_ok=True)
        for extension in (".h", ".c"):
            content = self.run("old-navigation" + extension,
                ["git", "show", BASELINE + ":" + NAVIGATION + extension]).stdout
            (negative / ("gimppainternavigation" + extension)).write_text(content)
        old_h = negative / "gimppainternavigation.h"
        old_c = negative / "gimppainternavigation.c"
        c_object = self.output / "old-navigation.o"
        self.run("old-navigation-compile", self.compiler(False) + ["-std=c11"] + self.flags +
                 ["-I" + str(ROOT / "app/display"), "-c", old_c, "-o", c_object])
        c_hash = sha256(c_object)
        old_include = '#include "' + str(old_h) + '"\n'
        fixture = '#include "navigation-calls.inc"\nint main(void) { painter_check_navigation(); return 0; }\n'
        # Deliberately do not reuse the app prelude here: it transitively includes
        # the current display enum header, which is irrelevant to this regression.
        old_cpp = self.compile("old-navigation-caller", old_include * 2 + fixture, True,
                               extra=["-I" + str(ROOT / "app/display")])
        names = sorted({f["symbol"] for h in self.scope["headers"] if h["path"] == NAVIGATION + ".h"
                        for f in h["functions"]})
        if len(names) != 7:
            raise RuntimeError("Negative control expected exactly seven navigation APIs")
        mangled = self.undefined(old_cpp, "old-navigation-nm")
        if any(name in mangled for name in names) or any(not any(name in m for m in mangled) for name in names):
            raise RuntimeError("Old caller did not retain all seven C++-mangled API references")
        libs = shlex.split(self.run("negative-pkg-libs", ["pkg-config", "--libs", "gtk+-3.0"]).stdout) + ["-lm"]
        failed = self.run("old-navigation-expected-link-failure", self.compiler(True) +
            [old_cpp, c_object, "-o", self.output / "negative-old"] + libs, expected_failure=True)
        if any(name not in failed.stderr for name in names):
            raise RuntimeError("Old link failure did not diagnose all seven navigation APIs")
        new_include = include(NAVIGATION + ".h")
        new_cpp = self.compile("fixed-navigation-caller", new_include * 2 + fixture, True)
        executable = self.output / "negative-fixed"
        self.run("fixed-navigation-link", self.compiler(True) + [new_cpp, c_object, "-o", executable] + libs)
        self.run("fixed-navigation-runtime", [executable])
        if sha256(c_object) != c_hash:
            raise RuntimeError("Negative control C implementation object changed")
        self.report["results"]["negative_control"] = {"baseline_commit": BASELINE,
            "old_header_sha256": sha256(old_h), "old_c_source_sha256": sha256(old_c),
            "unchanged_c_object_sha256": c_hash, "old_link_rejected_all_7": names,
            "header_only_fix_linked_and_called_all_7": True}

    def native_link(self, objects):
        for filename, cpp in (("runtime-c.c", False), ("runtime-cpp.cpp", True)):
            objects.append(self.compile(Path(filename).stem, (FIXTURES / filename).read_text(), cpp))
        if self.args.link_command:
            info = json.loads(self.args.link_command.read_text())
            command = info.get("argv") or shlex.split(info["command"])
            cwd = Path(info["cwd"])
        else:
            cwd = self.args.build_dir.resolve()
            output = self.run("native-link-command", ["ninja", "-C", cwd, "-t", "commands", "app/gimp-3.0"]).stdout
            matching = [shlex.split(line) for line in output.splitlines()
                        if " -o app/gimp-3.0 " in line]
            if len(matching) != 1:
                raise RuntimeError("Could not identify exactly one native GUI linker command")
            command = matching[0]
        main_object = "app/gimp-3.0.p/main.c.o"
        if command.count(main_object) != 1:
            raise RuntimeError("Native GUI command must contain exactly one main object")
        definitions = self.run("native-main-symbols", ["nm", "--defined-only", "--extern-only", "--format=posix",
                                                      cwd / main_object]).stdout
        if {line.split()[0] for line in definitions.splitlines() if line.strip()} != {"main"}:
            raise RuntimeError("Replacing the main object would remove other production definitions")
        executable = self.output / "native-header-linkage"
        command[command.index("-o") + 1] = str(executable)
        pos = command.index(main_object)
        command[pos:pos + 1] = [str(obj) for obj in objects]
        http = self.args.http_build_dir.resolve() / "app/httpd"
        archives = [http / "libapphttpd.a", http / "libapphttpdgui.a"]
        app_archives = [cwd / arg for arg in command
                        if arg.startswith("app/") and arg.endswith(".a")] + archives
        self.snapshot(app_archives)
        self.report["linked_production_archives"] = [
            {"path": str(path), "sha256": self.input_hashes[path.resolve()]}
            for path in app_archives]
        sdk_libraries = [cwd / arg for arg in command
                         if not arg.startswith("-") and ".so" in arg and (cwd / arg).is_file()]
        self.snapshot(sdk_libraries)
        defined_output = self.run("native-app-defined-symbols", ["nm", "--defined-only",
            "--extern-only", "--format=posix"] + app_archives).stdout
        defined = {line.split()[0] for line in defined_output.splitlines()
                   if len(line.split()) >= 2 and len(line.split()[1]) == 1}
        missing = set(self.symbols) - defined
        if missing != DECLARATION_ONLY.keys():
            raise RuntimeError("Production archive missing-function set changed: " +
                               ", ".join(sorted(missing)))
        self.report["results"]["production_definitions"] = {
            "application_archives": len(app_archives),
            "defined_application_functions": 1758,
            "missing_exactly_baseline_declarations": sorted(missing)}
        soup = shlex.split(self.run("http-pkg-libs", ["pkg-config", "--libs", "libsoup-3.0"]).stdout)
        end = command.index("-Wl,--end-group")
        command[end:end] = [str(path) for path in archives] + soup
        self.run("native-production-link", command, cwd)
        # Meson's executable normally lives in build/app. The verifier's external
        # output directory needs the same real SDK libraries on its loader path.
        env = os.environ.copy()
        directories = sorted({str((cwd / arg).parent) for arg in command if ".so" in arg and not arg.startswith("-")})
        env["LD_LIBRARY_PATH"] = ":".join(directories + [env.get("LD_LIBRARY_PATH", "")])
        runtime = self.run("native-production-runtime", [executable], cwd, env=env)
        self.report["results"]["native_link"] = {"runtime_output": runtime.stdout.strip(),
            "executable_sha256": sha256(executable),
            "http_archives": [{"path": str(path), "sha256": sha256(path)} for path in archives],
            "replacement": "Only app/gimp-3.0.p/main.c.o, verified to define main only"}

    def execute(self):
        self.verify_scope()
        self.save()
        self.syntax()
        self.save()
        objects = self.references()
        self.save()
        self.negative_control()
        self.save()
        self.native_link(objects)
        self.verify_unchanged_inputs()
        self.report["tool_sha256"] = self.input_hashes[Path(__file__).resolve()]
        self.report["status"] = "PASS"
        self.save()
        print("PASS: 103 routes / 101 targets, 1762 C/C++ application declarations, "
              "1758 native production APIs, four SDK APIs, and real bidirectional runtime calls")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scope", type=Path, default=ROOT / "migration/inventory/c-header-linkage.json")
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--http-build-dir", type=Path, required=True)
    parser.add_argument("--link-command", type=Path, help="Optional JSON containing exact native linker argv/cwd")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    verification = None
    try:
        verification = Verification(args)
        verification.execute()
    except (RuntimeError, OSError, KeyError, ValueError) as error:
        if verification:
            verification.report["status"] = "FAIL"
            verification.report["error"] = str(error)
            verification.save()
        print("FAIL:", error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
