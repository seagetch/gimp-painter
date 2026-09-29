#!/usr/bin/env python3
"""Join every legacy CXXPointer occurrence with its owner and port follow-up."""

import csv
from pathlib import Path

SOURCE = Path("migration/inventory/cpp-scoped-pointer-review.tsv")
OUTPUT = Path("migration/inventory/cpp-scoped-owner-contracts.tsv")

FOLLOWUPS = {
    "app/base/glib-cxx-utils.hpp": "06.019",
    "app/base/scopeguard.hpp": "06.010",
    "app/base/soup-cxx-utils.hpp": "31.008/router-lifetime",
    "app/core/gimpclonelayer.cpp": "14.001,07.008",
    "app/core/gimpfilterlayer.cpp": "16.019,07.008",
    "app/display/gimpdisplayshell-overlays.cpp": "29.001,07.008",
    "app/httpd/httpd-features-gui.cpp": "31.008",
    "app/httpd/httpd-features.cpp": "31.008",
    "app/tools/gimpmypaintbrusheditor.cpp": "08.008,07.008",
    "app/widgets/gimplayertileview.cpp": "29.002,29.020/preview-idle-teardown,07.008",
    "app/widgets/gimptooltileview.cpp": "29.008,07.008",
}


def contract(row):
    site, role, line = row["legacy_site"], row["owner_role"], row["source_line"]
    path = site.rsplit(":", 1)[0]
    followup = FOLLOWUPS[path]
    if role == "SIGNAL_CONNECTION":
        return ("containing C++ Impl or nested preview", "CXXPointer deletes Connection; Connection disconnects a borrowed target",
                followup, "target must survive disconnect; verify close order")
    if role == "SOURCE_OWNER":
        assert path == "app/widgets/gimplayertileview.cpp"
        followup = "29.020/preview-idle-teardown" if "update_idle" in line else "29.002/add-timeout-owner"
        risk = ("idle captures raw preview and viewable; cancel on replacement and teardown" if "update_idle" in line
                else "timeout captures raw tile; add-button callback also clears its own CXXPointer" if "add_timeout" in line
                else "scroll callback captures tile and scrolled_window; cancel on drag end and close")
        return ("LayerPreview" if "update_idle" in line else "LayerTileView", "CXXPointer deletes EventSource; destructor removes registered GSource id", followup, risk)
    if role == "DELEGATOR_OWNER":
        risk = "DnD user data borrows delegator; unregister callback before delete" if "drag_viewable_holder" in line else "delegate/closure alias must not outlive CXXPointer owner"
        return ("containing C++ Impl or EventSource", "CXXPointer deletes C++ delegator", followup, risk)
    if role == "CXX_INSTANCE":
        if "ProcedureRunner" in line:
            followup = "16.019"
            risk = "adopt new runner before set_arg can fail; replacement deletes old runner"
        elif "LayerPopupWindow" in line:
            risk = "new popup replaces old decorator; callback must not retain old this"
        elif "DragAction" in line or "GdkPoint" in line:
            risk = "allocated with new and transferred to CXXPointer; clear on drag/button end"
        else:
            risk = "confirm new allocation transfer and replacement/teardown ordering"
        return ("containing C++ Impl or local scope", "CXXPointer deletes owned C++ instance on replacement or destruction", followup, risk)
    assert role == "HELPER_DEFINITION", site
    return ("guard caller", "guard(T*) adopts new C++ object; ScopedPointer destructor deletes it", followup,
            "callers must pass owned C++ pointer")


def main():
    with SOURCE.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    assert len(rows) == 54 and len({r["legacy_site"] for r in rows}) == 54
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "owner_role", "owner", "release_contract", "followup", "port_risk"))
        writer.writerows((row["legacy_site"], row["owner_role"], *contract(row)) for row in rows)
    print(f"{len(rows)} scoped pointer sites mapped to owner, release contract and port follow-up")


if __name__ == "__main__":
    main()
