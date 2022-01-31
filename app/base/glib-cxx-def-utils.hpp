/*
 * glib-cxx-def-utils.hpp
 *
 *  Created on: 2017/06/25
 *      Author: seagetch
 */

#ifndef APP_BASE_GLIB_CXX_DEF_UTILS_HPP_
#define APP_BASE_GLIB_CXX_DEF_UTILS_HPP_

extern "C" {
#include <gtk/gtk.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkscrolledwindow.h>
}
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include <functional>

namespace GLib {

template<typename T>
class Definer : public IObject<T>
{
  typedef IObject<T> super;
public:
  Definer() : super(NULL) { };
  Definer(T* object) : super(object) { };
  Definer(const IObject<T>& src) : super (src.ptr()) { }
  Definer(const Definer& src) : super (src.obj) { }

  operator T* () { return (T*)(*(super*)this); };
  template<typename G> operator G* () { return (G*)(*(super*)this); };
  T* operator ->() { return (T*)(*(super*)this); };
  operator bool() { return bool(*(super*)this); };

  void operator = (T* src) { (*(super*)this) = src; }
  template<typename Ret, typename C, typename... Args>
  auto operator []( Ret f(C*, Args...) ) { return (*(super*)this)[f]; };
  template<typename Ret, typename C, typename... Args>
  auto operator []( Ret f(const C*, Args...) ) { return (*(super*)this)[f]; };
  CopyValue operator[](const gchar* name) { return (*(super*)this)[name]; }

  // Helpers for easy GUI definition.
  struct Packer {
    Definer& wrapper;
    bool start, expand, fill;
    unsigned int padding;
    Packer(Definer& w, bool s, bool e, bool f, unsigned int p) :
      wrapper(w), start(s), expand(e), fill(f), padding(p) {};
    template<typename T2, typename F>
    Packer& operator ()(T2* child, F init) {
      if (start)
        wrapper [gtk_box_pack_start] (GTK_WIDGET(child), expand, fill, padding);
      else
        wrapper [gtk_box_pack_end] (GTK_WIDGET(child), expand, fill, padding);
      init(Definer<T2>(child));
      return *this;
    };
    Packer& pack_start(bool expand, bool fill, unsigned int padding) {
      this->start   = true;
      this->expand  = expand;
      this->fill    = fill;
      this->padding = padding;
      return *this;
    }
    Packer& pack_end(bool expand, bool fill, unsigned int padding) {
      this->start   = false;
      this->expand  = expand;
      this->fill    = fill;
      this->padding = padding;
      return *this;
    }
    template<typename T2>
    Definer& pack_start(T2* child, gboolean expand, gboolean fill, guint padding) {
      return wrapper.pack_start(child, expand, fill, padding);
    }
    template<typename T2, typename F>
    Definer& pack_start(T2* child, gboolean expand, gboolean fill, guint padding, F f) {
      return wrapper.pack_start(child, expand, fill, padding, f);
    }
    template<typename T2>
    Definer& pack_end(T2* child, gboolean expand, gboolean fill, guint padding) {
      return wrapper.pack_end(child, expand, fill, padding);
    }
    template<typename T2, typename F>
    Definer& pack_end(T2* child, gboolean expand, gboolean fill, guint padding, F f) {
      return wrapper.pack_end(child, expand, fill, padding, f);
    }
  };

  template<typename T2>
  Definer& pack_start(T2* child, gboolean expand, gboolean fill, guint padding) {
    (*this)[gtk_box_pack_start](GTK_WIDGET(child), expand, fill, padding);
    return *this;
  }
  template<typename T2, typename F>
  Definer& pack_start(T2* child, gboolean expand, gboolean fill, guint padding, F init) {
    pack_start<T2>(child, expand, fill, padding);
    init(Definer<T2>(child));
    return *this;
  }
  auto pack_start(bool expand, bool fill, unsigned int padding) {
    return Packer(*this, true, expand, fill, padding);
  }

  template<typename T2>
  Definer& pack_end(T2* child, gboolean expand, gboolean fill, guint padding) {
    (*this)[gtk_box_pack_end](GTK_WIDGET(child), expand, fill, padding);
    return *this;
  }
  template<typename T2, typename F>
  Definer& pack_end(T2* child, gboolean expand, gboolean fill, guint padding, F init) {
    pack_end<T2>(child, expand, fill, padding);
    init(Definer<T2>(child));
    return *this;
  }
  auto pack_end(bool expand, bool fill, unsigned int padding) {
    return Packer(*this, false, expand, fill, padding);
  }

  template<typename T2>
  void add(T2* child) {
    (*this)[gtk_container_add](GTK_WIDGET(child));
  }
  template<typename T2, typename F>
  Definer<T2> add(T2* child, F init) {
    add<T2>(child);
    auto i_child = Definer<T2>(child);
    init(i_child);
    return i_child;
  }

  template<typename T2>
  void add_with_viewport(T2* child) {
    (*this)[gtk_scrolled_window_add_with_viewport](GTK_WIDGET(child));
  }
  template<typename T2, typename F>
  Definer<T2> add_with_viewport(T2* child, F init) {
    add_with_viewport<T2>(child);
    auto i_child = Definer<T2>(child);
    init(i_child);
    return i_child;
  }
};

template<class T, typename F>
inline Definer<T> with(T* obj, F init) {
  auto result = Definer<T>(obj);
  init(result);
  return result;
}

template<class T, typename F>
inline Definer<T> with(IObject<T> obj, F init) {
  auto result = Definer<T>(obj.ptr());
  init(result);
  return result;
}

template<class T, typename F>
inline Definer<T> with(Definer<T> obj, F init) {
  init(obj);
  return obj;
}

};
#endif /* APP_BASE_GLIB_CXX_DEF_UTILS_HPP_ */
