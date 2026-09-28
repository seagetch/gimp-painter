#!/usr/bin/env python3
"""Separate legacy callback registrations from their multiline continuations."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "callback-registration-review.tsv"
MEMBER = re.compile(r"(?:Delegators::)?delegator\s*\(\s*this\s*,\s*&\s*"
                    r"([A-Za-z_]\w*(?:::\w+)*)::(\w+)")
G_CALLBACK = re.compile(r"\bG_CALLBACK\s*\(\s*([A-Za-z_]\w*(?:::\w+)*)\s*\)")


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    sites = [row for row in candidates if row["kind"] == "callback_registration"]
    cache: dict[str, list[str]] = {}
    results = []
    for row in sites:
        path, line = row["source"], int(row["line"])
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        source = cache[path]
        code = source[line - 1].strip()
        assert code == row["legacy_code"], row
        statement = row["boundary_statement"]
        registration_site = f"{path}:{line}"
        if path == "app/base/delegators.hpp" and line in (167, 182):
            role = "HELPER_DECLARATION"
        elif "g_type_add_interface_static" in code or "g_type_register_static_simple" in code:
            role = "GTYPE_REGISTRATION"
        elif "g_cclosure_new" in code:
            role = "CLOSURE_CREATION"
        elif "g_signal_connect_closure" in code:
            role = "CLOSURE_INSTALLATION"
        elif "g_timeout_add" in code:
            role = "TIMEOUT_SOURCE"
            assert path == "app/core/gimpfilterlayer.cpp" and line == 614
            assert "notify_filter_end_callback, this" in code
        elif "g_signal_connect" in code:
            if (path in ("app/base/glib-cxx-utils.hpp",
                          "app/tools/gimptooloptions-gui-cxx.cpp")
                    or (path == "app/tools/gimptooloptions-gui-cxx.hpp" and line == 46)):
                role = "GENERIC_HELPER_FORWARD"
            elif path == "app/widgets/gimpeditor-cxx.cpp" and line in (132, 136):
                role = "PARAMETER_CALLBACK"
            else:
                role = "SIGNAL_CONNECTION"
        elif "G_CALLBACK" in code:
            previous = [(i, source[i - 1]) for i in range(max(1, line - 5), line)]
            action_starts = [i for i, text in previous
                             if "gimp_action_group_add_string_actions" in text]
            if action_starts:
                role = "ACTION_CALLBACK"
                registration_site = f"{path}:{action_starts[-1]}"
            elif "gimp_image_window_activate_navigation_bar" in code:
                role = "CUSTOM_API_CALLBACK"
            else:
                starts = [i for i, text in previous if "g_signal_connect" in text]
                assert starts, (path, line, code)
                role = "CONTINUATION_OF_SIGNAL"
                registration_site = f"{path}:{starts[-1]}"
        else:
            raise ValueError(f"Unclassified candidate: {row}")

        member = MEMBER.search(statement)
        callback = G_CALLBACK.search(statement)
        callback_target = (f"{member[1]}::{member[2]}" if member else
                           callback[1] if callback else "-")
        if role == "TIMEOUT_SOURCE":
            callback_target = "FilterLayer::notify_filter_end_callback"
            port_check = "Track source ID and cancel before owner destruction; raw this is passed"
        elif role == "CONTINUATION_OF_SIGNAL":
            port_check = "Same registration as the initiating signal line"
        elif role in ("SIGNAL_CONNECTION", "GENERIC_HELPER_FORWARD",
                      "PARAMETER_CALLBACK", "CLOSURE_CREATION", "CLOSURE_INSTALLATION"):
            port_check = "Check handler lifetime, disconnect and callback signature"
        else:
            port_check = "Check callback lifetime and C ABI during port"
        results.append((f"{path}:{line}", role, registration_site, callback_target,
                        port_check, "DONE"))

    counts = Counter(row[1] for row in results)
    assert len(results) == 94 and counts == {
        "HELPER_DECLARATION": 2, "GTYPE_REGISTRATION": 2,
        "CLOSURE_CREATION": 2, "CLOSURE_INSTALLATION": 2,
        "TIMEOUT_SOURCE": 1, "SIGNAL_CONNECTION": 60,
        "GENERIC_HELPER_FORWARD": 10, "PARAMETER_CALLBACK": 2,
        "ACTION_CALLBACK": 1, "CUSTOM_API_CALLBACK": 1,
        "CONTINUATION_OF_SIGNAL": 11,
    }, counts
    roles = {site: role for site, role, _, _, _, _ in results}
    assert all(roles.get(parent) == "SIGNAL_CONNECTION"
               for _, role, parent, _, _, _ in results
               if role == "CONTINUATION_OF_SIGNAL")
    assert all(target != "-" for _, role, _, target, _, _ in results
               if role == "SIGNAL_CONNECTION")
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "role", "registration_site",
                         "callback_target_if_visible", "port_check", "status"))
        writer.writerows(results)
    print(f"{len(results)} callback candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
