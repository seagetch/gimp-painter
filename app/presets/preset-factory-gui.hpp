/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_FACTORY_GUI_HPP
#define GIMP_PRESET_FACTORY_GUI_HPP
extern "C" {
#include "preset-factory-gui.h"
#include "widgets/gimpactiongroup.h"
#include "core/gimp.h"
#include "gimp-features.h"
}
#include "painter/object-ref.hpp"
#include "painter/resources.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpActionGroup>
{ static GType type () noexcept { return gimp_action_group_get_type (); } };
template<> struct TypeTraits<Gimp>
{ static GType type () noexcept { return GIMP_TYPE_GIMP; } };
class ApplicationRef {
public:
  ApplicationRef () noexcept = default;
  static ApplicationRef adopt (Gimp *p)
  { return ApplicationRef (ObjectRef<Gimp>::adopt (p)); }
  static ApplicationRef retain (Gimp *p)
  { return ApplicationRef (ObjectRef<Gimp>::retain (p)); }
  Gimp *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
  /* Native descriptor metadata is application-independent, including empty handles. */
  void initialize_features () const
  { features_entry_point (get ()); }
  ArrayRef preset_preferences () const
  { return ArrayRef::adopt (preset_factory_gui_prefs_entry_point ()); }
private:
  explicit ApplicationRef (ObjectRef<Gimp> value) noexcept : owner_ (std::move (value)) {}
  ObjectRef<Gimp> owner_;
};
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
