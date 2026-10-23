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

/* GObject property vfuncs cannot return GError. Keep the native owner and spec
 * alive through reentrant diagnostics, and report a contained failure without
 * changing GLib's initialized getter value or automatic notify semantics. */
template<class Function>
bool
property_boundary (GObject *object, GParamSpec *spec, const char *operation,
                   Function&& function) noexcept
{
  g_object_ref (object);
  g_param_spec_ref (spec);
  GError *error = nullptr;
  const bool success = boundary<bool> (&error, false, [&] {
    function ();
    return true;
  });
  if (error)
    {
      g_warning ("Painter property %s.%s %s failed: %s",
                 G_OBJECT_TYPE_NAME (object), spec->name, operation,
                 error->message);
      g_error_free (error);
    }
  g_param_spec_unref (spec);
  g_object_unref (object);
  return success;
}

} // namespace GimpPainter
#endif
