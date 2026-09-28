#!/usr/bin/env python3
"""Check the legacy preset action callbacks and the GIMP 3 factory owner."""

import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION


LEGACY = {
    "app/presets/preset-factory-gui.cpp": ("singleton = new PresetGuiFactory()", "preset_factory_gui_action_group_update", "preset_factory_gui_action_group_entry_point"),
    "app/widgets/widgets-types.h": ("GimpActionGroupSetupFunc", "GimpActionGroupUpdateFunc"),
    "app/widgets/gimpactionfactory.c": ("entry->setup_func  = setup_func", "entry->update_func = update_func", "entry->setup_func (group)", "g_slice_free (GimpActionFactoryEntry, entry)"),
    "app/widgets/gimpactiongroup.c": ("group->update_func = update_func", "group->update_func (group, update_data)"),
    "app/actions/actions.c": ("global_action_factory = gimp_action_factory_new (gimp)", "g_object_unref (global_action_factory)"),
}
TARGET = {
    "app/widgets/widgets-types.h": ("GimpActionGroupSetupFunc", "GimpActionGroupUpdateFunc"),
    "app/widgets/gimpactionfactory.c": ("entry->setup_func  = setup_func", "entry->update_func = update_func", "entry->groups      = g_hash_table_new_full", "gimp_action_factory_get_group", "g_hash_table_insert (entry->groups, user_data, group)", "g_hash_table_unref (entry->groups)"),
    "app/widgets/gimpactiongroup.c": ("group->update_func = update_func", "group->update_func (group, update_data)"),
    "app/actions/actions.c": ("global_action_factory = gimp_action_factory_new (gimp)", "g_clear_object (&global_action_factory)"),
}


def read(revision, path):
    if revision == "WORKTREE":
        return Path(path).read_text(encoding="utf-8")
    return subprocess.check_output(["git", "show", f"{revision}:{path}"]).decode("utf-8")


def signatures(source):
    result = {}
    for name in ("Setup", "Update"):
        match = re.search(r"typedef\s+void\s*\(\*\s*GimpActionGroup" + name +
                          r"Func\s*\)\s*\(([^;]+)\);", source)
        assert match, name
        result[name] = re.sub(r"\s+", "", match[1])
    return result


def main():
    texts = {}
    for revision, cases in ((REVISION, LEGACY), ("WORKTREE", TARGET)):
        for path, tokens in cases.items():
            source = read(revision, path)
            texts[revision, path] = source
            for token in tokens:
                assert token in source, (revision, path, token)
    assert signatures(texts[REVISION, "app/widgets/widgets-types.h"]) == signatures(
        texts["WORKTREE", "app/widgets/widgets-types.h"])

    old = texts[REVISION, "app/widgets/gimpactionfactory.c"]
    modern = texts["WORKTREE", "app/widgets/gimpactionfactory.c"]
    assert "gimp_action_factory_group_new" in old
    assert "gimp_action_factory_group_new" not in modern
    assert "gimp_action_factory_get_group" in modern
    assert old.index("entry->setup_func  = setup_func") < old.index("entry->setup_func (group)")
    assert modern.index("entry->setup_func  = setup_func") < modern.index("entry->setup_func (group)")
    print("Preset action callbacks: legacy and GIMP 3 typedefs match; "
          "owner, setup, update, cached group and teardown anchors verified")


if __name__ == "__main__":
    main()
