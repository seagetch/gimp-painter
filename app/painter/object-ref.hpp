/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_OBJECT_REF_HPP
#define GIMP_PAINTER_OBJECT_REF_HPP

#include "gimp-painter-visibility.h"
#include "boundary.hpp"
#include <memory>
#include <utility>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

template<class T> struct TypeTraits;
template<> struct TypeTraits<GObject>
{ static GType type () noexcept { return G_TYPE_OBJECT; } };

template<class T>
class ObjectRef
{
public:
  ObjectRef () noexcept = default;
  ObjectRef (std::nullptr_t) noexcept {}
  ~ObjectRef () { reset (); }
  ObjectRef (const ObjectRef& other) noexcept : object_ (other.object_)
  { if (object_) g_object_ref (object_); }
  ObjectRef (ObjectRef&& other) noexcept : object_ (other.release ()) {}
  ObjectRef& operator= (const ObjectRef& other) noexcept
  { ObjectRef copy (other); swap (copy); return *this; }
  ObjectRef& operator= (ObjectRef&& other) noexcept
  { if (this != &other) { ObjectRef moved (std::move (other)); swap (moved); } return *this; }

  static ObjectRef retain (T *object)
  { check (object); if (object) g_object_ref (object); return ObjectRef (object, Owned{}); }
  static ObjectRef adopt (T *object)
  { check (object); return ObjectRef (object, Owned{}); }
  static ObjectRef sink (T *object)
  { check (object); if (object) g_object_ref_sink (object); return ObjectRef (object, Owned{}); }

  T *get () const noexcept { return object_; }
  explicit operator bool () const noexcept { return object_ != nullptr; }
  T *release () noexcept { T *result = object_; object_ = nullptr; return result; }
  void reset () noexcept
  { T *old = release (); if (old) g_object_unref (old); }
  void swap (ObjectRef& other) noexcept { std::swap (object_, other.object_); }

private:
  struct Owned {};
  ObjectRef (T *object, Owned) noexcept : object_ (object) {}
  static void check (T *object)
  {
    if (object && !G_TYPE_CHECK_INSTANCE_TYPE (object, TypeTraits<T>::type ()))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Object has the wrong GType");
  }
  T *object_ = nullptr;
};

template<class T>
class WeakRef
{
  struct State
  {
    explicit State (T *object) { g_weak_ref_init (&ref, object); }
    ~State () { g_weak_ref_clear (&ref); }
    GWeakRef ref;
  };
public:
  WeakRef () = default;
  explicit WeakRef (const ObjectRef<T>& owner)
    : state_ (new State (owner.get ())) {}
  WeakRef (const WeakRef&) = delete;
  WeakRef& operator= (const WeakRef&) = delete;
  WeakRef (WeakRef&&) noexcept = default;
  WeakRef& operator= (WeakRef&&) noexcept = default;
  ObjectRef<T> lock () const
  {
    if (!state_) return {};
    return ObjectRef<T>::adopt (static_cast<T *> (g_weak_ref_get (&state_->ref)));
  }
  void reset () noexcept { state_.reset (); }
private:
  std::unique_ptr<State> state_;
};

} // namespace GimpPainter
#endif
