/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "tools-types.h"
#include "config/gimpguiconfig.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpdata.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"
#include "paint/gimpfillbrush.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimphelp-ids.h"
#include "gimpfillbrushtool.h"
#include "gimppaintoptions-gui.h"
#include "gimptoolcontrol.h"
#include "gimp-intl.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter/source.hpp"
#include <cmath>
#include <deque>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace GimpPainter;
G_DEFINE_TYPE (GimpFillBrushTool, gimp_fill_brush_tool, GIMP_TYPE_BRUSH_TOOL)
namespace GimpPainter {
template<> struct TypeTraits<GimpFillBrushTool> { static GType type () { return GIMP_TYPE_FILL_BRUSH_TOOL; } };
template<> struct TypeTraits<GimpFillBrush> { static GType type () { return GIMP_TYPE_FILL_BRUSH; } };
template<> struct TypeTraits<GimpDrawable> { static GType type () { return GIMP_TYPE_DRAWABLE; } };
template<> struct TypeTraits<GimpImage> { static GType type () { return GIMP_TYPE_IMAGE; } };
template<> struct TypeTraits<GimpPaintOptions> { static GType type () { return GIMP_TYPE_PAINT_OPTIONS; } };
}
namespace {
bool valid_input (const GimpCoords *coords) {
  if (!coords) return false;
  const double values[] = {coords->x, coords->y, coords->pressure, coords->xtilt,
    coords->ytilt, coords->wheel, coords->distance, coords->rotation, coords->slider,
    coords->velocity, coords->direction, coords->xscale, coords->yscale, coords->angle};
  for (double value : values) if (!std::isfinite (value)) return false;
  return std::abs (coords->x) <= G_MAXINT / 2 && std::abs (coords->y) <= G_MAXINT / 2;
}
struct Input { GimpCoords coords; guint32 time; };
struct Stroke {
  ObjectRef<GimpDrawable> drawable;
  ObjectRef<GimpImage> image;
  ObjectRef<GimpPaintOptions> options;
  ObjectRef<GimpFillBrush> core;
  std::deque<Input> inputs;
  bool released = false, cancelled = false, started = false, draining = false;
  bool first_input = true, line = false;
};
struct Controller;
struct ToolSlot : SlotSpec<GimpFillBrushTool, Controller> {};
template<class F> void with (GimpTool *tool, F function)
{
  boundary_void (nullptr, [&] {
    auto *store = BindingStore::find (G_OBJECT (tool));
    if (store && store->state () == BindingStore::State::active)
      store->with<ToolSlot> (function);
  });
}
void changed (GObject *, gpointer);
void notified (GObject *, GParamSpec *, gpointer);
void dirtied (GimpImage *, GimpDirtyMask, gpointer);
void saving (GimpImage *, gpointer);
gboolean pending_paint (GimpImage *, gpointer);
void updated (GimpDrawable *, gint, gint, gint, gint, gpointer);

struct Controller {
  explicit Controller (GimpTool *value) : tool (value) {}
  GimpTool *tool; // owner owns this slot
  Source source;
  std::vector<Connection> connections;
  WeakRef<GObject> display;
  std::deque<std::shared_ptr<Stroke>> strokes;
  std::shared_ptr<Stroke> input;
  bool closed = false, dispatching = false, committing = false;
  unsigned commit_halts = 0;
  std::uint64_t revision = 0;

  void invalidate (bool notify = false) noexcept {
    ++revision;
    source.close ();
    std::vector<Connection> retired_connections; retired_connections.swap (connections);
    std::deque<std::shared_ptr<Stroke>> retired; retired.swap (strokes);
    auto old_display = std::move (display);
    input.reset ();
    for (auto& connection : retired_connections) connection.close ();
    if (gimp_tool_control_is_active (tool->control)) gimp_tool_control_halt (tool->control);
    for (auto& stroke : retired) {
      stroke->cancelled = true;
      if (stroke->core) {
        gimp_fill_brush_cancel_pending (stroke->core.get ());
        if (!dispatching) gimp_image_flush (stroke->image.get ());
      }
    }
    if (notify && !retired.empty () && !closed) {
      auto current = old_display.lock ();
      if (current && gimp_display_get_image (GIMP_DISPLAY (current.get ())))
        gimp_tool_message_literal (tool, GIMP_DISPLAY (current.get ()),
          _("Pending Fill Brush strokes were canceled because their target changed."));
    }
  }
  void close () noexcept { closed = true; invalidate (); }
  void connect (GObject *object, const char *signal, GCallback callback) {
    connections.emplace_back (Connection::connect (ObjectRef<GObject>::retain (object),
                                                    signal, callback, tool, nullptr));
  }
  void watch (GimpDisplay *view, GimpDrawable *drawable, GimpImage *image) {
    display = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (view)));
    connect (G_OBJECT (view), "notify::image", G_CALLBACK (notified));
    connect (G_OBJECT (gimp_display_get_shell (view)), "destroy", G_CALLBACK (changed));
    connect (G_OBJECT (drawable), "removed", G_CALLBACK (changed));
    connect (G_OBJECT (drawable), "lock-content-changed", G_CALLBACK (changed));
    connect (G_OBJECT (drawable), "format-changed", G_CALLBACK (changed));
    connect (G_OBJECT (drawable), "notify::width", G_CALLBACK (notified));
    connect (G_OBJECT (drawable), "notify::height", G_CALLBACK (notified));
    connect (G_OBJECT (drawable), "notify::offset-x", G_CALLBACK (notified));
    connect (G_OBJECT (drawable), "notify::offset-y", G_CALLBACK (notified));
    connect (G_OBJECT (drawable), "update", G_CALLBACK (updated));
    connect (G_OBJECT (image), "selected-layers-changed", G_CALLBACK (changed));
    connect (G_OBJECT (image), "selected-channels-changed", G_CALLBACK (changed));
    connect (G_OBJECT (image), "mask-changed", G_CALLBACK (changed));
    connect (G_OBJECT (image), "dirty", G_CALLBACK (dirtied));
    connect (G_OBJECT (image), "clean", G_CALLBACK (dirtied));
    connect (G_OBJECT (image), "precision-changed", G_CALLBACK (changed));
    connect (G_OBJECT (image), "query-pending-paint", G_CALLBACK (pending_paint));
    connect (G_OBJECT (image), "saving", G_CALLBACK (saving));
  }
  bool work_available () const {
    if (closed || strokes.empty ()) return false;
    const auto& stroke = strokes.front ();
    return !stroke->started || stroke->draining || !stroke->inputs.empty () || stroke->released;
  }
  void report (const char *message) {
    auto current = display.lock ();
    invalidate ();
    if (!closed && current && gimp_display_get_image (GIMP_DISPLAY (current.get ())))
      gimp_tool_message_literal (tool, GIMP_DISPLAY (current.get ()), message);
  }
  bool tick () {
    if (!work_available () || dispatching) return false;
    struct Flag { bool& value; explicit Flag (bool& v) : value (v) { value = true; } ~Flag () { value = false; } } flag (dispatching);
    auto stroke = strokes.front (); // survive any native signal invalidation
    const auto epoch = revision;
    auto current = display.lock ();
    if (!current || gimp_display_get_image (GIMP_DISPLAY (current.get ())) != stroke->image.get () ||
        !gimp_item_is_attached (GIMP_ITEM (stroke->drawable.get ())) ||
        gimp_item_get_image (GIMP_ITEM (stroke->drawable.get ())) != stroke->image.get ()) {
      invalidate (true); return false;
    }
    GError *error = nullptr;
    bool ok = true;
    /* There is deliberately no drain loop. Each owner-context dispatch performs
     * only one phase. Interpolation advances at most one dab and search
     * candidates have a budget. Native start, mask generation, publication
     * and cleanup have separately measured costs.
     * This is not a claim of a total UI latency or memory bound. */
    if (!stroke->started) {
      stroke->core = ObjectRef<GimpFillBrush>::adopt (GIMP_FILL_BRUSH (
        g_object_new (GIMP_TYPE_FILL_BRUSH, "undo-desc", _("Fill Brush"), nullptr)));
      ok = gimp_fill_brush_begin (stroke->core.get (), stroke->drawable.get (),
                                  stroke->options.get (), &stroke->inputs.front ().coords, &error);
      stroke->started = ok;
    } else if (stroke->draining) {
      stroke->draining = !gimp_fill_brush_step (stroke->core.get (), 4096, &error);
      ok = !error;
      if (ok && !stroke->cancelled && !closed && epoch == revision)
        gimp_image_flush (stroke->image.get ());
    } else if (!stroke->inputs.empty ()) {
      Input event = stroke->inputs.front ();
      stroke->inputs.pop_front ();
      if (!stroke->first_input && !stroke->line)
        gimp_paint_core_smooth_coords (GIMP_PAINT_CORE (stroke->core.get ()),
                                      stroke->options.get (), &event.coords);
      stroke->first_input = false;
      ok = gimp_fill_brush_motion_begin (stroke->core.get (), &event.coords, event.time, &error);
      stroke->draining = ok;
    } else if (stroke->released) {
      committing = true;
      gimp_tool_control_push_preserve (tool->control, TRUE);
      ok = gimp_fill_brush_finish (stroke->core.get (), TRUE, &error);
      gimp_tool_control_pop_preserve (tool->control);
      committing = false;
      if (!ok && !error) return work_available ();
      if (ok && !closed && epoch == revision && !stroke->cancelled) {
        strokes.pop_front ();
        gimp_image_flush (stroke->image.get ());
        if (strokes.empty ()) {
          auto retired = std::move (connections);
          display.reset ();
        }
      }
    }
    if (stroke->cancelled || closed || epoch != revision) {
      if (stroke->core) {
        gimp_fill_brush_cancel_pending (stroke->core.get ());
        gimp_image_flush (stroke->image.get ());
      }
      g_clear_error (&error);
      return work_available ();
    }
    if (!ok || error) {
      report (error ? error->message : _("The Fill Brush stroke could not be completed."));
      g_clear_error (&error);
      return false;
    }
    return work_available ();
  }
  void schedule () {
    if (closed || source.active () || dispatching || !work_available ()) return;
    auto weak = std::make_shared<WeakRef<GObject>> (ObjectRef<GObject>::retain (G_OBJECT (tool)));
    const auto generation = BindingStore::require (G_OBJECT (tool)).generation ();
    source = Source::idle (nullptr, G_PRIORITY_DEFAULT_IDLE, [weak, generation] {
      auto owner = weak->lock (); if (!owner) return false;
      auto *store = BindingStore::find (owner.get ());
      if (!store || !store->accepts (generation)) return false;
      return store->with<ToolSlot> ([] (Controller& self) {
        bool more = false;
        try { more = self.tick (); }
        catch (const std::exception& error) { self.report (error.what ()); more = self.work_available (); }
        catch (...) { self.report (_("The Fill Brush stroke failed.")); more = self.work_available (); }
        /* An invalidation may destroy the executing source, and a native
         * signal may then enqueue new input. Re-arm that newer generation. */
        if (!self.source.active ()) { self.schedule (); return false; }
        return more;
      });
    });
  }
};
void changed (GObject *, gpointer data)
{ with (GIMP_TOOL (data), [] (Controller& self) { self.invalidate (true); }); }
void notified (GObject *object, GParamSpec *, gpointer data) { changed (object, data); }
void dirtied (GimpImage *, GimpDirtyMask, gpointer data)
{ with (GIMP_TOOL (data), [] (Controller& self) { if (!self.committing) self.invalidate (true); }); }

gboolean pending_paint (GimpImage *, gpointer data)
{
  return gimp_fill_brush_tool_is_pending (GIMP_FILL_BRUSH_TOOL (data));
}
void updated (GimpDrawable *, gint, gint, gint, gint, gpointer data)
{ with (GIMP_TOOL (data), [] (Controller& self) { if (!self.dispatching) self.invalidate (true); }); }

void constructed (GObject *object) {
  G_OBJECT_CLASS (gimp_fill_brush_tool_parent_class)->constructed (object);
  boundary_void (nullptr, [&] { BindingStore::require (object).activate (); });
}
void dispose (GObject *object) {
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_fill_brush_tool_parent_class)->dispose (object);
}
void saving (GimpImage *image, gpointer data) {
  auto *tool = GIMP_TOOL (data);
  with (tool, [&] (Controller& self) {
    if (self.closed || !self.input) return;
    auto display = self.display.lock ();
    // BrushTool preserves native state by default, so the generic tool manager
    // intentionally skips its saving notification. Seal this owner's accepted
    // input ourselves without changing that global preservation policy.
    if (display && gimp_display_get_image (GIMP_DISPLAY (display.get ())) == image)
      gimp_tool_control (tool, GIMP_TOOL_ACTION_COMMIT, GIMP_DISPLAY (display.get ()));
  });
}
void control (GimpTool *tool, GimpToolAction action, GimpDisplay *display) {
  if (action == GIMP_TOOL_ACTION_COMMIT) with (tool, [] (Controller& self) {
    if (self.input) { self.input->released = true; self.input.reset (); }
    /* gimp_tool_control() always follows this vfunc with one HALT. Preserve
     * the sealed asynchronous transaction across those automatic HALTs,
     * including nested COMMIT callbacks, without consuming a later HALT. */
    ++self.commit_halts;
    self.schedule ();
  });
  if (action == GIMP_TOOL_ACTION_HALT) with (tool, [] (Controller& self) {
    if (self.commit_halts) --self.commit_halts;
    else self.invalidate (true);
  });
  GIMP_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->control (tool, action, display);
}
ObjectRef<GimpPaintOptions> freeze_options (GimpTool *tool) {
  auto *options = GIMP_PAINT_TOOL_GET_OPTIONS (tool);
  auto frozen = ObjectRef<GimpPaintOptions>::adopt (GIMP_PAINT_OPTIONS (gimp_config_duplicate (GIMP_CONFIG (options))));
  auto *context = GIMP_CONTEXT (frozen.get ());
  /* The context values and mutable brush/dynamics resources belong to the
   * input envelope, so a later settings edit cannot rewrite queued input. */
  gimp_context_copy_properties (GIMP_CONTEXT (options), context, GIMP_CONTEXT_PROP_MASK_ALL);
  if (auto *brush = gimp_context_get_brush (context)) {
    GimpData *copy = gimp_data_duplicate (GIMP_DATA (brush));
    gimp_context_set_brush (context, GIMP_BRUSH (copy)); g_object_unref (copy);
  }
  if (auto *dynamics = gimp_context_get_dynamics (context)) {
    GimpData *copy = gimp_data_duplicate (GIMP_DATA (dynamics));
    gimp_context_set_dynamics (context, GIMP_DYNAMICS (copy)); g_object_unref (copy);
  }
  return frozen;
}
void press (GimpTool *tool, const GimpCoords *coords, guint32 time, GdkModifierType state,
            GimpButtonPressType type, GimpDisplay *display) {
  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool))) {
    GIMP_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->button_press (tool, coords, time, state, type, display); return;
  }
  with (tool, [&] (Controller& self) {
    try {
      if (self.closed) return;
      if (!valid_input (coords))
        throw std::invalid_argument (_("Invalid Fill Brush coordinates."));
      GimpImage *image = gimp_display_get_image (display);
      if (!image) return;
      GList *targets = gimp_image_get_selected_drawables (image);
      GimpDrawable *drawable = targets && !targets->next ? GIMP_DRAWABLE (targets->data) : nullptr;
      g_list_free (targets);
      if (!drawable) throw std::runtime_error (_("Fill Brush requires one selected drawable."));
      if (gimp_viewable_get_children (GIMP_VIEWABLE (drawable))) throw std::runtime_error (_("Cannot paint on layer groups."));
      if (gimp_item_is_content_locked (GIMP_ITEM (drawable), nullptr)) throw std::runtime_error (_("The selected item's pixels are locked."));
      if (!gimp_item_is_visible (GIMP_ITEM (drawable)) && !GIMP_GUI_CONFIG (display->gimp->config)->edit_non_visible)
        throw std::runtime_error (_("The selected layer is not visible."));
      if (!self.strokes.empty () && (self.strokes.front ()->image.get () != image ||
                                     self.strokes.front ()->drawable.get () != drawable ||
                                     self.display.lock ().get () != G_OBJECT (display))) self.invalidate (true);
      /* A second press without a release ends the preceding input envelope;
       * queued work is retained in order rather than discarded. */
      if (self.input) self.input->released = true;
      auto stroke = std::make_shared<Stroke> ();
      stroke->drawable = ObjectRef<GimpDrawable>::retain (drawable);
      stroke->image = ObjectRef<GimpImage>::retain (image);
      const auto epoch = self.revision;
      stroke->options = freeze_options (tool);
      if (self.closed || epoch != self.revision) return;
      auto *preview = GIMP_PAINT_TOOL (tool);
      stroke->line = preview->draw_line && tool->display == display;
      if (stroke->line) {
        stroke->inputs.push_back ({preview->core->last_coords, time});
        stroke->inputs.push_back ({preview->core->cur_coords, time});
      } else stroke->inputs.push_back ({*coords, time});
      if (self.strokes.empty ()) self.watch (display, drawable, image);
      self.strokes.push_back (stroke); self.input = stroke;
      tool->display = display;
      g_list_free (tool->drawables); tool->drawables = g_list_append (nullptr, drawable);
      preview->core->last_coords = *coords;
      if (!gimp_tool_control_is_active (tool->control)) gimp_tool_control_activate (tool->control);
      self.schedule ();
    } catch (const std::exception& error) { self.report (error.what ()); }
  });
}
void motion (GimpTool *tool, const GimpCoords *coords, guint32 time, GdkModifierType state, GimpDisplay *display) {
  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool))) {
    GIMP_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->motion (tool, coords, time, state, display); return;
  }
  with (tool, [&] (Controller& self) {
    if (!self.input || self.closed) return;
    if (!valid_input (coords)) { self.report (_("Invalid Fill Brush coordinates.")); return; }
    if (self.display.lock ().get () != G_OBJECT (display)) { self.invalidate (true); return; }
    try {
      if (!self.input->line) self.input->inputs.push_back ({*coords, time});
      self.schedule ();
    }
    catch (const std::exception& error) { self.report (error.what ()); }
    auto *draw = GIMP_DRAW_TOOL (tool);
    auto *preview = GIMP_PAINT_TOOL (tool);
    gimp_draw_tool_pause (draw);
    preview->core->last_coords = *coords;
    preview->cursor_x = coords->x;
    preview->cursor_y = coords->y;
    if (gimp_display_get_image (display))
      gimp_brush_core_eval_transform_dynamics (GIMP_BRUSH_CORE (preview->core),
        gimp_display_get_image (display), GIMP_PAINT_TOOL_GET_OPTIONS (tool), coords);
    gimp_draw_tool_resume (draw);
  });
}
void release (GimpTool *tool, const GimpCoords *coords, guint32 time, GdkModifierType state,
              GimpButtonReleaseType type, GimpDisplay *display) {
  if (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool))) {
    GIMP_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->button_release (tool, coords, time, state, type, display); return;
  }
  with (tool, [&] (Controller& self) {
    if (gimp_tool_control_is_active (tool->control)) gimp_tool_control_halt (tool->control);
    if (type == GIMP_BUTTON_RELEASE_CANCEL) self.invalidate ();
    else if (self.input) {
      self.input->released = true; self.input.reset (); self.schedule ();
    }
  });
}
void oper_update (GimpTool *tool, const GimpCoords *coords, GdkModifierType state, gboolean proximity, GimpDisplay *display) {
  with (tool, [&] (Controller& self) {
    if (!self.strokes.empty () && self.display.lock ().get () != G_OBJECT (display)) self.invalidate (true);
  });
  GIMP_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->oper_update (tool, coords, state, proximity, display);
}
void draw (GimpDrawTool *draw_tool) {
  auto *paint = GIMP_PAINT_TOOL (draw_tool);
  // Canvas cursor/path properties have a smaller numerical domain than valid
  // paint input. Omit an unrepresentable off-canvas preview, without changing
  // or dropping the accepted stroke coordinates.
  auto representable = [] (double x, double y) {
    return std::isfinite (x) && std::isfinite (y) &&
           std::abs (x) <= GIMP_MAX_IMAGE_SIZE && std::abs (y) <= GIMP_MAX_IMAGE_SIZE;
  };
  if (!representable (paint->cursor_x, paint->cursor_y) ||
      (paint->draw_line && !representable (paint->core->last_coords.x, paint->core->last_coords.y))) return;
  GIMP_DRAW_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->draw (draw_tool);
}
GimpCanvasItem *outline (GimpPaintTool *paint, GimpDisplay *display, double x, double y) {
  auto *brush = GIMP_BRUSH_CORE (paint->core);
  if (brush->main_brush && brush->scale > 0.0) {
    int width, height;
    gimp_brush_transform_size (brush->main_brush, brush->scale, brush->aspect_ratio,
                               brush->angle, brush->reflect, &width, &height);
    if (std::abs (x - width / 2.0) > GIMP_MAX_IMAGE_SIZE ||
        std::abs (y - height / 2.0) > GIMP_MAX_IMAGE_SIZE) return nullptr;
  }
  return GIMP_PAINT_TOOL_CLASS (gimp_fill_brush_tool_parent_class)->get_outline (paint, display, x, y);
}
GtkWidget *options_gui (GimpToolOptions *options) {
  GtkWidget *box = gimp_paint_options_gui (options);
  GtkWidget *rate = gimp_prop_spin_scale_new (G_OBJECT (options), "rate", 1, 10, 1);
  gtk_box_pack_start (GTK_BOX (box), rate, FALSE, FALSE, 0); gtk_widget_show (rate);
  GtkWidget *erase = gimp_prop_check_button_new (G_OBJECT (options), "eraser-mode", _("Eraser mode"));
  gtk_box_pack_start (GTK_BOX (box), erase, FALSE, FALSE, 0); gtk_widget_show (erase);
  return box;
}
}
static void gimp_fill_brush_tool_class_init (GimpFillBrushToolClass *klass) {
  auto *object = G_OBJECT_CLASS (klass); object->constructed = constructed; object->dispose = dispose;
  auto *tool = GIMP_TOOL_CLASS (klass); tool->control = control; tool->button_press = press;
  tool->button_release = release; tool->motion = motion; tool->oper_update = oper_update;
  GIMP_DRAW_TOOL_CLASS (klass)->draw = draw;
  GIMP_PAINT_TOOL_CLASS (klass)->get_outline = outline;
}
static void gimp_fill_brush_tool_init (GimpFillBrushTool *tool) {
  tool->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (tool)).emplace<ToolSlot> (GIMP_TOOL (tool)); return true;
  });
  gimp_tool_control_set_tool_cursor (GIMP_TOOL (tool)->control, GIMP_TOOL_CURSOR_PAINTBRUSH);
  gimp_paint_tool_enable_color_picker (GIMP_PAINT_TOOL (tool), GIMP_COLOR_PICK_TARGET_FOREGROUND);
  GIMP_PAINT_TOOL (tool)->status = _("Click to fill with a brush");
  GIMP_PAINT_TOOL (tool)->status_line = _("Click to fill along the line");
}
void gimp_fill_brush_tool_register (GimpToolRegisterCallback callback, gpointer data) {
  callback (GIMP_TYPE_FILL_BRUSH_TOOL, GIMP_TYPE_FILL_BRUSH_OPTIONS, options_gui,
            GimpContextPropMask (GIMP_PAINT_OPTIONS_CONTEXT_MASK), "gimp-bucket-fill-brush-tool", _("Fill Brush"),
            _("Fill Brush: Fill connected colors within a brush stroke"), N_("_Fill Brush"), "bracketright",
            nullptr, GIMP_HELP_TOOL_BUCKET_FILL, GIMP_ICON_TOOL_BUCKET_FILL, data);
}
gboolean gimp_fill_brush_tool_is_pending (GimpFillBrushTool *tool) {
  return boundary<gboolean> (nullptr, FALSE, [&] {
    auto *store = BindingStore::find (G_OBJECT (tool));
    return store ? store->read<ToolSlot> ([] (const Controller& self) { return !self.strokes.empty (); }) : FALSE;
  });
}
