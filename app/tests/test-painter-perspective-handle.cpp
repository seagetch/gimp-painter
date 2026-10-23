/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "core/gimpperspectiveguide.hpp"
#include "painter/gimp-painter-binding.h"
#include <cmath>
#include <type_traits>
using GimpPainter::PerspectiveGuideRef;
static_assert (std::is_nothrow_move_constructible<PerspectiveGuideRef>::value,
               "Moving a guide transfers its native owner");
static_assert (std::is_nothrow_move_assignable<PerspectiveGuideRef>::value,
               "Moving a guide assignment transfers its native owner");
namespace {
void finalized (gpointer data, GObject *) { ++*static_cast<guint *> (data); }
void ownership ()
{
  guint deaths = 0;
  auto guide = PerspectiveGuideRef::create (17);
  g_assert_true (bool (guide));
  g_assert_false (g_object_is_floating (guide.get ()));
  g_object_weak_ref (G_OBJECT (guide.get ()), finalized, &deaths);
  auto *raw = guide.get ();
  auto copy = guide;
  auto retained = PerspectiveGuideRef::retain (raw);
  guide = guide;
  auto& self = guide;
  guide = std::move (self);
  g_assert_true (guide.get () == raw);
  auto moved = std::move (guide);
  g_assert_false (bool (guide));
  guide = std::move (moved);
  g_assert_false (bool (moved));
  guide.reset (); copy.reset ();
  g_assert_cmpuint (deaths, ==, 0);
  retained.reset ();
  g_assert_cmpuint (deaths, ==, 1);
  auto adopted = PerspectiveGuideRef::adopt (gimp_perspective_guide_new (18));
  g_object_weak_ref (G_OBJECT (adopted.get ()), finalized, &deaths);
  adopted.reset ();
  g_assert_cmpuint (deaths, ==, 2);
}
void empty_and_duplicate ()
{
  PerspectiveGuideRef empty;
  g_assert_false (bool (empty));
  g_assert_false (bool (empty.duplicate ()));
  g_assert_false (bool (PerspectiveGuideRef::retain (nullptr)));
  g_assert_false (bool (PerspectiveGuideRef::adopt (nullptr)));
  GimpPerspectiveGuideState sentinel {}; sentinel.id = 123;
  g_assert_false (empty.state (&sentinel));
  g_assert_cmpuint (sentinel.id, ==, 123);
  auto guide = PerspectiveGuideRef::create (G_MAXUINT32);
  g_object_set (guide.get (), "angle", 1.25, nullptr);
  for (gint i = 0; i < 3; ++i) g_assert_cmpint (guide.add_point (i + .25, i - .5), ==, i);
  auto copy = guide.duplicate ();
  g_assert_true (copy.get () != guide.get ());
  g_assert_false (g_object_is_floating (copy.get ()));
  GimpPerspectiveGuideState state {};
  g_assert_true (copy.state (&state));
  g_assert_cmpuint (state.id, ==, G_MAXUINT32);
  g_assert_cmpfloat (state.angle, ==, 1.25);
  g_assert_cmpint (copy.count (), ==, 3);
  g_assert_true (guide.set_point (0, 91, 92));
  guide.reset ();
  for (gint i = 0; i < 3; ++i) {
    double x = 0, y = 0;
    g_assert_true (copy.get_point (i, &x, &y));
    g_assert_cmpfloat (x, ==, i + .25); g_assert_cmpfloat (y, ==, i - .5);
  }
}
void rejection_and_close ()
{
  auto wrong = GimpPainter::ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  auto *raw = reinterpret_cast<GimpPerspectiveGuide *> (wrong.get ());
  for (bool adopt : { false, true }) {
    bool caught = false;
    try {
      auto rejected = adopt ? PerspectiveGuideRef::adopt (raw) : PerspectiveGuideRef::retain (raw);
    } catch (const GimpPainter::Error& error) {
      caught = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE;
    }
    g_assert_true (caught);
    g_assert_cmpuint (G_OBJECT (raw)->ref_count, ==, 1);
  }
  auto guide = PerspectiveGuideRef::create (1);
  double x = 41, y = 42;
  g_assert_false (guide.get_point (0, &x, &y));
  g_assert_cmpfloat (x, ==, 41); g_assert_cmpfloat (y, ==, 42);
  g_assert_cmpint (guide.add_point (NAN, 0), ==, -1);
  for (gint i = 0; i < 3; ++i) g_assert_cmpint (guide.add_point (i, i), ==, i);
  g_assert_cmpint (guide.add_point (4, 4), ==, -1);
  g_assert_false (guide.set_point (-1, 1, 2));
  g_assert_false (guide.set_point (0, INFINITY, 2));
  g_assert_false (guide.remove_point (3));
  g_assert_false (guide.get_point (0, nullptr, &y));
  g_assert_false (guide.state (nullptr));
  g_assert_true (guide.remove_point (1));
  g_assert_cmpint (guide.count (), ==, 2);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (guide.get ()), nullptr));
  g_assert_cmpint (guide.add_point (9, 9), ==, -1);
  g_assert_false (guide.set_point (0, 9, 9));
  g_assert_false (guide.remove_point (0));
  /* Closed const snapshots remain available through the original provider. */
  g_assert_cmpint (guide.count (), ==, 2);
}
void reset_owner (GObject *, gpointer data)
{ static_cast<PerspectiveGuideRef *> (data)->reset (); }
void callback_reentry ()
{
  guint deaths = 0;
  auto guide = PerspectiveGuideRef::create (1);
  g_object_weak_ref (G_OBJECT (guide.get ()), finalized, &deaths);
  g_signal_connect (guide.get (), "changed", G_CALLBACK (reset_owner), &guide);
  /* The C mutation lease keeps storage alive while the caller owner resets. */
  g_assert_cmpint (guide.add_point (1, 2), ==, 0);
  g_assert_false (bool (guide));
  g_assert_cmpuint (deaths, ==, 1);
}
}
extern "C" void gimp_test_perspective_handle_register (void);
extern "C" void gimp_test_perspective_handle_register (void)
{
  g_test_add_func ("/perspective/handle/ownership", ownership);
  g_test_add_func ("/perspective/handle/empty-and-duplicate", empty_and_duplicate);
  g_test_add_func ("/perspective/handle/rejection-and-close", rejection_and_close);
  g_test_add_func ("/perspective/handle/callback-reentry", callback_reentry);
}

#include "test-painter-guide-factory-wrapper.inc"

#include "test-painter-guide-options-wrapper.inc"
