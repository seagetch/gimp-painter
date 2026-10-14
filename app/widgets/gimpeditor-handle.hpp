/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_EDITOR_HANDLE_HPP
#define GIMP_EDITOR_HANDLE_HPP
#include <gtk/gtk.h>
extern "C" {
#include "widgets-types.h"
#include "gimpeditor.h"
#include "gimpeditor-cxx.h"
}
#include "painter/object-ref.hpp"
#include <type_traits>

namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpEditor>
{ static GType type () noexcept { return GIMP_TYPE_EDITOR; } };
template<> struct TypeTraits<GtkWidget> { static GType type () noexcept { return GTK_TYPE_WIDGET; } };
namespace Detail {
template<class... Args> struct EditorAlternates : std::false_type {};
template<> struct EditorAlternates<> : std::true_type {};
template<class Name, class Mask, class... Rest>
struct EditorAlternates<Name, Mask, Rest...> : std::integral_constant<bool,
  (std::is_same<typename std::decay<Name>::type, gchar*>::value ||
   std::is_same<typename std::decay<Name>::type, const gchar*>::value) &&
  std::is_same<typename std::decay<Mask>::type, GdkModifierType>::value &&
  EditorAlternates<Rest...>::value> {};
}

/* GUI-thread operations on a live native editor. Owning a reference does not
 * resurrect a destroyed GTK widget. No separate model or callback store. */
class EditorRef {
public:
  static EditorRef retain (GimpEditor *editor)
  { return EditorRef (ObjectRef<GimpEditor>::retain (editor)); }
  GimpEditor *get () const noexcept { return owner_.get (); }
  ObjectRef<GtkWidget> add_dropdown (const gchar *icon_name,
                                    const gchar *tooltip,
                                    const gchar *help_id,
                                    const gchar *list_id,
                                    GCallback callback,
                                    GCallback extended_callback,
                                    gpointer callback_data) const
  {
    // The native editor already parents this transfer-none primary button.
    return ObjectRef<GtkWidget>::retain (gimp_editor_add_dropdown (
      get (), icon_name, tooltip, help_id, list_id, callback,
      extended_callback, callback_data));
  }
  template<class... Alternates>
  ObjectRef<GtkWidget> add_action_dropdown (const gchar *list_id,
                                           const gchar *group_name,
                                           const gchar *action_name,
                                           Alternates... alternates) const
  {
    static_assert (Detail::EditorAlternates<Alternates...>::value,
                   "Editor alternates require action-name/GdkModifierType pairs; no caller sentinel");
    return ObjectRef<GtkWidget>::retain (gimp_editor_add_action_dropdown (
      get (), list_id, group_name, action_name, alternates...,
      static_cast<const gchar*> (nullptr)));
  }
private:
  explicit EditorRef (ObjectRef<GimpEditor> owner) : owner_ (std::move (owner))
  { if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected editor"); }
  ObjectRef<GimpEditor> owner_;
};
}
#endif
