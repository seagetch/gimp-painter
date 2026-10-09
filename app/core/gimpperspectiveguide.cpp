/* SPDX-License-Identifier: GPL-3.0-or-later
 * Direction selection and coordinate constraint: gimp-painter afa43fae.
 */
#include "config.h"
#include "gimpperspectiveguide.hpp"
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
#include <cmath>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpPerspectiveGuide, gimp_perspective_guide, G_TYPE_OBJECT)
namespace {
enum { PROP_0, PROP_ID, PROP_ANGLE };
struct GuideImpl { GimpPerspectiveGuideState state {}; void close () noexcept {} };
struct GuideSlot : SlotSpec<GimpPerspectiveGuide, GuideImpl> {};
BindingStore& store (GimpPerspectiveGuide *guide)
{
  if (!GIMP_IS_PERSPECTIVE_GUIDE (guide)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected perspective guide");
  return BindingStore::require (G_OBJECT (guide));
}
void constructed (GObject *object)
{
  G_OBJECT_CLASS (gimp_perspective_guide_parent_class)->constructed (object);
  auto *guide = GIMP_PERSPECTIVE_GUIDE (object);
  if (!guide->binding_failed) guide->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::require (object).activate (); return true;
  });
  if (guide->binding_failed) gimp_painter_binding_close (object, nullptr);
}
void dispose (GObject *object)
{ gimp_painter_binding_close (object, nullptr); G_OBJECT_CLASS (gimp_perspective_guide_parent_class)->dispose (object); }
void set_property (GObject *object, guint prop, const GValue *value, GParamSpec *pspec)
{
  property_boundary (object, pspec, "set", [&] {
    auto& binding = BindingStore::require (object);
    auto set = [&] (GuideImpl& impl) {
      if (prop == PROP_ID) impl.state.id = g_value_get_uint (value);
      else if (prop == PROP_ANGLE) impl.state.angle = g_value_get_double (value);
      else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
    };
    if (binding.state () == BindingStore::State::constructing) binding.initialize<GuideSlot> (set);
    else { binding.with<GuideSlot> (set); g_signal_emit_by_name (object, "changed"); }
  });
}
void get_property (GObject *object, guint prop, GValue *value, GParamSpec *pspec)
{
  property_boundary (object, pspec, "get", [&] { BindingStore::require (object).read<GuideSlot> ([&] (const GuideImpl& impl) {
    if (prop == PROP_ID) g_value_set_uint (value, impl.state.id);
    else if (prop == PROP_ANGLE) g_value_set_double (value, impl.state.angle);
    else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
  }); });
}
template<class F> gboolean mutate (GimpPerspectiveGuide *guide, F function)
{
  return boundary<gboolean> (nullptr, FALSE, [&] {
    auto lease = ObjectRef<GimpPerspectiveGuide>::retain (guide);
    gboolean changed = store (guide).with<GuideSlot> ([&] (GuideImpl& impl) { return function (impl.state); });
    if (changed) g_signal_emit_by_name (guide, "changed");
    return changed;
  });
}
}
static void gimp_perspective_guide_class_init (GimpPerspectiveGuideClass *klass)
{
  auto *object = G_OBJECT_CLASS (klass);
  object->constructed = constructed; object->dispose = dispose;
  object->set_property = set_property; object->get_property = get_property;
  g_object_class_install_property (object, PROP_ID, g_param_spec_uint ("id", nullptr, nullptr, 0, G_MAXUINT32, 0,
    GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object, PROP_ANGLE, g_param_spec_double ("angle", nullptr, nullptr, 0, G_PI, 0,
    GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS)));
  g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
  g_signal_new ("removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GimpPerspectiveGuideClass, removed), nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}
static void gimp_perspective_guide_init (GimpPerspectiveGuide *guide)
{
  guide->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (guide)).emplace<GuideSlot> (); return true;
  });
}
GimpPerspectiveGuide *gimp_perspective_guide_new (guint32 id)
{
  return boundary<GimpPerspectiveGuide *> (nullptr, nullptr, [&] {
    auto guide = ObjectRef<GimpPerspectiveGuide>::adopt (GIMP_PERSPECTIVE_GUIDE (g_object_new (GIMP_TYPE_PERSPECTIVE_GUIDE, "id", id, nullptr)));
    if (guide.get ()->binding_failed) return static_cast<GimpPerspectiveGuide *> (nullptr);
    return guide.release ();
  });
}
gboolean gimp_perspective_guide_get_state (GimpPerspectiveGuide *guide, GimpPerspectiveGuideState *state)
{
  if (!state) return FALSE;
  return boundary<gboolean> (nullptr, FALSE, [&] { return store (guide).read<GuideSlot> ([&] (const GuideImpl& impl) {
    *state = impl.state; return TRUE;
  }); });
}
GimpPerspectiveGuide *gimp_perspective_guide_duplicate (GimpPerspectiveGuide *guide)
{
  if (!guide) return nullptr;
  return boundary<GimpPerspectiveGuide *> (nullptr, nullptr, [&] {
    GimpPerspectiveGuideState state {};
    if (!gimp_perspective_guide_get_state (guide, &state)) return static_cast<GimpPerspectiveGuide *> (nullptr);
    auto copy = ObjectRef<GimpPerspectiveGuide>::adopt (gimp_perspective_guide_new (state.id));
    if (!copy) return static_cast<GimpPerspectiveGuide *> (nullptr);
    store (copy.get ()).with<GuideSlot> ([&] (GuideImpl& impl) { impl.state = state; });
    return copy.release ();
  });
}
gint gimp_perspective_guide_get_vanish_point_length (GimpPerspectiveGuide *guide)
{ GimpPerspectiveGuideState state {}; return gimp_perspective_guide_get_state (guide, &state) ? state.n_points : 0; }
gint gimp_perspective_guide_add_vanish_points (GimpPerspectiveGuide *guide, gdouble x, gdouble y)
{
  gint index = -1;
  mutate (guide, [&] (GimpPerspectiveGuideState& state) -> gboolean {
    if (state.n_points == 3 || !std::isfinite (x) || !std::isfinite (y)) return FALSE;
    index = state.n_points++; state.points[index] = { x, y }; return TRUE;
  });
  return index;
}
gboolean gimp_perspective_guide_remove_vanish_points (GimpPerspectiveGuide *guide, gint index)
{
  return mutate (guide, [&] (GimpPerspectiveGuideState& state) -> gboolean {
    if (index < 0 || index >= state.n_points) return FALSE;
    for (gint i = index + 1; i < state.n_points; ++i) state.points[i - 1] = state.points[i];
    state.points[--state.n_points] = { 0, 0 }; return TRUE;
  });
}
gboolean gimp_perspective_guide_get_vanish_points (GimpPerspectiveGuide *guide, gint index, gdouble *x, gdouble *y)
{
  GimpPerspectiveGuideState state {};
  if (!x || !y || !gimp_perspective_guide_get_state (guide, &state) || index < 0 || index >= state.n_points) return FALSE;
  *x = state.points[index].x; *y = state.points[index].y; return TRUE;
}
gboolean gimp_perspective_guide_set_vanish_points (GimpPerspectiveGuide *guide, gint index, gdouble x, gdouble y)
{
  return mutate (guide, [&] (GimpPerspectiveGuideState& state) -> gboolean {
    if (index < 0 || index >= state.n_points || !std::isfinite (x) || !std::isfinite (y)) return FALSE;
    state.points[index] = { x, y }; return TRUE;
  });
}
gboolean gimp_perspective_guide_snap_angle (GimpPerspectiveGuide *guide, gdouble ox, gdouble oy,
                                            gdouble x, gdouble y, gdouble sx, gdouble sy, gdouble *angle)
{
  GimpPerspectiveGuideState state {};
  if (!angle || !gimp_perspective_guide_get_state (guide, &state) || state.n_points == 0 ||
      !std::isfinite (ox) || !std::isfinite (oy) || !std::isfinite (x) || !std::isfinite (y) ||
      !std::isfinite (sx) || !std::isfinite (sy) || sx <= 0 || sy <= 0) return FALSE;
  double dx = (x - ox) * sx, dy = (y - oy) * sy;
  if (dx * dx + dy * dy < 32 * 32) return FALSE;
  double current = std::fmod (std::atan2 (-dy, dx) + G_PI, G_PI);
  double candidates[5]; int n = 0;
  if (state.n_points == 1)
    {
      candidates[n++] = std::fmod (state.angle + G_PI, G_PI);
      candidates[n++] = std::fmod (state.angle + G_PI * 1.5, G_PI);
    }
  else if (state.n_points == 2)
    candidates[n++] = std::fmod (std::atan2 (state.points[0].y - state.points[1].y,
                                           state.points[1].x - state.points[0].x) + G_PI * 1.5, G_PI);
  for (int i = 0; i < state.n_points; ++i)
    candidates[n++] = std::fmod (std::atan2 (state.points[i].y - oy, ox - state.points[i].x) + G_PI, G_PI);
  double minimum = -1; int selected = 0;
  for (int i = 0; i < n; ++i)
    {
      double diff = std::fmod (candidates[i] + G_PI - current, G_PI);
      diff = MIN (diff, G_PI - diff);
      if (minimum < 0 || diff < minimum) { minimum = diff; selected = i; }
    }
  *angle = candidates[selected]; return TRUE;
}
void gimp_perspective_guide_constrain (gdouble angle, gdouble ox, gdouble oy, gdouble *x, gdouble *y)
{
  if (!x || !y || !std::isfinite (angle) || !std::isfinite (ox) || !std::isfinite (oy) ||
      !std::isfinite (*x) || !std::isfinite (*y)) return;
  angle = std::fmod (angle, G_PI); if (angle < 0) angle += G_PI;
  if (angle < G_PI / 4 || angle > G_PI * 3 / 4) *y = oy - std::sin (angle) / std::cos (angle) * (*x - ox);
  else *x = ox - std::cos (angle) / std::sin (angle) * (*y - oy);
}
