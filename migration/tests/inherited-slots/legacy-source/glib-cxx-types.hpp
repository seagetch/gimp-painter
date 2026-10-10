/*
 * glib-cxx-impl.hpp
 *
 * Copyright (C) 2015 - seagetch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GLIB_CXX_TYPES_HPP__
#define __GLIB_CXX_TYPES_HPP__

extern "C" {
#include <glib.h>
#include <glib-object.h>
};

namespace GLib {

template<typename T>
struct strip_ref { using type = T; };
template<typename T> struct strip_ref<T&> { using type = T; };
template<typename T> struct strip_ref<const T&> { using type = T; };
template<typename T> struct strip_ref<T&&> { using type = T; };


template<typename T>
inline GType g_type()
{
  T::__this_should_generate_error_on_compilation;
  return G_TYPE_INVALID;
}

template<typename T>
inline auto g_class_type(const T* obj) {
  T::__this_should_generate_error_on_compilation;
  return G_TYPE_INSTANCE_GET_CLASS(obj, get_type<T>(), GTypeClass);
}

//typename _Class = typename std::remove_reference<decltype(*GLib::g_class_type<_Instance>(NULL))>::type,
template <typename _Instance,
          typename _Class = typename strip_ref<decltype(*GLib::g_class_type<_Instance>(NULL))>::type,
          GType (*g_type)() = g_type<_Instance> >
class TraitsBase {
public:
  typedef _Instance Instance;
  typedef _Class Class;
  static GType get_type() { return (*g_type)(); };
  static Instance* cast(gpointer obj) { return G_TYPE_CHECK_INSTANCE_CAST(obj, get_type(), Instance); }
  static Class*    cast_class(gpointer klass) { return G_TYPE_CHECK_CLASS_CAST(klass, get_type(), Class); }
  static gboolean  is_instance(gpointer obj) { return G_TYPE_CHECK_INSTANCE_TYPE(obj, get_type()); }
  static gboolean  is_class(gpointer obj) { return G_TYPE_CHECK_CLASS_TYPE(obj, get_type()); }
  static Class*    get_class(gpointer obj) { return G_TYPE_INSTANCE_GET_CLASS(obj, get_type(), Class); }
};

template <typename _Instance>
class Traits : public TraitsBase<_Instance>
{
#if 0
  using Parent = TraitsBase<_Instance>;
public:
  using Instance = typename Parent::Instance;
  using Class    = typename Parent::Class;
  static GType get_type() { return Parent::get_type(); }
  static Instance* cast(gpointer obj) { return Parent::cast(obj); }
  static Class* cast_class(gpointer klass) { return Parent::cast_class(klass); }
  static gboolean is_instance(gpointer obj) { return Parent::is_instance(obj); }
  static gboolean is_class(gpointer obj) { return Parent::is_class(obj); }
  static Class* get_class(gpointer obj) { return Parent::get_class(obj); }
#endif
};

}; // namespace

#define __DECLARE_GTK_CLASS__(klass, _g_type_)                                                       \
extern "C++"{                                                                                        \
  namespace GLib {                                                                                   \
    template<> inline GType g_type<klass>() { return _g_type_; }                                     \
    template<> inline auto g_class_type(const klass* obj) {                                          \
      return G_TYPE_INSTANCE_GET_CLASS(obj, g_type<klass>(), klass##Class);                          \
    }                                                                                                \
  };                                                                                                 \
};
#define __DECLARE_GTK_IFACE__(klass, cast_lower)                                                     \
extern "C++"{                                                                                        \
  namespace GLib {                                                                                   \
    template<> inline GType g_type<klass>() { return cast_lower##_get_type(); }                      \
    template<> inline auto g_class_type(const klass* obj) {                                          \
      return G_TYPE_INSTANCE_GET_CLASS(obj, g_type<klass>(), klass##Iface);                          \
    }                                                                                                \
  };                                                                                                 \
};
#define __DECLARE_GIMP_INTERFACE__(klass, _g_type_)                                                  \
extern "C++"{                                                                                        \
  namespace GLib {                                                                                   \
    template<> inline GType g_type<klass>() { return _g_type_; }                                     \
    template<> inline auto g_class_type(const klass* obj) {                                          \
      return G_TYPE_INSTANCE_GET_CLASS(obj, g_type<klass>(), klass##Interface);                      \
    }                                                                                                \
  };                                                                                                 \
};
#define __DECLARE_GTK_CAST__(klass, caster, cast_lower) __DECLARE_GTK_CLASS__(klass, cast_lower##_get_type())

#endif /* __GLIB_CXX_TYPES_HPP__ */
