/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_FACTORY_GUI_HPP
#define GIMP_PRESET_FACTORY_GUI_HPP
extern "C" {
#include "preset-factory-gui.h"
#include "widgets/gimpactiongroup.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpActionGroup>
{ static GType type () noexcept { return gimp_action_group_get_type (); } };
class ActionGroupRef {
public:
  ActionGroupRef () noexcept = default;
  static ActionGroupRef adopt (GimpActionGroup *p)
  { return ActionGroupRef (ObjectRef<GimpActionGroup>::adopt (p)); }
  static ActionGroupRef retain (GimpActionGroup *p)
  { return ActionGroupRef (ObjectRef<GimpActionGroup>::retain (p)); }
  GimpActionGroup *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
  void register_preset_actions () const
  { preset_factory_gui_action_group_entry_point (get ()); }
  void register_preset_dialog_actions () const
  { preset_factory_gui_dialogs_actions_entry_point (get ()); }
private:
  explicit ActionGroupRef (ObjectRef<GimpActionGroup> value) noexcept : owner_ (std::move (value)) {}
  ObjectRef<GimpActionGroup> owner_;
};
}
#endif
