/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BYTES_HPP
#define GIMP_PAINTER_BYTES_HPP
#include "gimp-painter-visibility.h"
#include <glib.h>
#include <utility>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Explicit transfer before callbacks: even a GBytes free function can reenter
 * the wrapper being assigned. Never unref its old value before publishing. */
class Bytes
{
public:
  Bytes () noexcept = default;
  ~Bytes () { reset (); }
  static Bytes adopt (GBytes *p) noexcept { return Bytes (p); }
  static Bytes retain (GBytes *p) noexcept { return Bytes (p ? g_bytes_ref (p) : nullptr); }
  Bytes (const Bytes& other) noexcept : value_ (other.value_ ? g_bytes_ref (other.value_) : nullptr) {}
  Bytes (Bytes&& other) noexcept : value_ (other.release ()) {}
  Bytes& operator= (const Bytes& other) noexcept { Bytes next (other); swap (next); return *this; }
  Bytes& operator= (Bytes&& other) noexcept
  { if (this != &other) { Bytes next (std::move (other)); swap (next); } return *this; }
  void swap (Bytes& other) noexcept { std::swap (value_, other.value_); }
  GBytes *release () noexcept { auto *p = value_; value_ = nullptr; return p; }
  void reset () noexcept { auto *p = release (); if (p) g_bytes_unref (p); }
  GBytes *get () const noexcept { return value_; }
  explicit operator bool () const noexcept { return value_ != nullptr; }
private:
  explicit Bytes (GBytes *p) noexcept : value_ (p) {}
  GBytes *value_ = nullptr;
};
}
#endif
