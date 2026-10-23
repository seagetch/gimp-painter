/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BOUNDARY_HPP
#define GIMP_PAINTER_BOUNDARY_HPP

#include "gimp-painter-visibility.h"
#include "gimp-painter-error.h"
#include <exception>
#include <stdexcept>
#include <utility>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class Error : public std::runtime_error
{
public:
  Error (GimpPainterError code, const char *message)
    : std::runtime_error (message), code_ (code) {}
  GimpPainterError code () const noexcept { return code_; }
private:
  GimpPainterError code_;
};

/* Called only from inside a catch block. GError follows normal GLib ownership. */
inline void
capture_exception (GError **error) noexcept
{
  try { throw; }
  catch (const Error& ex)
    { g_set_error_literal (error, GIMP_PAINTER_ERROR, ex.code (), ex.what ()); }
  catch (const std::exception& ex)
    { g_set_error_literal (error, GIMP_PAINTER_ERROR,
                           GIMP_PAINTER_ERROR_EXCEPTION, ex.what ()); }
  catch (...)
    { g_set_error_literal (error, GIMP_PAINTER_ERROR,
                           GIMP_PAINTER_ERROR_EXCEPTION, "Unknown C++ exception"); }
}

template<class Result, class Function>
Result
boundary (GError **error, Result fallback, Function&& function) noexcept
{
  try { return function (); }
  catch (...) { capture_exception (error); return fallback; }
}

template<class Function>
void
boundary_void (GError **error, Function&& function) noexcept
{
  try { function (); }
  catch (...) { capture_exception (error); }
}

} // namespace GimpPainter
#endif
