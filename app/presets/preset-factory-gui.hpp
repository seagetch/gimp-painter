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
class PreferencesSnapshotRef {
public:
  PreferencesSnapshotRef () noexcept = default;
  ~PreferencesSnapshotRef () { reset (); }
  static PreferencesSnapshotRef create (Gimp *owner, GError **error = nullptr)
  { return PreferencesSnapshotRef (gimp_preset_preferences_snapshot_new (owner, error)); }
  PreferencesSnapshotRef (const PreferencesSnapshotRef &other) noexcept
    : value_ (gimp_preset_preferences_snapshot_ref (other.value_)) {}
  PreferencesSnapshotRef (PreferencesSnapshotRef &&other) noexcept : value_ (other.value_)
  { other.value_ = nullptr; }
  PreferencesSnapshotRef &operator= (PreferencesSnapshotRef other) noexcept
  { std::swap (value_, other.value_); return *this; }
  explicit operator bool () const noexcept { return value_ != nullptr; }
  const PrefsDialogEntry *entries (guint *length = nullptr) const noexcept
  { return gimp_preset_preferences_snapshot_get_entries (value_, length); }
  void reset () noexcept
  { auto *old = value_; value_ = nullptr; gimp_preset_preferences_snapshot_unref (old); }
private:
  explicit PreferencesSnapshotRef (GimpPresetPreferencesSnapshot *value) noexcept : value_ (value) {}
  GimpPresetPreferencesSnapshot *value_ = nullptr;
};
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
  /* Legacy borrowed strings: owner/catalog epoch must remain alive. */
  ArrayRef preset_preferences () const
  { return ArrayRef::adopt (preset_factory_gui_prefs_entry_point ()); }
  PreferencesSnapshotRef preset_preferences_snapshot (GError **error = nullptr) const
  { return PreferencesSnapshotRef::create (get (), error); }
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
