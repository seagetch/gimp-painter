# Preset action factory callback owner (01.005 child audit)

Legacy revision: `afa43fae3e920210146abed514f136fd49f671b5`.
Target: current GIMP 3 branch. Run
`python3 -B tools/audit_preset_action_factory.py` to check the source anchors
and the `GimpActionGroupSetupFunc` / `GimpActionGroupUpdateFunc` typedefs.

| Phase | Legacy source | GIMP 3 source | Contract |
|---|---|---|---|
| Register | `app/presets/preset-factory-gui.cpp:207-218`; `app/widgets/gimpactionfactory.c:113-122` | `app/widgets/gimpactionfactory.c:142-167` | The preset GUI factory passes two plain functions; the action factory copies the setup and update pointers into its registered entry. No C++ object pointer is stored in the entry. |
| Setup | `app/widgets/gimpactionfactory.c:132-153`; `app/presets/preset-factory-gui.cpp:247-250` | `app/widgets/gimpactionfactory.c:171-204` | The entry's setup callback runs on group creation; the wrapper obtains the preset singleton and forwards to `action_group_entry_point`. Both old and new typedefs are `void (GimpActionGroup*)`. |
| Update | `app/presets/preset-factory-gui.cpp:77-82`; `app/widgets/gimpactiongroup.c:335-343` | `app/widgets/gimpactiongroup.c:530-537` | The static update callback retrieves the singleton, then forwards the group and user data. Both old and new typedefs are `void (GimpActionGroup*, gpointer)`. |
| Teardown | `app/actions/actions.c:278-279`; `app/widgets/gimpactionfactory.c:58-72` | `app/actions/actions.c:285`; `app/widgets/gimpactionfactory.c:68-83` | The global action factory is released during action shutdown. GIMP 3 also caches groups per `user_data` and releases that hash table during factory finalization. |

The legacy `PresetGuiFactory::get_factory()` allocates a singleton on first
use (`app/presets/preset-factory-gui.cpp:88-92`). The callback wrappers resolve
that singleton when invoked, instead of capturing an instance at registration.
The old `PresetGuiFactory::exit()` (`:223-229`) forwards to its base and does
not explicitly delete the singleton. There is no evidence here that callbacks
are safe after the factory is shut down; migration must establish startup,
group destruction, and action shutdown order before connecting them.

The GIMP 3 creation API is `gimp_action_factory_get_group()` and caches each
group by `user_data`, while the old `gimp_action_factory_group_new()` created
a new group. The preset action group implementation task must inspect the
new `get_group`/`delete_group` lifecycle and ensure setup is idempotent for
cached groups. The old `action_group_entry_point` and `action_group_update`
methods currently iterate over the preset registry with empty bodies
(`app/presets/preset-factory-gui.cpp:152-169`); the registration path exists,
but this audit does not establish useful action behavior.
