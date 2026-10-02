/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_RESOURCES_HPP
#define GIMP_PAINTER_RESOURCES_HPP
#include "boundary.hpp"
#include <memory>
#include <utility>

namespace GimpPainter {
struct Free { void operator() (gpointer value) const noexcept { g_free (value); } };
using String = std::unique_ptr<gchar, Free>;

class Value
{
public:
  Value () noexcept = default;
  explicit Value (GType type)
  {
    if (!G_TYPE_IS_VALUE (type))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "GType cannot hold a GValue");
    g_value_init (&value_, type);
  }
  ~Value () { reset (); }
  Value (const Value& other)
  {
    if (G_IS_VALUE (&other.value_))
      { g_value_init (&value_, G_VALUE_TYPE (&other.value_)); g_value_copy (&other.value_, &value_); }
  }
  Value (Value&& other) noexcept { swap (other); }
  Value& operator= (const Value& other)
  { Value copy (other); swap (copy); return *this; }
  Value& operator= (Value&& other) noexcept
  { if (this != &other) { Value moved (std::move (other)); swap (moved); } return *this; }
  static Value copy (const GValue *value)
  {
    if (!value || !G_IS_VALUE (value)) return {};
    Value result (G_VALUE_TYPE (value));
    g_value_copy (value, result.get ());
    return result;
  }
  GValue *get () noexcept { return &value_; }
  const GValue *get () const noexcept { return &value_; }
  void reset () noexcept
  {
    GValue old = value_;
    GValue empty = G_VALUE_INIT;
    value_ = empty;
    if (G_IS_VALUE (&old)) g_value_unset (&old);
  }
  void swap (Value& other) noexcept { std::swap (value_, other.value_); }
private:
  GValue value_ = G_VALUE_INIT;
};

/* A short const borrow, never an owning copy of a GValue's internal pointers. */
class ValueView
{
public:
  explicit ValueView (const GValue *value) noexcept : value_ (value) {}
  const GValue *get () const noexcept { return value_; }
  Value copy () const { return Value::copy (value_); }
private:
  const GValue *value_;
};

class ArrayRef
{
public:
  ArrayRef () noexcept = default;
  ~ArrayRef () { reset (); }
  static ArrayRef adopt (GArray *array) noexcept { return ArrayRef (array); }
  static ArrayRef retain (GArray *array) noexcept
  { return ArrayRef (array ? g_array_ref (array) : nullptr); }
  ArrayRef (const ArrayRef& other) noexcept : array_ (other.array_)
  { if (array_) g_array_ref (array_); }
  ArrayRef (ArrayRef&& other) noexcept : array_ (other.release ()) {}
  ArrayRef& operator= (const ArrayRef& other) noexcept
  { ArrayRef copy (other); swap (copy); return *this; }
  ArrayRef& operator= (ArrayRef&& other) noexcept
  { if (this != &other) { ArrayRef moved (std::move (other)); swap (moved); } return *this; }
  GArray *get () const noexcept { return array_; }
  GArray *release () noexcept { auto *old = array_; array_ = nullptr; return old; }
  void reset () noexcept { auto *old = release (); if (old) g_array_unref (old); }
  void swap (ArrayRef& other) noexcept { std::swap (array_, other.array_); }
private:
  explicit ArrayRef (GArray *array) noexcept : array_ (array) {}
  GArray *array_ = nullptr;
};

class MutexGuard
{
public:
  explicit MutexGuard (GMutex& mutex) noexcept : mutex_ (mutex) { g_mutex_lock (&mutex_); }
  ~MutexGuard () { g_mutex_unlock (&mutex_); }
  MutexGuard (const MutexGuard&) = delete;
  MutexGuard& operator= (const MutexGuard&) = delete;
private:
  GMutex& mutex_;
};
} // namespace GimpPainter
#endif
