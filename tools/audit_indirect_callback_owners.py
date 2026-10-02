#!/usr/bin/env python3
"""Map every legacy indirect-call syntax candidate to an owner and lifetime."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
INPUT = ROOT / "function-pointer-review.tsv"
OUTPUT = ROOT / "indirect-callback-owner-review.tsv"

# Each entry: role, owner/storage, invocation/registration, end/cleanup,
# followup task, lifetime note. All locations refer to REVISION.
REVIEW = {
    "app/base/delegators.hpp:35": ("SIGNAL_CLOSURE", "app/base/delegators.hpp:173", "app/base/delegators.hpp:35", "app/base/delegators.hpp:162", "06.017", "GClosure owns heap delegator; captured object remains borrowed"),
    "app/base/delegators.hpp:106": ("SIGNAL_CLOSURE", "app/base/delegators.hpp:107", "app/base/delegators.hpp:35", "app/base/delegators.hpp:162", "06.017", "Raw function copied into heap delegator; C closure destroy-notify deletes wrapper"),
    "app/base/delegators.hpp:228": ("UNUSED_TRAMPOLINE", "app/base/delegators.hpp:228", "app/base/delegators.hpp:229", "-", "-", "Template returns callback address without storing f; no app callers found"),
    "app/base/glib-cxx-impl.hpp:345": ("GTYPE_STATIC_CALLBACK", "app/base/glib-cxx-impl.hpp:345", "app/base/glib-cxx-impl.hpp:362", "-", "06.014", "Per-type singleton stores class init callback for process lifetime"),
    "app/base/glib-cxx-impl.hpp:397": ("GTYPE_STATIC_CALLBACK", "app/base/glib-cxx-impl.hpp:402", "app/base/glib-cxx-impl.hpp:362", "-", "06.014", "Constructor installs callback into static per-type slot"),
    "app/base/glib-cxx-impl.hpp:449": ("VFUNC_SLOT", "app/base/glib-cxx-impl.hpp:450", "app/base/glib-cxx-impl.hpp:462", "app/base/glib-cxx-impl.hpp:480", "06.014", "Binder targets GObject class slot; slot belongs to registered GType"),
    "app/base/glib-cxx-impl.hpp:467": ("VFUNC_SLOT", "app/base/glib-cxx-impl.hpp:450", "app/base/glib-cxx-impl.hpp:469", "app/base/glib-cxx-impl.hpp:480", "06.014", "Cast C++ function pointer before storing C class vfunc; ABI must be typed"),
    "app/base/glib-cxx-impl.hpp:474": ("VFUNC_SLOT", "app/base/glib-cxx-impl.hpp:475", "app/base/glib-cxx-impl.hpp:469", "app/base/glib-cxx-impl.hpp:480", "06.014", "Assignment forwards to Binder::bind"),
    "app/base/glib-cxx-impl.hpp:493": ("INTERFACE_STATIC_CALLBACK", "app/base/glib-cxx-impl.hpp:494", "app/base/glib-cxx-impl.hpp:498", "-", "06.016", "Impl::iface_init is static; GInterfaceInfo passed at type registration"),
    "app/base/glib-cxx-types.hpp:53": ("COMPILETIME_GTYPE", "app/base/glib-cxx-types.hpp:53", "app/base/glib-cxx-types.hpp:58", "-", "06.014", "Template non-type parameter is a static get_type function"),
    "app/base/glib-cxx-types.hpp:58": ("COMPILETIME_GTYPE", "app/base/glib-cxx-types.hpp:53", "app/base/glib-cxx-types.hpp:58", "-", "06.014", "Synchronous invocation of static get_type function"),
    "app/base/glib-cxx-utils.hpp:172": ("SYNCHRONOUS_STACK", "app/base/glib-cxx-utils.hpp:229", "app/base/glib-cxx-utils.hpp:172", "app/base/glib-cxx-utils.hpp:230", "05.004", "GList foreach borrows stack functor only until foreach returns"),
    "app/base/glib-cxx-utils.hpp:425": ("SYNCHRONOUS_STACK", "app/base/glib-cxx-utils.hpp:448", "app/base/glib-cxx-utils.hpp:425", "app/base/glib-cxx-utils.hpp:449", "05.004", "GHashTable foreach borrows stack functor only until foreach returns"),
    "app/base/glib-cxx-utils.hpp:962": ("BORROWED_BOUND_METHOD", "app/base/glib-cxx-utils.hpp:959", "app/base/glib-cxx-utils.hpp:962", "-", "05.004/bound-method-lifetime", "Returned callable holds raw object and static method pointer; must not outlive object"),
    "app/base/glib-cxx-utils.hpp:979": ("BORROWED_BOUND_METHOD", "app/base/glib-cxx-utils.hpp:976", "app/base/glib-cxx-utils.hpp:979", "-", "05.004/bound-method-lifetime", "Const variant holds raw object and static method pointer"),
    "app/base/glib-cxx-utils.hpp:1203": ("DEFERRED_SOURCE", "app/base/glib-cxx-utils.hpp:1185", "app/base/glib-cxx-utils.hpp:1203", "app/base/glib-cxx-utils.hpp:1218", "06.019", "EventSource stores handler and ID; destroy notify or remove_func closes source"),
    "app/base/glib-cxx-utils.hpp:1209": ("DEFERRED_SOURCE", "app/base/glib-cxx-utils.hpp:1206", "app/base/glib-cxx-utils.hpp:1209", "app/base/glib-cxx-utils.hpp:1218", "06.019", "Compile-time g_source_remove pointer cancels a retained source ID"),
    "app/base/json-cxx-utils.hpp:121": ("SYNCHRONOUS_STACK", "app/base/json-cxx-utils.hpp:125", "app/base/json-cxx-utils.hpp:121", "app/base/json-cxx-utils.hpp:131", "05.004", "JSON array foreach borrows stack functor during call only"),
    "app/base/json-cxx-utils.hpp:358": ("NOT_FUNCTION_POINTER", "app/base/json-cxx-utils.hpp:353", "app/base/json-cxx-utils.hpp:358", "-", "-", "IBuilder's operator() recursively invokes its own overloaded instance"),
    "app/base/scopeguard.hpp:10": ("COMPILETIME_DELETER", "app/base/scopeguard.hpp:10", "app/base/scopeguard.hpp:19", "app/base/scopeguard.hpp:17", "06.021", "Template predicate/deleter are static; ScopeGuard destructor calls Free conditionally"),
    "app/base/scopeguard.hpp:52": ("COMPILETIME_DELETER", "app/base/scopeguard.hpp:37", "app/base/scopeguard.hpp:52", "app/base/scopeguard.hpp:48", "06.021", "ScopedPointer replaces prior object with matching deleter"),
    "app/core/gimpmypaintbrush-load.cpp:488": ("LOCAL_TRANSFORM", "app/core/gimpmypaintbrush-load.cpp:455", "app/core/gimpmypaintbrush-load.cpp:488", "app/core/gimpmypaintbrush-load.cpp:677", "19.006/v1-curve-validation", "Optional static transform borrowed during v1 parse; unconditional goto skips every point"),
    "app/core/gimpmypaintbrush-load.cpp:535": ("LOCAL_TRANSFORM", "app/core/gimpmypaintbrush-load.cpp:503", "app/core/gimpmypaintbrush-load.cpp:535", "app/core/gimpmypaintbrush-load.cpp:679", "19.006", "Optional static transform borrowed during v2 parse; no persistent storage"),
    "app/gimp-features.cpp:48": ("STATIC_FEATURE_FACTORY", "app/gimp-features.cpp:50", "app/gimp-features.cpp:78", "-", "30.016/feature-entry-point", "Conditional function table holds static singleton factories"),
    "app/gimp-features.cpp:78": ("STATIC_FEATURE_FACTORY", "app/gimp-features.cpp:50", "app/gimp-features.cpp:78", "-", "30.016/feature-entry-point", "Factory result appended to static feature array; startup signals capture owner"),
    "app/httpd/navigation-guide.cpp:65": ("SYNCHRONOUS_STACK", "app/httpd/navigation-guide.cpp:70", "app/httpd/navigation-guide.cpp:65", "app/httpd/navigation-guide.cpp:71", "05.004", "gimp_container_foreach borrows stack functor during call only"),
    "app/httpd/rest-image-tree.cpp:59": ("SYNCHRONOUS_STACK", "app/httpd/rest-image-tree.cpp:64", "app/httpd/rest-image-tree.cpp:59", "app/httpd/rest-image-tree.cpp:65", "05.004", "gimp_container_foreach borrows stack functor during call only"),
    "app/tools/gimpmypainttool.cpp:816": ("SYNCHRONOUS_REGISTRATION", "app/tools/gimp-tools.c:220", "app/tools/gimpmypainttool.cpp:816", "app/tools/gimp-tools.c:220", "08.008", "Caller callback invoked synchronously; tool registry owns resulting tool info"),
    "app/widgets/popupper.cpp:512": ("WIDGET_OWNED_DELEGATOR", "app/widgets/popupper.cpp:523", "app/widgets/popupper.cpp:512", "app/widgets/popupper.cpp:544", "29.006/popover-handler-teardown", "Widget data owns decorator; destructor deletes view delegator; signal handles need teardown review"),
    "app/widgets/popupper.cpp:563": ("UNUSED_VIEW_CREATOR", "app/widgets/popupper.cpp:557", "app/widgets/popupper.cpp:583", "app/widgets/popupper.cpp:581", "-", "ToolbarPopoverViewCreator has no instantiated call sites in app"),
    "app/widgets/popupper.cpp:568": ("UNUSED_VIEW_CREATOR", "app/widgets/popupper.cpp:571", "app/widgets/popupper.cpp:587", "app/widgets/popupper.cpp:581", "-", "Uninstantiated helper stores C extended create-view callback and data destructor"),
    "app/widgets/popupper.cpp:583": ("UNUSED_VIEW_CREATOR", "app/widgets/popupper.cpp:571", "app/widgets/popupper.cpp:583", "app/widgets/popupper.cpp:581", "-", "Would free data if instantiated; no observed construction path"),
    "app/widgets/popupper.cpp:587": ("UNUSED_VIEW_CREATOR", "app/widgets/popupper.cpp:571", "app/widgets/popupper.cpp:587", "app/widgets/popupper.cpp:581", "-", "Would call extended view creator synchronously if instantiated"),
    "app/widgets/popupper.cpp:589": ("UNUSED_VIEW_CREATOR", "app/widgets/popupper.cpp:579", "app/widgets/popupper.cpp:589", "app/widgets/popupper.cpp:581", "-", "Would call basic view creator synchronously if instantiated"),
}


def main():
    with INPUT.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file, delimiter="\t"))
    assert len(rows) == 34 and len({row["legacy_site"] for row in rows}) == 34
    assert set(REVIEW) == {row["legacy_site"] for row in rows}, (
        set(REVIEW) ^ {row["legacy_site"] for row in rows})
    cache = {}

    def at(anchor):
        if anchor == "-":
            return ""
        path, number = anchor.rsplit(":", 1)
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"]).decode(
                    "utf-8", errors="replace").splitlines()
        assert 0 < int(number) <= len(cache[path]), anchor
        return cache[path][int(number) - 1]

    result = []
    for row in rows:
        anchor = row["legacy_site"]
        role, owner, call, cleanup, task, note = REVIEW[anchor]
        assert row["pointer_name"] in at(anchor), anchor
        assert row["status"] == "DONE"
        for related in (owner, call, cleanup):
            at(related)
        result.append((anchor, row["pointer_name"], row["syntax_role"],
                       role, owner, call, cleanup, task, note, "DONE"))

    assert sum(r[3] == "NOT_FUNCTION_POINTER" for r in result) == 1
    assert sum(r[3] == "UNUSED_VIEW_CREATOR" for r in result) == 5
    assert sum(r[3] == "BORROWED_BOUND_METHOD" for r in result) == 2
    assert sum(r[3] == "DEFERRED_SOURCE" for r in result) == 2

    popup = "\n".join(cache["app/widgets/popupper.cpp"])
    assert "delete create_view_delegator;" in popup
    destructor = popup.split("~PopoverDecorator() {", 1)[1].split("\n  }", 1)[0]
    assert "scroll_event_handler" not in destructor
    assert "button_press_handler" not in destructor
    v1 = "\n".join(cache["app/core/gimpmypaintbrush-load.cpp"])
    assert re.search(r"if\s*\(!\(x_val > prev_x\)\);\s*\{\s*"
                     r"g_print\s*\([^;]+;\s*goto next;", v1), "legacy v1 skip changed"
    assert v1.index("goto next;", v1.index("parse_points_v1 (\n")) < v1.index(
        "mapping->set_point", v1.index("parse_points_v1 (\n"))
    unused = subprocess.run(["git", "grep", "-n", "-w", "ToolbarPopoverViewCreator",
                             REVISION, "--", "app"], text=True, capture_output=True,
                            check=True).stdout.splitlines()
    assert len(unused) == 4 and all(":app/widgets/popupper.cpp:" in line
                                    for line in unused), unused
    proxy = subprocess.run(["git", "grep", "-n", "-w", "proxy", REVISION,
                            "--", "app/base/delegators.hpp"], text=True, capture_output=True,
                           check=True).stdout.splitlines()
    assert len(proxy) == 2 and all(":app/base/delegators.hpp:" in line
                                   for line in proxy), proxy
    wbs = Path("tasks.md").read_text(encoding="utf-8")
    assert all(task == "-" or f"| {task} |" in wbs
               for _, _, _, _, _, _, _, task, _, _ in result)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "pointer_name", "syntax_role", "owner_class",
                         "owner_or_storage", "call_or_registration", "release_or_end",
                         "followup_task", "ownership_note", "status"))
        writer.writerows(result)
    print(f"{len(result)} legacy pointer candidates mapped to owners and lifetimes")


if __name__ == "__main__":
    main()
