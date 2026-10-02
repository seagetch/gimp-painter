/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent legacy Smudge raster semantics; current standard Smudge is unchanged. */
#include "config.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <gegl.h>
#include <cairo.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "paint-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimpimage.h"
#include "core/gimpsymmetry.h"
#include "core/gimptempbuf.h"
#include "gimppaintersmudge.h"
#include "gimpbrushcore-loops.h"
#include "operations/layer-modes/gimp-layer-modes.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter-smudge/legacy-pixels.hpp"
#include "painter-mypaint-surface/legacy-mask-transform.hpp"
#include "painter-mypaint-surface/legacy-generated-mask.hpp"
#include "gimp-intl.h"
#include "gimpbrushcore-kernels.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpPainterSmudgeOptions, gimp_painter_smudge_options, GIMP_TYPE_PAINT_OPTIONS)
G_DEFINE_TYPE (GimpPainterSmudge, gimp_painter_smudge, GIMP_TYPE_BRUSH_CORE)
namespace GimpPainter {
template<> struct TypeTraits<GimpPainterSmudgeOptions> { static GType type () { return GIMP_TYPE_PAINTER_SMUDGE_OPTIONS; } };
template<> struct TypeTraits<GimpPainterSmudge> { static GType type () { return GIMP_TYPE_PAINTER_SMUDGE; } };
template<> struct TypeTraits<GimpDrawable> { static GType type () { return GIMP_TYPE_DRAWABLE; } };
template<> struct TypeTraits<GimpImage> { static GType type () { return GIMP_TYPE_IMAGE; } };
template<> struct TypeTraits<GeglBuffer> { static GType type () { return GEGL_TYPE_BUFFER; } };
}
namespace {
struct Settings { double rate = 50; bool blending = false; void close () noexcept {} };
struct OptionsSlot : SlotSpec<GimpPainterSmudgeOptions, Settings> {};
struct Watch { std::atomic<bool> changed{false}, writing{false}, busy{false}; };
void buffer_changed (GeglBuffer*, const GeglRectangle*, gpointer data) {
  auto& watch = **static_cast<std::shared_ptr<Watch>*>(data);
  if (!watch.writing.load (std::memory_order_acquire)) watch.changed.store (true, std::memory_order_release);
}
gboolean pending_paint (GimpImage*, gpointer data) {
  return (*static_cast<std::shared_ptr<Watch>*>(data))->busy.load (std::memory_order_acquire);
}
void release_watch (gpointer data, GClosure*) { delete static_cast<std::shared_ptr<Watch>*>(data); }
struct State;
void finish_frame (State& state, bool commit);
struct State {
  explicit State (GimpPainterSmudge* value): owner(value) {}
  GimpPainterSmudge* owner;
  ObjectRef<GeglBuffer> accumulator;
  ObjectRef<GimpImage> image;
  ObjectRef<GimpDrawable> drawable;
  ObjectRef<GObject> options;
  ObjectRef<GeglBuffer> observed;
  Connection buffer_connection, pending_connection;
  std::shared_ptr<Watch> watch;
  const Babl* format = nullptr;
  int width = 0, height = 0, offset_x = 0, offset_y = 0;
  int size = 0;
  using Segment = std::unique_ptr<GimpBrushCoreInterpolation, decltype (&gimp_brush_core_interpolation_free)>;
  Segment segment {nullptr, gimp_brush_core_interpolation_free};
  GimpCoords pending_coords = GIMP_COORDS_DEFAULT_VALUES;
  guint32 pending_time = 0;
  bool first_pending = false, start_permit = false, native_start_armed = false;
  std::string error;
  bool closed = false, local_coordinates = false;
  bool active = false, starting = false, processing = false, ending = false;
  bool first = true, cancel_requested = false;
  std::uint64_t revision = 0;
  void clear () { ++revision;auto retired = std::move (accumulator);size = 0; }
  void release_frame () {
    segment.reset ();first_pending = false;start_permit = false;native_start_armed = false;clear ();auto connection = std::move (buffer_connection);connection.close ();
    auto pending = std::move (pending_connection);auto old_watch = std::move (watch);
    if (old_watch) old_watch->busy.store (false, std::memory_order_release);
    pending.close ();auto old_buffer = std::move (observed);
    auto old_options = std::move (options);auto old_drawable = std::move (drawable);auto old_image = std::move (image);
  }
  void close () noexcept {
    closed = true;cancel_requested = true;
    if (starting || processing || ending) return;
    try { finish_frame (*this, false); } catch (...) { release_frame (); }
  }
};
struct LocalCoordinates {
  GimpPaintCore* core;State& state;int x, y;
  LocalCoordinates (GimpPaintCore* c, State& s):core(c),state(s),x(s.offset_x),y(s.offset_y) {
    if (state.local_coordinates) throw std::runtime_error ("Smudge local-coordinate reentry");
    state.local_coordinates = true;
    core->cur_coords.x -= x;core->cur_coords.y -= y;
    core->last_coords.x -= x;core->last_coords.y -= y;
    core->last_paint.x -= x;core->last_paint.y -= y;
  }
  ~LocalCoordinates () {
    core->cur_coords.x += x;core->cur_coords.y += y;
    core->last_coords.x += x;core->last_coords.y += y;
    core->last_paint.x += x;core->last_paint.y += y;
    state.local_coordinates = false;
  }
};
bool frame_current (const State& state, bool writable = true) {
  auto* drawable = state.drawable.get ();
  if (!drawable || gimp_item_get_image (GIMP_ITEM (drawable)) != state.image.get ()) return false;
  if (writable && (!gimp_item_is_attached (GIMP_ITEM (drawable)) || gimp_item_is_content_locked (GIMP_ITEM (drawable), nullptr))) return false;
  int x, y;gimp_item_get_offset (GIMP_ITEM (drawable), &x, &y);
  return state.watch && !state.watch->changed.load (std::memory_order_acquire) && x == state.offset_x && y == state.offset_y &&
    gimp_item_get_width (GIMP_ITEM (drawable)) == state.width && gimp_item_get_height (GIMP_ITEM (drawable)) == state.height &&
    gimp_drawable_get_format (drawable) == state.format && gimp_drawable_get_buffer (drawable) == state.observed.get ();
}
struct SmudgeSlot : SlotSpec<GimpPainterSmudge, State> {};
struct TempDelete { void operator() (GimpTempBuf* p) const { if (p) gimp_temp_buf_unref (p); } };
using Temp = std::unique_ptr<GimpTempBuf, TempDelete>;
void options_constructed (GObject* object) {
  G_OBJECT_CLASS (gimp_painter_smudge_options_parent_class)->constructed (object);
  boundary_void (nullptr, [&] { BindingStore::require (object).activate (); });
}
void options_dispose (GObject* object) {
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_painter_smudge_options_parent_class)->dispose (object);
}
void options_set (GObject* object, guint id, const GValue* value, GParamSpec* pspec) {
  boundary_void (nullptr, [&] {
    auto& store = BindingStore::require (object);
    auto set = [&] (Settings& settings) {
      if (id == 1) settings.rate = g_value_get_double (value);
      else if (id == 2) settings.blending = g_value_get_boolean (value);
      else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    };
    if (store.state () == BindingStore::State::constructing) store.initialize<OptionsSlot> (set);
    else store.with<OptionsSlot> (set);
  });
}
void options_get (GObject* object, guint id, GValue* value, GParamSpec* pspec) {
  boundary_void (nullptr, [&] { BindingStore::require (object).read<OptionsSlot> ([&] (const Settings& settings) {
    if (id == 1) g_value_set_double (value, settings.rate);
    else if (id == 2) g_value_set_boolean (value, settings.blending);
    else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
  }); });
}
void constructed (GObject* object) {
  G_OBJECT_CLASS (gimp_painter_smudge_parent_class)->constructed (object);
  boundary_void (nullptr, [&] { BindingStore::require (object).activate (); });
}
void dispose (GObject* object) {
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_painter_smudge_parent_class)->dispose (object);
}
void coordinates_valid (const GimpCoords* coords) {
  if (!coords) throw std::invalid_argument ("Missing legacy Smudge coordinates");
  const double fields[] = {coords->x,coords->y,coords->pressure,coords->xtilt,coords->ytilt,coords->wheel,
    coords->distance,coords->rotation,coords->slider,coords->velocity,coords->direction,coords->xscale,coords->yscale,coords->angle};
  for (double value : fields)
    if (!std::isfinite (value)) throw std::invalid_argument ("Nonfinite legacy Smudge coordinate or axis");
  if (std::abs (coords->x) > G_MAXINT / 2 || std::abs (coords->y) > G_MAXINT / 2)
    throw std::invalid_argument ("Legacy Smudge coordinates exceed safe integer bounds");
}
double finite_value (double value, const char* label) {
  if (!std::isfinite (value)) throw std::invalid_argument (label);
  return value;
}

gboolean check_start (GimpPaintCore* core, GList*, GimpPaintOptions*, const GimpCoords* coords, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    coordinates_valid (coords);
    return BindingStore::require (G_OBJECT (core)).with<SmudgeSlot> ([&] (State& state) -> gboolean {
      if (state.closed || state.active || state.processing || state.ending || !state.starting || !state.start_permit)
        throw std::runtime_error ("Painter Smudge requires its owned stroke adapter; generic stroking is not integrated");
      state.start_permit = false;state.native_start_armed = true;return TRUE;
    });
  });
}
gboolean start (GimpPaintCore* core, GList* drawables, GimpPaintOptions* options,
                const GimpCoords* coords, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    const bool admitted = BindingStore::require (G_OBJECT (core)).with<SmudgeSlot> ([] (State& state) {
      const bool result = state.starting && state.native_start_armed && !state.closed;
      state.native_start_armed = false;return result;
    });
    if (!admitted) throw std::runtime_error ("Painter Smudge native start has no owned adapter permit");
    coordinates_valid (coords);
    if (!drawables || drawables->next || !GIMP_IS_DRAWABLE (drawables->data))
      throw std::invalid_argument ("Legacy Smudge requires one drawable");
    auto* drawable = GIMP_DRAWABLE (drawables->data);
    auto* image = gimp_item_get_image (GIMP_ITEM (drawable));
    if (!image || gimp_drawable_is_indexed (drawable) ||
        gimp_image_get_precision (image) != GIMP_PRECISION_U8_NON_LINEAR)
      throw std::invalid_argument ("Legacy Smudge requires nonlinear byte RGB or Gray");
    if (!G_TYPE_CHECK_INSTANCE_TYPE (options, GIMP_TYPE_PAINTER_SMUDGE_OPTIONS))
      throw std::invalid_argument ("Invalid legacy Smudge options");
    return GIMP_PAINT_CORE_CLASS (gimp_painter_smudge_parent_class)->start (core, drawables, options, coords, error);
  });
}
std::pair<int,int> original_dimensions (GimpBrush* brush) {
  if (GIMP_IS_BRUSH_GENERATED (brush)) {
    auto* generated = GIMP_BRUSH_GENERATED (brush);
    return MyPaint::legacy_generated_dimensions (static_cast<MyPaint::GeneratedShape> (generated->shape),
      generated->radius, generated->spikes, generated->hardness, generated->aspect_ratio, generated->angle);
  }
  const auto* mask = gimp_brush_get_mask (brush);
  if (!mask) throw std::runtime_error ("Legacy Smudge brush has no mask");
  return {gimp_temp_buf_get_width (mask), gimp_temp_buf_get_height (mask)};
}
void transform_dynamics (GimpBrushCore* brush, GimpImage* image, GimpPaintOptions* options,
                         const GimpCoords& coords, bool ignore_size) {
  const auto dimensions = original_dimensions (brush->main_brush);
  const int largest = std::max (dimensions.first, dimensions.second);
  if (largest <= 0) throw std::runtime_error ("Legacy Smudge brush has empty dimensions");
  brush->scale = options->brush_size / largest;brush->angle = options->brush_angle;
  brush->aspect_ratio = options->brush_aspect_ratio;brush->reflect = FALSE;brush->hardness = 1;
  if (gimp_paint_options_are_dynamics_enabled (options)) {
    const double fade = gimp_paint_options_get_fade (options, image, GIMP_PAINT_CORE (brush)->pixel_dist);
    if (!ignore_size) brush->scale *= gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_SIZE, &coords, options, fade);
    brush->angle += gimp_dynamics_get_angular_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_ANGLE, &coords, options, fade);
    brush->hardness = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_HARDNESS, &coords, options, fade);
    if (gimp_dynamics_is_output_enabled (brush->dynamics, GIMP_DYNAMICS_OUTPUT_ASPECT_RATIO)) {
      const double aspect = gimp_dynamics_get_aspect_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_ASPECT_RATIO, &coords, options, fade);
      brush->aspect_ratio = brush->aspect_ratio == 0 ? 10 * aspect : brush->aspect_ratio * aspect;
    }
  }
  finite_value (brush->scale, "Nonfinite Smudge scale");finite_value (brush->angle, "Nonfinite Smudge angle");
  finite_value (brush->aspect_ratio, "Nonfinite Smudge aspect");finite_value (brush->hardness, "Nonfinite Smudge hardness");
}
MyPaint::ShapeMask transformed_mask (GimpBrushCore* brush) {
  if (brush->reflect) throw std::runtime_error ("Legacy Smudge reflected brush needs explicit transform integration");
  if (GIMP_IS_BRUSH_GENERATED (brush->brush)) {
    auto* generated = GIMP_BRUSH_GENERATED (brush->brush);
    const double ratio = brush->aspect_ratio == 0 ? generated->aspect_ratio : std::min (std::abs (brush->aspect_ratio) + 1, 20.0);
    const double turns = brush->angle + (brush->aspect_ratio < 0 ? .25 : 0);
    return MyPaint::generate_legacy_mask (static_cast<MyPaint::GeneratedShape> (generated->shape), generated->radius * brush->scale,
                                        generated->spikes, generated->hardness * brush->hardness, ratio, generated->angle + 360 * turns);
  }
  const auto* mask = gimp_brush_get_mask (brush->brush);
  if (!mask) throw std::runtime_error ("Legacy Smudge brush has no mask");
  MyPaint::ShapeMask source;source.width = gimp_temp_buf_get_width (mask);source.height = gimp_temp_buf_get_height (mask);
  source.pixels.resize (std::size_t (source.width) * source.height);
  babl_process (babl_fish (gimp_temp_buf_get_format (mask), babl_format ("Y u8")), gimp_temp_buf_get_data (mask), source.pixels.data (), source.pixels.size ());
  return MyPaint::transform_bitmap_mask (source, brush->scale, brush->aspect_ratio, brush->angle, brush->hardness);
}
Temp legacy_subsample (const MyPaint::ShapeMask& mask, double x, double y, double force, bool pressure) {
  // Preserve the old asymmetric row rounding: +127 during input rows, +128
  // while flushing the final two kernel rows. Modern BrushCore rounds both up.
  if (x < 0) x -= std::floor (x / mask.width) * mask.width;
  if (y < 0) y -= std::floor (y / mask.height) * mask.height;
  int ix = int ((x - std::floor (x)) * 5), iy = int ((y - std::floor (y)) * 5);
  int dx = 0, dy = 0;
  if (!(mask.width & 1)) { ix += 2;if (ix > 4) { ix -= 5;dx = 1; } }
  if (!(mask.height & 1)) { iy += 2;if (iy > 4) { iy -= 5;dy = 1; } }
  const auto* kernel = Subsample<guchar>::kernel[iy][ix];
  Temp result (gimp_temp_buf_new (mask.width + 2, mask.height + 2, babl_format ("Y u8")));
  gimp_temp_buf_data_clear (result.get ());
  const int width = mask.width + 2, height = mask.height + 2;
  std::array<std::vector<unsigned>, 3> rows;
  for (auto& row : rows) row.resize (width + 1);
  auto rotate = [&] { std::swap (rows[0], rows[1]);std::swap (rows[1], rows[2]); };
  auto* output = gimp_temp_buf_get_data (result.get ());
  int row = 0;
  for (; row < mask.height; ++row) {
    for (int column = 0; column < mask.width; ++column)
      for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
          rows[r][column + dx + c] += mask.pixels[std::size_t (row) * mask.width + column] * kernel[r * 3 + c];
    for (int column = 0; column < width; ++column) output[std::size_t (row + dy) * width + column] = (rows[0][column] + 127) / 256;
    rotate ();std::fill_n (rows[2].begin (), width, 0);
  }
  for (; row + dy < height; ++row) {
    for (int column = 0; column < width; ++column) output[std::size_t (row + dy) * width + column] = (rows[0][column] + 128) / 256;
    rotate ();
  }
  if (pressure && int (force * 100 + .5) != 50) {
    std::uint8_t mapping[256];double value = 0;
    for (unsigned n = 0; n < 256; ++n) { mapping[n] = value > 255 ? 255 : int (value);value += force + force; }
    for (int n = 0; n < width * height; ++n) output[n] = mapping[output[n]];
  }
  return result;
}
GeglRectangle brush_area (GimpDrawable* drawable, const GimpCoords& coords, int width, int height) {
  if (width <= 0 || height <= 0) return {0, 0, 0, 0};
  const int x = int (std::floor (coords.x)) - width / 2;
  const int y = int (std::floor (coords.y)) - height / 2;
  const int dw = gimp_item_get_width (GIMP_ITEM (drawable)), dh = gimp_item_get_height (GIMP_ITEM (drawable));
  const int x1 = CLAMP (x - 1, 0, dw), y1 = CLAMP (y - 1, 0, dh);
  const int x2 = CLAMP (std::int64_t (x) + width + 1, 0, dw), y2 = CLAMP (std::int64_t (y) + height + 1, 0, dh);
  return {x1, y1, x2 - x1, y2 - y1};
}

void motion (GimpPaintCore* core, GimpDrawable* drawable, GimpPaintOptions* options,
             GimpSymmetry* symmetry, State& state) {
  auto target = ObjectRef<GimpDrawable>::retain (drawable);
  auto image = ObjectRef<GimpImage>::retain (gimp_item_get_image (GIMP_ITEM (drawable)));
  if (!image || !gimp_item_is_attached (GIMP_ITEM (drawable))) throw std::runtime_error ("Detached legacy Smudge target");
  if (gimp_symmetry_get_size (symmetry) != 1) throw std::runtime_error ("Legacy Smudge symmetry requires explicit integration");
  auto* brush = GIMP_BRUSH_CORE (core);
  auto option_lease = ObjectRef<GObject>::retain (G_OBJECT (options));
  auto main_brush = ObjectRef<GObject>::retain (G_OBJECT (brush->main_brush));
  auto selected_brush = ObjectRef<GObject>::retain (G_OBJECT (brush->brush));
  auto dynamics = ObjectRef<GObject>::retain (G_OBJECT (brush->dynamics));
  if (!main_brush || !selected_brush || !dynamics) throw std::runtime_error ("Legacy Smudge resources are unavailable");
  auto buffer = ObjectRef<GeglBuffer>::retain (gimp_drawable_get_buffer (drawable));
  const Babl* format = gimp_drawable_get_format (drawable);
  const int original_width = gimp_item_get_width (GIMP_ITEM (drawable)), original_height = gimp_item_get_height (GIMP_ITEM (drawable));
  const auto revision = state.revision;
  auto* context = GIMP_CONTEXT (options);
  GimpCoords coords = *gimp_symmetry_get_origin (symmetry);
  int ox, oy;gimp_item_get_offset (GIMP_ITEM (drawable), &ox, &oy);if (!state.local_coordinates) { coords.x -= ox;coords.y -= oy; }coordinates_valid (&coords);
  const auto current = [&] {
    int x, y;gimp_item_get_offset (GIMP_ITEM (drawable), &x, &y);
    if (state.closed || state.cancel_requested || state.revision != revision || !gimp_item_is_attached (GIMP_ITEM (drawable)) ||
        gimp_item_get_image (GIMP_ITEM (drawable)) != image.get () || gimp_item_is_content_locked (GIMP_ITEM (drawable), nullptr) ||
        x != ox || y != oy || gimp_item_get_width (GIMP_ITEM (drawable)) != original_width ||
        gimp_item_get_height (GIMP_ITEM (drawable)) != original_height || gimp_drawable_get_format (drawable) != format ||
        gimp_drawable_get_buffer (drawable) != buffer.get () || G_OBJECT (brush->main_brush) != main_brush.get () ||
        G_OBJECT (brush->brush) != selected_brush.get () || G_OBJECT (brush->dynamics) != dynamics.get () ||
        G_OBJECT (gimp_context_get_brush (context)) != main_brush.get () || G_OBJECT (gimp_context_get_dynamics (context)) != dynamics.get ())
      throw std::runtime_error ("Legacy Smudge changed during resource preparation or publication");
  };
  current ();
  const double fade = gimp_paint_options_get_fade (options, image.get (), core->pixel_dist);
  const double opacity = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_OPACITY, &coords, options, fade);
  finite_value (opacity, "Nonfinite Smudge opacity");
  if (opacity == 0) return;
  const Settings settings = BindingStore::require (G_OBJECT (options)).read<OptionsSlot> ([] (const Settings& s) { return s; });
  brush->spacing = gimp_brush_get_spacing (brush->main_brush) / 100.0;
  // The old accumulation footprint explicitly ignored size dynamics, and
  // its hardness came from dynamics rather than GIMP3's constant brush slider.
  transform_dynamics (brush, image.get (), options, coords, true);
  const auto footprint = transformed_mask (brush);
  current ();
  const auto area = brush_area (drawable, coords, footprint.width, footprint.height);
  if (!area.width || !area.height) return;
  const bool gray = gimp_drawable_is_gray (drawable), alpha = gimp_drawable_has_alpha (drawable);
  const unsigned channels = (gray ? 1 : 3) + alpha;
  const Babl* space = babl_format_get_space (gimp_drawable_get_format (drawable));
  const Babl* native = babl_format_with_space (gray ? (alpha ? "Y'A u8" : "Y' u8") : (alpha ? "R'G'B'A u8" : "R'G'B' u8"), space);
  if (!state.accumulator) {
    state.size = int (std::ceil (std::hypot (double (footprint.width), double (footprint.height)))) + 2;
    if (state.size <= 2 || state.size > 32768) throw std::runtime_error ("Legacy Smudge accumulator exceeds safe bounds");
    GeglRectangle extent = {0, 0, state.size, state.size};
    state.accumulator = ObjectRef<GeglBuffer>::adopt (gegl_buffer_new (&extent, native));
    std::uint8_t fill[4] = {};
    GeglRectangle seed = {CLAMP (int (coords.x), 0, gimp_item_get_width (GIMP_ITEM (drawable)) - 1), CLAMP (int (coords.y), 0, gimp_item_get_height (GIMP_ITEM (drawable)) - 1), 1, 1};
    gegl_buffer_get (gimp_drawable_get_buffer (drawable), &seed, 1, native, fill, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    auto* color = gegl_color_new (nullptr);gegl_color_set_pixel (color, native, fill);gegl_buffer_set_color (state.accumulator.get (), nullptr, color);g_object_unref (color);
    const int x = int (coords.x) - (state.size - 2) / 2 - 1, y = int (coords.y) - (state.size - 2) / 2 - 1;
    GeglRectangle destination = {area.x - x, area.y - y, area.width, area.height};
    gegl_buffer_copy (gimp_drawable_get_buffer (drawable), &area, GEGL_ABYSS_NONE, state.accumulator.get (), &destination);
  }
  const int x = int (coords.x) - (state.size - 2) / 2 - 1, y = int (coords.y) - (state.size - 2) / 2 - 1;
  GeglRectangle accum_area = {area.x - x, area.y - y, area.width, area.height};
  if (accum_area.x < 0 || accum_area.y < 0 || accum_area.x + accum_area.width > state.size || accum_area.y + accum_area.height > state.size)
    throw std::runtime_error ("Legacy Smudge geometry outgrew its fixed accumulator");
  const std::size_t count = std::size_t (area.width) * area.height;
  std::vector<std::uint8_t> sampled (count * channels), accumulated (count * channels), shaded (count * channels);
  gegl_buffer_get (gimp_drawable_get_buffer (drawable), &area, 1, native, sampled.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  gegl_buffer_get (state.accumulator.get (), &accum_area, 1, native, accumulated.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  const double rate = settings.rate / 100 * gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_RATE, &coords, options, fade);
  Smudge::accumulate (sampled.data (), accumulated.data (), accumulated.size (), channels, std::uint8_t (CLAMP (std::floor (finite_value (rate, "Nonfinite Smudge rate") * 255 + .5), 0, 255)));
  gegl_buffer_set (state.accumulator.get (), &accum_area, 0, native, accumulated.data (), GEGL_AUTO_ROWSTRIDE);
  const auto* pixels = accumulated.data ();
  if (settings.blending) {
    double rgba[4];std::uint8_t color[4] = {};
    gegl_color_get_pixel (gimp_context_get_foreground (context), babl_format ("R'G'B'A double"), rgba);
    for (unsigned c = 0; c < 3; ++c) color[c] = std::uint8_t (CLAMP (std::floor (finite_value (rgba[c], "Nonfinite Smudge foreground") * 255 + .5), 0, 255));
    if (gray) color[0] = std::uint8_t (color[0] * .2126 + color[1] * .7152 + color[2] * .0722 + .5);
    if (alpha) color[channels - 1] = 255;
    const double blending = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING, &coords, options, fade);
    Smudge::shade (accumulated.data (), shaded.data (), shaded.size (), channels, color, std::uint8_t (CLAMP (std::floor (finite_value (blending, "Nonfinite Smudge blending") * 255 + .5), 0, 255)));
    pixels = shaded.data ();
  }
  transform_dynamics (brush, image.get (), options, coords, false);
  const auto dimensions = original_dimensions (brush->main_brush);
  brush->scale = std::max (.5 / float (std::min (dimensions.first, dimensions.second)), brush->scale);
  const auto transformed = transformed_mask (brush);
  current ();
  const auto crop = brush_area (drawable, coords, transformed.width, transformed.height);
  if (!crop.width || !crop.height) return;
  if (crop.x < area.x || crop.y < area.y || crop.x + crop.width > area.x + area.width || crop.y + crop.height > area.y + area.height)
    throw std::runtime_error ("Legacy Smudge dynamic paint exceeds accumulation footprint");
  const double force = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_HARDNESS, &coords, options, fade);
    Temp transformed_temp (gimp_temp_buf_new (transformed.width, transformed.height, babl_format ("Y u8")));
    std::copy (transformed.pixels.begin (), transformed.pixels.end (), gimp_temp_buf_get_data (transformed_temp.get ()));
    brush->subsample_cache_invalid = TRUE;brush->solid_cache_invalid = TRUE;
    const auto brush_mode = options->hard ? GIMP_BRUSH_HARD :
      gimp_dynamics_is_output_enabled (brush->dynamics, GIMP_DYNAMICS_OUTPUT_FORCE) ? GIMP_BRUSH_PRESSURE : GIMP_BRUSH_SOFT;
    Temp legacy_mask;
    const GimpTempBuf* mask = nullptr;
    if (brush_mode == GIMP_BRUSH_HARD)
      mask = gimp_brush_core_solidify_mask (brush, transformed_temp.get (), coords.x, coords.y);
    else {
      legacy_mask = legacy_subsample (transformed, coords.x, coords.y, force, brush_mode == GIMP_BRUSH_PRESSURE);
      mask = legacy_mask.get ();
    }
    if (mask) mask = gimp_brush_core_texturize_mask (brush, mask, coords.x, coords.y);
    if (!mask) throw std::runtime_error ("Legacy Smudge paper mask could not be generated");
    const int mx = int (std::floor (coords.x)) - (gimp_temp_buf_get_width (mask) >> 1);
    const int my = int (std::floor (coords.y)) - (gimp_temp_buf_get_height (mask) >> 1);
    const auto* mask_data = gimp_temp_buf_get_data (mask);
    const int mask_width = gimp_temp_buf_get_width (mask), mask_height = gimp_temp_buf_get_height (mask);
    if (crop.x - mx < 0 || crop.y - my < 0 || crop.x - mx + crop.width > mask_width || crop.y - my + crop.height > mask_height)
      throw std::runtime_error ("Legacy Smudge mask exceeds transformed bounds");
    std::vector<std::uint8_t> result (std::size_t (crop.width) * crop.height * channels);
    std::vector<std::uint8_t> coverage (std::size_t (crop.width) * crop.height);
    gegl_buffer_get (gimp_drawable_get_buffer (drawable), &crop, 1, native, result.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    for (int row = 0; row < crop.height; ++row)
      std::copy_n (mask_data + (std::size_t (crop.y - my + row) * mask_width + crop.x - mx), crop.width, coverage.data () + std::size_t (row) * crop.width);
    std::vector<std::uint8_t> selection;
    if (core->mask_buffer) {
      selection.resize (coverage.size ());GeglRectangle rectangle = {crop.x + ox, crop.y + oy, crop.width, crop.height};
      gegl_buffer_get (core->mask_buffer, &rectangle, 1, babl_format ("Y u8"), selection.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (alpha) for (std::size_t n = 0; n < coverage.size (); ++n) coverage[n] = Smudge::multiply (selection[n], coverage[n]);
    }
    const auto components = gimp_drawable_get_active_mask (drawable);
    bool affect[4] = {bool (components & GIMP_COMPONENT_MASK_RED), bool (components & GIMP_COMPONENT_MASK_GREEN), bool (components & GIMP_COMPONENT_MASK_BLUE), bool (components & GIMP_COMPONENT_MASK_ALPHA)};
    if (gray) affect[1] = affect[3];
    const unsigned image_opacity = CLAMP (gimp_context_get_opacity (context) * 255.999, 0, 255);
    for (int row = 0; row < crop.height; ++row) {
      const auto* source_row = pixels + (std::size_t (crop.y - area.y + row) * area.width + crop.x - area.x) * channels;
      auto* result_row = result.data () + std::size_t (row) * crop.width * channels;
      const auto* coverage_row = coverage.data () + std::size_t (row) * crop.width;
      if (alpha) Smudge::replace (source_row, result_row, coverage_row, crop.width, channels, image_opacity, affect);
      else Smudge::opaque_paint (source_row, result_row, coverage_row,
                                selection.empty () ? nullptr : selection.data () + std::size_t (row) * crop.width,
                                crop.width, channels, CLAMP (opacity * 255.999, 0, 255), image_opacity, affect);
    }
    // Native start already owns the original COW undo buffer; the exact byte
    // renderer only publishes pixels and marks the native transaction bounds.
    core->x1 = std::min (core->x1, crop.x);core->y1 = std::min (core->y1, crop.y);
    core->x2 = std::max (core->x2, crop.x + crop.width);core->y2 = std::max (core->y2, crop.y + crop.height);
    current ();
    auto watch = state.watch;
    if (watch) watch->writing.store (true, std::memory_order_release);
    gegl_buffer_set (buffer.get (), &crop, 0, native, result.data (), GEGL_AUTO_ROWSTRIDE);
    if (watch) watch->writing.store (false, std::memory_order_release);
    current ();
    if (!state.closed) gimp_drawable_update (drawable, crop.x, crop.y, crop.width, crop.height);
    current ();
}
void interpolate (GimpPaintCore* core, GList* drawables, GimpPaintOptions* options, guint32 time) {
  boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (core)).with<SmudgeSlot> ([&] (State& state) {
    try {
    coordinates_valid (&core->cur_coords);coordinates_valid (&core->last_coords);
    if (state.local_coordinates || !drawables || drawables->next || !GIMP_IS_DRAWABLE (drawables->data)) throw std::runtime_error ("Legacy Smudge interpolation reentry");
    auto drawable = ObjectRef<GimpDrawable>::retain (GIMP_DRAWABLE (drawables->data));
    auto image = ObjectRef<GimpImage>::retain (gimp_item_get_image (GIMP_ITEM (drawable.get ())));
    auto option_lease = ObjectRef<GObject>::retain (G_OBJECT (options));
    if (!image) throw std::runtime_error ("Detached legacy Smudge interpolation image");
    int x, y;gimp_item_get_offset (GIMP_ITEM (drawable.get ()), &x, &y);
    // The old native iterator evaluated drawable-local doubles. Moving the
    // integer offset outside its arithmetic also preserves subpixel tie cases.
    struct Local {
      GimpPaintCore* core;State& state;int x, y;
      Local (GimpPaintCore* c, State& s, int a, int b):core(c),state(s),x(a),y(b) {
        state.local_coordinates = true;
        core->cur_coords.x -= x;core->cur_coords.y -= y;
        core->last_coords.x -= x;core->last_coords.y -= y;
        core->last_paint.x -= x;core->last_paint.y -= y;
      }
      ~Local () {
        core->cur_coords.x += x;core->cur_coords.y += y;
        core->last_coords.x += x;core->last_coords.y += y;
        core->last_paint.x += x;core->last_paint.y += y;
        state.local_coordinates = false;
      }
    } local (core, state, x, y);
    std::unique_ptr<GimpBrushCoreInterpolation, decltype (&gimp_brush_core_interpolation_free)> continuation
      (gimp_brush_core_interpolation_begin (GIMP_BRUSH_CORE (core), drawables, options, &core->cur_coords, time), gimp_brush_core_interpolation_free);
    if (!continuation) throw std::runtime_error ("Unrepresentable legacy Smudge interpolation");
    while (!state.closed && !state.cancel_requested && state.error.empty () &&
           !gimp_brush_core_interpolation_step (GIMP_BRUSH_CORE (core), drawables, options, continuation.get (), 1)) {}
    } catch (const std::exception& error) { state.error = error.what ();state.clear (); }
  }); });
}
void paint (GimpPaintCore* core, GList* drawables, GimpPaintOptions* options,
            GimpSymmetry* symmetry, GimpPaintState phase, guint32) {
  boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (core)).with<SmudgeSlot> ([&] (State& state) {
    if (phase == GIMP_PAINT_STATE_INIT) { state.clear (); state.error.clear (); return; }
    if (phase == GIMP_PAINT_STATE_FINISH) { state.clear (); return; }
    if (state.closed || !state.error.empty ()) return;
    try {
      if (!drawables || drawables->next || !GIMP_IS_DRAWABLE (drawables->data) ||
          !options || !G_TYPE_CHECK_INSTANCE_TYPE (options, GIMP_TYPE_PAINTER_SMUDGE_OPTIONS) || !GIMP_IS_SYMMETRY (symmetry))
        throw std::invalid_argument ("Invalid legacy Smudge paint arguments");
      motion (core, GIMP_DRAWABLE (drawables->data), options, symmetry, state);
    }
    catch (const std::exception& error) { state.error = error.what (); state.clear (); }
  }); });
}
void release_scratch (GimpPaintCore* core) {
  g_clear_object (&core->mask_buffer);
  if (core->applicators) { g_hash_table_unref (core->applicators);core->applicators = nullptr; }
  if (core->stroke_buffer) { g_array_free (core->stroke_buffer, TRUE);core->stroke_buffer = nullptr; }
  core->image_pickable = nullptr;gimp_paint_core_cleanup (core);
}
void finish_frame (State& state, bool commit) {
  if (state.starting || state.processing) { state.cancel_requested = true;return; }
  if (state.ending) return;
  if (!state.active) { state.release_frame ();return; }
  state.ending = true;state.active = false;
  struct Ending { bool& value;~Ending () { value = false; } } ending {state.ending};
  auto image = state.image;auto drawable = state.drawable;auto options = state.options;
  auto* core = GIMP_PAINT_CORE (state.owner);GList list = {drawable.get (), nullptr, nullptr};
  const bool safe = frame_current (state, false);state.buffer_connection.close ();
  if (!state.closed) gimp_paint_core_paint (core, &list, GIMP_PAINT_OPTIONS (options.get ()), GIMP_PAINT_STATE_FINISH, 0);
  if (!safe || core->x1 == core->x2 || core->y1 == core->y2) gimp_paint_core_finish (core, &list, FALSE);
  else if (commit && !state.cancel_requested && state.error.empty ()) gimp_paint_core_finish (core, &list, TRUE);
  else gimp_paint_core_cancel (core, &list);
  release_scratch (core);state.release_frame ();
}

}
static void gimp_painter_smudge_options_class_init (GimpPainterSmudgeOptionsClass* klass) {
  auto* object = G_OBJECT_CLASS (klass);object->constructed = options_constructed;object->dispose = options_dispose;object->get_property = options_get;object->set_property = options_set;
  GIMP_CONFIG_PROP_DOUBLE (object, 1, "rate", _("Rate"), _("Legacy Smudge accumulation rate"), 0, 100, 50, GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_PROP_BOOLEAN (object, 2, "use-color-blending", _("Color blending"), _("Shade Smudge output with the independent blending dynamics"), FALSE, GIMP_PARAM_STATIC_STRINGS);
}
static void gimp_painter_smudge_options_init (GimpPainterSmudgeOptions* options) {
  options->binding_failed = !boundary<bool> (nullptr, false, [&] { BindingStore::ensure (G_OBJECT (options)).emplace<OptionsSlot> (); return true; });
}
static void gimp_painter_smudge_class_init (GimpPainterSmudgeClass* klass) {
  auto* object = G_OBJECT_CLASS (klass);object->constructed = constructed;object->dispose = dispose;
  auto* paint_class = GIMP_PAINT_CORE_CLASS (klass);paint_class->check_start = check_start;paint_class->start = start;paint_class->paint = paint;paint_class->interpolate = interpolate;
  auto* brush = GIMP_BRUSH_CORE_CLASS (klass);brush->handles_changing_brush = TRUE;brush->handles_transforming_brush = TRUE;brush->handles_dynamic_transforming_brush = TRUE;
}
static void gimp_painter_smudge_init (GimpPainterSmudge* smudge) {
  smudge->binding_failed = !boundary<bool> (nullptr, false, [&] { BindingStore::ensure (G_OBJECT (smudge)).emplace<SmudgeSlot> (smudge); return true; });
}
void gimp_painter_smudge_register (Gimp* gimp, GimpPaintRegisterCallback callback) {
  callback (gimp, GIMP_TYPE_PAINTER_SMUDGE, GIMP_TYPE_PAINTER_SMUDGE_OPTIONS, "gimp-painter-smudge", _("Painter Smudge"), "gimp-tool-smudge");
}
gchar* gimp_painter_smudge_dup_error (GimpPainterSmudge* smudge) {
  return boundary<gchar*> (nullptr, nullptr, [&] { return BindingStore::require (G_OBJECT (smudge)).read<SmudgeSlot> ([] (const State& state) { return state.closed ? g_strdup ("Legacy Smudge owner is closed") : state.error.empty () ? nullptr : g_strdup (state.error.c_str ()); }); });
}

gboolean gimp_painter_smudge_begin (GimpPainterSmudge* smudge, GimpDrawable* drawable,
                                    GimpPaintOptions* options, const GimpCoords* coords, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    coordinates_valid (coords);
    return BindingStore::require (G_OBJECT (smudge)).with<SmudgeSlot> ([&] (State& state) -> gboolean {
      if (state.active || state.starting || state.processing || state.ending || state.closed) throw std::runtime_error ("Legacy Smudge already active or closed");
      auto target = ObjectRef<GimpDrawable>::retain (drawable);
      if (!target || !gimp_item_is_attached (GIMP_ITEM (drawable)) || gimp_item_is_content_locked (GIMP_ITEM (drawable), nullptr)) throw std::invalid_argument ("Legacy Smudge target unavailable");
      auto image = ObjectRef<GimpImage>::retain (gimp_item_get_image (GIMP_ITEM (drawable)));
      if (!options || !G_TYPE_CHECK_INSTANCE_TYPE (options, GIMP_TYPE_PAINTER_SMUDGE_OPTIONS)) throw std::invalid_argument ("Invalid legacy Smudge options");
      state.release_frame ();state.image = image;state.drawable = target;state.options = ObjectRef<GObject>::retain (G_OBJECT (options));
      state.observed = ObjectRef<GeglBuffer>::retain (gimp_drawable_get_buffer (drawable));state.format = gimp_drawable_get_format (drawable);
      state.width = gimp_item_get_width (GIMP_ITEM (drawable));state.height = gimp_item_get_height (GIMP_ITEM (drawable));
      gimp_item_get_offset (GIMP_ITEM (drawable), &state.offset_x, &state.offset_y);
      state.watch = std::make_shared<Watch> ();auto data = std::unique_ptr<std::shared_ptr<Watch>> (new std::shared_ptr<Watch> (state.watch));
      state.buffer_connection = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT (state.observed.get ())), "changed", G_CALLBACK (buffer_changed), data.get (), release_watch);data.release ();
      auto query = std::unique_ptr<std::shared_ptr<Watch>> (new std::shared_ptr<Watch> (state.watch));
      state.watch->busy.store (true, std::memory_order_release);
      state.pending_connection = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT (image.get ())), "query-pending-paint", G_CALLBACK (pending_paint), query.get (), release_watch);query.release ();
      state.first = true;state.cancel_requested = false;state.error.clear ();state.starting = true;
      auto* core = GIMP_PAINT_CORE (smudge);GList list = {drawable, nullptr, nullptr};GError* native_error = nullptr;
      state.start_permit = true;
      const bool started = gimp_paint_core_start (core, &list, options, coords, &native_error);state.start_permit = false;state.native_start_armed = false;state.starting = false;
      if (!started) { std::string message = native_error ? native_error->message : "Legacy Smudge start failed";g_clear_error (&native_error);release_scratch (core);state.release_frame ();throw std::runtime_error (message); }
      state.active = true;
      if (state.closed || state.cancel_requested || !frame_current (state)) { finish_frame (state, false);throw std::runtime_error ("Legacy Smudge start cancelled"); }
      gimp_paint_core_paint (core, &list, options, GIMP_PAINT_STATE_INIT, 0);
      return TRUE;
    });
  });
}
gboolean gimp_painter_smudge_motion_begin (GimpPainterSmudge* smudge, const GimpCoords* coords,
                                           guint32 time, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    coordinates_valid (coords);
    return BindingStore::require (G_OBJECT (smudge)).with<SmudgeSlot> ([&] (State& state) -> gboolean {
      if (!state.active || state.starting || state.processing || state.ending || state.closed || state.segment || state.first_pending)
        throw std::runtime_error ("Legacy Smudge input outside an available transaction");
      if (!frame_current (state)) { finish_frame (state, false);throw std::runtime_error ("Legacy Smudge target changed; independent pixels preserved"); }
      if (state.first) { state.pending_coords = *coords;state.pending_time = time;state.first_pending = true;return TRUE; }
      auto image = state.image;auto drawable = state.drawable;auto options = state.options;
      auto* core = GIMP_PAINT_CORE (smudge);GList list = {drawable.get (), nullptr, nullptr};
      struct Flag { bool& value;Flag(bool& v):value(v){value=true;}~Flag(){value=false;} } flag(state.processing);
      LocalCoordinates local (core, state);
      GimpCoords target = *coords;target.x -= state.offset_x;target.y -= state.offset_y;
      state.segment.reset (gimp_brush_core_interpolation_begin (GIMP_BRUSH_CORE (core), &list,
        GIMP_PAINT_OPTIONS (options.get ()), &target, time));
      if (!state.segment) throw std::runtime_error ("Unrepresentable legacy Smudge interpolation");
      return TRUE;
    });
  });
}
gboolean gimp_painter_smudge_step (GimpPainterSmudge* smudge, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    return BindingStore::require (G_OBJECT (smudge)).with<SmudgeSlot> ([&] (State& state) -> gboolean {
      if (!state.active || state.starting || state.processing || state.ending || state.closed)
        throw std::runtime_error ("Legacy Smudge step outside active transaction");
      if (!frame_current (state)) { finish_frame (state, false);throw std::runtime_error ("Legacy Smudge target changed; independent pixels preserved"); }
      auto image = state.image;auto drawable = state.drawable;auto options = state.options;
      auto* core = GIMP_PAINT_CORE (smudge);GList list = {drawable.get (), nullptr, nullptr};
      bool done = true;
      {
        struct Flag { bool& value;Flag(bool& v):value(v){value=true;}~Flag(){value=false;} } flag(state.processing);
        if (state.first_pending) {
          state.first_pending = false;state.first = false;
          gimp_paint_core_set_current_coords (core, &state.pending_coords);
          gimp_paint_core_paint (core, &list, GIMP_PAINT_OPTIONS (options.get ()), GIMP_PAINT_STATE_MOTION, state.pending_time);
          gimp_paint_core_set_last_coords (core, &state.pending_coords);
        } else if (state.segment) {
          LocalCoordinates local (core, state);
          done = gimp_brush_core_interpolation_step (GIMP_BRUSH_CORE (core), &list,
            GIMP_PAINT_OPTIONS (options.get ()), state.segment.get (), 1);
        }
      }
      if (done) state.segment.reset ();
      if (state.closed || state.cancel_requested || !state.error.empty ()) {
        const std::string message = state.error.empty () ? "Legacy Smudge cancelled" : state.error;
        finish_frame (state, false);throw std::runtime_error (message);
      }
      return done;
    });
  });
}
gboolean gimp_painter_smudge_motion (GimpPainterSmudge* smudge, const GimpCoords* coords,
                                     guint32 time, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    auto lease = ObjectRef<GimpPainterSmudge>::retain (smudge);
    GError* failure = nullptr;
    if (gimp_painter_smudge_motion_begin (smudge, coords, time, &failure))
      while (!gimp_painter_smudge_step (smudge, &failure) && !failure) {}
    if (failure) { g_propagate_error (error, failure);return FALSE; }
    return TRUE;
  });
}
void gimp_painter_smudge_cancel_pending (GimpPainterSmudge* smudge) {
  boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (smudge)).with<SmudgeSlot> ([] (State& state) {
    state.cancel_requested = true;finish_frame (state, false);
  }); });
}
gboolean gimp_painter_smudge_finish (GimpPainterSmudge* smudge, gboolean commit, GError** error) {
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    return BindingStore::require (G_OBJECT (smudge)).with<SmudgeSlot> ([&] (State& state) -> gboolean {
      if (!commit) state.cancel_requested = true;
      if (state.starting || state.processing || state.ending) return FALSE;
      if (commit && (state.segment || state.first_pending)) return FALSE;
      if (state.active && !frame_current (state)) { finish_frame (state, false);throw std::runtime_error ("Legacy Smudge target changed; independent pixels preserved"); }
      const std::string failure = state.error;finish_frame (state, commit);
      if (commit && !failure.empty ()) throw std::runtime_error (failure);
      return TRUE;
    });
  });
}
