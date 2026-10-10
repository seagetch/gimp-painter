/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
extern "C" {
#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <cairo.h>
#include <math.h>
   
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "core-types.h"

#include "base/temp-buf.h"

#include "gimp.h"
#include "gimpdatafactory.h"
#include "gimpcontainer.h"
#include "gimpbrush.h"
#include "gimppattern.h"

#include "gimpbezierdesc.h"
#include "gimpmypaintbrush.h"
#include "gimpmypaintbrush-private.hpp"
#include "gimpmypaintbrush-load.h"
#include "gimpmypaintbrush-save.h"
#include "gimpmarshal.h"
#include "gimptagged.h"

#include "gimp-intl.h"


enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
};


static void          gimp_mypaint_brush_tagged_iface_init     (GimpTaggedInterface  *iface);

static void          gimp_mypaint_brush_finalize              (GObject              *object);
static void          gimp_mypaint_brush_set_property          (GObject              *object,
                                                       guint                 property_id,
                                                       const GValue         *value,
                                                       GParamSpec           *pspec);
static void          gimp_mypaint_brush_get_property          (GObject              *object,
                                                       guint                 property_id,
                                                       GValue               *value,
                                                       GParamSpec           *pspec);

static gint64        gimp_mypaint_brush_get_memsize           (GimpObject           *object,
                                                       gint64               *gui_size);

static gboolean      gimp_mypaint_brush_get_size              (GimpViewable         *viewable,
                                                       gint                 *width,
                                                       gint                 *height);
static TempBuf     * gimp_mypaint_brush_get_new_preview       (GimpViewable         *viewable,
                                                       GimpContext          *context,
                                                       gint                  width,
                                                       gint                  height);
static gchar       * gimp_mypaint_brush_get_description       (GimpViewable         *viewable,
                                                       gchar               **tooltip);

static void          gimp_mypaint_brush_dirty                 (GimpData             *data);
static const gchar * gimp_mypaint_brush_get_extension         (GimpData             *data);

static void          gimp_mypaint_brush_real_begin_use        (GimpMypaintBrush            *mypaint_brush);
static void          gimp_mypaint_brush_real_end_use          (GimpMypaintBrush            *mypaint_brush);
static GimpMypaintBrush   * gimp_mypaint_brush_real_select_mypaint_brush     (GimpMypaintBrush            *mypaint_brush,
                                                       const GimpCoords     *last_coords,
                                                       const GimpCoords     *current_coords);
static gboolean      gimp_mypaint_brush_real_want_null_motion (GimpMypaintBrush            *mypaint_brush,
                                                       const GimpCoords     *last_coords,
                                                       const GimpCoords     *current_coords);

static gchar       * gimp_mypaint_brush_get_checksum          (GimpTagged           *tagged);


G_DEFINE_TYPE_WITH_CODE (GimpMypaintBrush, gimp_mypaint_brush, GIMP_TYPE_DATA,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_TAGGED,
                                                gimp_mypaint_brush_tagged_iface_init))

#define parent_class gimp_mypaint_brush_parent_class

//static guint mypaint_brush_signals[LAST_SIGNAL] = { 0 };


static void
gimp_mypaint_brush_class_init (GimpMypaintBrushClass *klass)
{
  GObjectClass        *object_class      = G_OBJECT_CLASS (klass);
  GimpObjectClass   *gimp_object_class = GIMP_OBJECT_CLASS (klass);
  GimpViewableClass *viewable_class    = GIMP_VIEWABLE_CLASS (klass);
  GimpDataClass      *data_class        = GIMP_DATA_CLASS (klass);

  object_class->finalize           = gimp_mypaint_brush_finalize;
  object_class->get_property       = gimp_mypaint_brush_get_property;
  object_class->set_property       = gimp_mypaint_brush_set_property;

  gimp_object_class->get_memsize   = gimp_mypaint_brush_get_memsize;

  viewable_class->default_stock_id = "gimp-tool-mypaint-brush";
  viewable_class->get_size         = gimp_mypaint_brush_get_size;
  viewable_class->get_new_preview  = gimp_mypaint_brush_get_new_preview;
  viewable_class->get_description  = gimp_mypaint_brush_get_description;

  data_class->dirty                = gimp_mypaint_brush_dirty;
  data_class->get_extension        = gimp_mypaint_brush_get_extension;
  data_class->save                 = gimp_mypaint_brush_save;

  klass->begin_use                 = gimp_mypaint_brush_real_begin_use;
  klass->end_use                   = gimp_mypaint_brush_real_end_use;
  klass->select_mypaint_brush      = gimp_mypaint_brush_real_select_mypaint_brush;
  klass->want_null_motion          = gimp_mypaint_brush_real_want_null_motion;
}

static void
gimp_mypaint_brush_tagged_iface_init (GimpTaggedInterface *iface)
{
  iface->get_checksum = gimp_mypaint_brush_get_checksum;
}

static void
gimp_mypaint_brush_init (GimpMypaintBrush *mypaint_brush)
{
  mypaint_brush->p = (gpointer)(new GimpMypaintBrushPrivate());
}

static void
gimp_mypaint_brush_finalize (GObject *object)
{
  GimpMypaintBrush *mypaint_brush = GIMP_MYPAINT_BRUSH (object);

  if (mypaint_brush->p) {
    GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(mypaint_brush->p);
    delete priv;
    mypaint_brush->p = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_mypaint_brush_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GimpMypaintBrush *mypaint_brush = GIMP_MYPAINT_BRUSH (object);

  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_mypaint_brush_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GimpMypaintBrush *mypaint_brush = GIMP_MYPAINT_BRUSH (object);

  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gint64
gimp_mypaint_brush_get_memsize (GimpObject *object,
                        gint64     *gui_size)
{
  GimpMypaintBrush *mypaint_brush   = GIMP_MYPAINT_BRUSH (object);
  gint64     memsize = 0;

//  memsize += temp_buf_get_memsize (mypaint_brush->mask);
//  memsize += temp_buf_get_memsize (mypaint_brush->pixmap);

  return memsize + GIMP_OBJECT_CLASS (parent_class)->get_memsize (object,
                                                                  gui_size);
}

static gboolean
gimp_mypaint_brush_get_size (GimpViewable *viewable,
                     gint         *width,
                     gint         *height)
{
  GimpMypaintBrush *mypaint_brush = GIMP_MYPAINT_BRUSH (viewable);

//  *width  = mypaint_brush->mask->width;
//  *height = mypaint_brush->mask->height;

  return TRUE;
}

static TempBuf *
gimp_mypaint_brush_get_new_preview (GimpViewable *viewable,
                            GimpContext  *context,
                            gint          width,
                            gint          height)
{
  GimpMypaintBrush      *mypaint_brush       = GIMP_MYPAINT_BRUSH (viewable);
  GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(mypaint_brush->p);  
  TempBuf       *return_buf  = NULL;
  guchar transp[4]   = { 0, 0, 0, 0 };

  return_buf = temp_buf_new (width, height, 4, 0, 0, transp);
  guchar *dest_buf   = temp_buf_get_data (return_buf);

  gimp_mypaint_brush_begin_use (mypaint_brush);

  priv->get_new_preview(dest_buf, context, width, height, 4, 4 * width);

  gimp_mypaint_brush_end_use (mypaint_brush);

  return return_buf;
}

static gchar *
gimp_mypaint_brush_get_description (GimpViewable  *viewable,
                            gchar        **tooltip)
{
  GimpMypaintBrush *mypaint_brush = GIMP_MYPAINT_BRUSH (viewable);
  g_return_val_if_fail (mypaint_brush != NULL, NULL);

  const gchar* desc1 = gimp_object_get_name(GIMP_OBJECT(mypaint_brush));  
  return g_strdup (desc1);
}

static void
gimp_mypaint_brush_dirty (GimpData *data)
{
  GimpMypaintBrush *mypaint_brush = GIMP_MYPAINT_BRUSH (data);
#if 0
  if (mypaint_brush->mask_cache)
    gimp_mypaint_brush_cache_clear (mypaint_brush->mask_cache);

  if (mypaint_brush->pixmap_cache)
    gimp_mypaint_brush_cache_clear (mypaint_brush->pixmap_cache);

  if (mypaint_brush->boundary_cache)
    gimp_mypaint_brush_cache_clear (mypaint_brush->boundary_cache);
#endif
  GIMP_DATA_CLASS (parent_class)->dirty (data);
}

static const gchar *
gimp_mypaint_brush_get_extension (GimpData *data)
{
  return GIMP_MYPAINT_BRUSH_FILE_EXTENSION;
}

static void
gimp_mypaint_brush_real_begin_use (GimpMypaintBrush *mypaint_brush)
{
}

static void
gimp_mypaint_brush_real_end_use (GimpMypaintBrush *mypaint_brush)
{
}

static GimpMypaintBrush *
gimp_mypaint_brush_real_select_mypaint_brush (GimpMypaintBrush        *mypaint_brush,
                              const GimpCoords *last_coords,
                              const GimpCoords *current_coords)
{
  return mypaint_brush;
}

static gboolean
gimp_mypaint_brush_real_want_null_motion (GimpMypaintBrush        *mypaint_brush,
                                  const GimpCoords *last_coords,
                                  const GimpCoords *current_coords)
{
  return TRUE;
}

static gchar *
gimp_mypaint_brush_get_checksum (GimpTagged *tagged)
{
  GimpMypaintBrush *mypaint_brush           = GIMP_MYPAINT_BRUSH (tagged);
  gchar     *checksum_string = NULL;
#if 0
  if (mypaint_brush->mask)
    {
      GChecksum *checksum = g_checksum_new (G_CHECKSUM_MD5);

      g_checksum_update (checksum, temp_buf_get_data (mypaint_brush->mask), temp_buf_get_data_size (mypaint_brush->mask));
      if (mypaint_brush->pixmap)
        g_checksum_update (checksum, temp_buf_get_data (mypaint_brush->pixmap), temp_buf_get_data_size (mypaint_brush->pixmap));
      g_checksum_update (checksum, (const guchar *) &mypaint_brush->spacing, sizeof (mypaint_brush->spacing));
      g_checksum_update (checksum, (const guchar *) &mypaint_brush->x_axis, sizeof (mypaint_brush->x_axis));
      g_checksum_update (checksum, (const guchar *) &mypaint_brush->y_axis, sizeof (mypaint_brush->y_axis));

      checksum_string = g_strdup (g_checksum_get_string (checksum));

      g_checksum_free (checksum);
    }
#endif
  return checksum_string;
}

/*  public functions  */

GimpData *
gimp_mypaint_brush_new (GimpContext *context,
                        const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (name[0] != '\0', NULL);

  GimpMypaintBrush *brush;
  brush = GIMP_MYPAINT_BRUSH (g_object_new (GIMP_TYPE_MYPAINT_BRUSH,
                                            "name", name,
                                            "mime-type", "application/x-mypaint-brush",
                                            NULL));

  return GIMP_DATA (brush);
}

GimpData * 
gimp_mypaint_brush_duplicate (GimpMypaintBrush *mypaint_brush)
{
  g_return_val_if_fail (GIMP_IS_MYPAINT_BRUSH (mypaint_brush), NULL);
  gchar* name;
  g_object_get (G_OBJECT(mypaint_brush), "name", &name, NULL);

  GimpMypaintBrush* result = GIMP_MYPAINT_BRUSH(gimp_mypaint_brush_new(NULL, name));
  delete reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);
  result->p = reinterpret_cast<GimpMypaintBrushPrivate*>(mypaint_brush->p)->duplicate();
  return GIMP_DATA(result);
}


GimpData *
gimp_mypaint_brush_get_standard (GimpContext *context)
{
  static GimpData *standard_mypaint_brush = NULL;

  if (! standard_mypaint_brush)
    {
      standard_mypaint_brush = gimp_mypaint_brush_new (context, "Standard");

      gimp_data_clean (standard_mypaint_brush);
      gimp_data_make_internal (standard_mypaint_brush, "gimp-mypaint_brush-standard");

      g_object_add_weak_pointer (G_OBJECT (standard_mypaint_brush),
                                 (gpointer *) &standard_mypaint_brush);
    }

  return standard_mypaint_brush;
}

void
gimp_mypaint_brush_begin_use (GimpMypaintBrush *mypaint_brush)
{
  g_return_if_fail (GIMP_IS_MYPAINT_BRUSH (mypaint_brush));

  mypaint_brush->use_count++;

  if (mypaint_brush->use_count == 1)
    GIMP_MYPAINT_BRUSH_GET_CLASS (mypaint_brush)->begin_use (mypaint_brush);
}

void
gimp_mypaint_brush_end_use (GimpMypaintBrush *mypaint_brush)
{
  g_return_if_fail (GIMP_IS_MYPAINT_BRUSH (mypaint_brush));
  g_return_if_fail (mypaint_brush->use_count > 0);

  mypaint_brush->use_count--;

  if (mypaint_brush->use_count == 0)
    GIMP_MYPAINT_BRUSH_GET_CLASS (mypaint_brush)->end_use (mypaint_brush);
}

GimpMypaintBrush *
gimp_mypaint_brush_select_mypaint_brush (GimpMypaintBrush        *mypaint_brush,
                         const GimpCoords *last_coords,
                         const GimpCoords *current_coords)
{
  g_return_val_if_fail (GIMP_IS_MYPAINT_BRUSH (mypaint_brush), NULL);
  g_return_val_if_fail (last_coords != NULL, NULL);
  g_return_val_if_fail (current_coords != NULL, NULL);

  return GIMP_MYPAINT_BRUSH_GET_CLASS (mypaint_brush)->select_mypaint_brush (mypaint_brush,
                                                     last_coords,
                                                     current_coords);
}

gboolean
gimp_mypaint_brush_want_null_motion (GimpMypaintBrush        *mypaint_brush,
                             const GimpCoords *last_coords,
                             const GimpCoords *current_coords)
{
  g_return_val_if_fail (GIMP_IS_MYPAINT_BRUSH (mypaint_brush), FALSE);
  g_return_val_if_fail (last_coords != NULL, FALSE);
  g_return_val_if_fail (current_coords != NULL, FALSE);

  return GIMP_MYPAINT_BRUSH_GET_CLASS (mypaint_brush)->want_null_motion (mypaint_brush,
                                                         last_coords,
                                                         current_coords);
}

} /* extern C */

#include "paint/gimpmypaintcore-surface.hpp"
#include "paint/mypaintbrush-brush.hpp"

GimpMypaintBrushPrivate::GimpMypaintBrushPrivate() {
  parent_brush_name = g_strdup("");
  group = g_strdup("");
  icon_image = NULL;
  for (int i = 0; i < BRUSH_MAPPING_COUNT; i ++) {
    settings[i].base_value = 0;
    settings[i].mapping    = NULL;
  }
  for (int i = 0; i < BRUSH_BOOL_COUNT; i ++) {
    switches[i] = FALSE;
  }
  for (int i = 0; i < BRUSH_TEXT_COUNT; i ++) {
    text[i] = NULL;
  }
  dirty = false;
}

GimpMypaintBrushPrivate::~GimpMypaintBrushPrivate() {
  if (parent_brush_name) {
    g_free (parent_brush_name);
    parent_brush_name = NULL;
  }
  if (group) {
    g_free (group);
    group = NULL;
  }
  for (int i = 0; i < BRUSH_MAPPING_COUNT; i ++) {
    deallocate_mapping(i);
  }
  for (int i = 0; i < BRUSH_TEXT_COUNT; i ++) {
    if (text[i])
      g_free(text[i]);
  }
  if (icon_image) {
    cairo_surface_destroy (icon_image);
    icon_image = NULL;
  }
}

void 
GimpMypaintBrushPrivate::set_base_value (int index, float value) {
  g_assert (index >= 0 && index < BRUSH_MAPPING_COUNT);
  allocate_mapping(index);
  settings[index].mapping->base_value = value;
  /*
  settings[index].base_value = value;
  if (settings[index].mapping)
    settings[index].mapping->base_value = value;
  */
  mark_as_dirty();
}

float
GimpMypaintBrushPrivate::get_base_value (int index) {
  g_assert (index >= 0 && index < BRUSH_MAPPING_COUNT);
  if (settings[index].mapping) {
    return settings[index].mapping->base_value;
  }
  return settings[index].base_value;
}

void 
GimpMypaintBrushPrivate::allocate_mapping (int index) {
  g_assert (index >= 0 && index < BRUSH_MAPPING_COUNT);
  if (!settings[index].mapping) {
    settings[index].mapping = new Mapping(INPUT_COUNT);
    mark_as_dirty();
  }
}

void
GimpMypaintBrushPrivate::deallocate_mapping (int index) {
  g_assert (index >= 0 && index < BRUSH_MAPPING_COUNT);
  if (settings[index].mapping) {
    delete settings[index].mapping;
    mark_as_dirty();
    settings[index].mapping = NULL;
  }
}

char*
GimpMypaintBrushPrivate::get_parent_brush_name() {
  return parent_brush_name;
}

void
GimpMypaintBrushPrivate::set_parent_brush_name(const char *name) {
  g_assert (name != NULL);
  if (parent_brush_name)
    g_free (parent_brush_name);
  parent_brush_name = g_strdup(name);
  mark_as_dirty();
}

char* 
GimpMypaintBrushPrivate::get_group() {
  return group;
}

void
GimpMypaintBrushPrivate::set_group(const char *name) {
  g_assert (name != NULL);
  if (group)
    g_free (group);
  group = g_strdup(name);
  mark_as_dirty();
}

void 
GimpMypaintBrushPrivate::set_bool_value (int index, bool value) {
  index -= BRUSH_BOOL_BASE;
  g_print("index=%d\n", index);
  g_assert (index >= 0 && index < BRUSH_BOOL_COUNT);
  switches[index] = value;
  mark_as_dirty();
}

bool
GimpMypaintBrushPrivate::get_bool_value (int index) {
  index -= BRUSH_BOOL_BASE;
  g_assert (index >= 0 && index < BRUSH_BOOL_COUNT);
  return switches[index];
}

void 
GimpMypaintBrushPrivate::set_text_value (int index, const char* value) {
  index -= BRUSH_TEXT_BASE;
  g_assert (index >= 0 && index < BRUSH_TEXT_COUNT);
  if (text[index])
    g_free (text[index]);
  text[index] = value ? g_strdup(value) : NULL;
  mark_as_dirty();
}

char*
GimpMypaintBrushPrivate::get_text_value (int index) {
  index -= BRUSH_TEXT_BASE;
  g_assert (index >= 0 && index < BRUSH_TEXT_COUNT);
  return text[index];
}


void 
GimpMypaintBrushPrivate::get_new_preview(guchar* dest_buf, 
                                         GimpContext* context,
                                         int width, 
                                         int height, 
                                         int bytes, 
                                         int dest_stride) {
#if 0
  ScopedPointer<cairo_surface_t, void(cairo_surface_t*), cairo_surface_destroy> scaled_icon = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  
  gint           icon_width = cairo_image_surface_get_width (icon_image);
  gint           icon_height = cairo_image_surface_get_height (icon_image);
  gint           x, y;
  gdouble        scale = 1.0;

  ScopedPointer<cairo_t, void(cairo_t*), cairo_destroy> cr = cairo_create(scaled_icon.ptr());
  if (icon_width > width || icon_height > height)
    {
      gdouble ratio_x = (gdouble) width  / (gdouble) icon_width;
      gdouble ratio_y = (gdouble) height / (gdouble) icon_height;
      scale   = MIN (ratio_x, ratio_y);
      cairo_scale (cr.ptr(), scale, scale);
    }
  cairo_set_source_surface (cr.ptr(), icon_image, 0, 0);
  cairo_paint (cr.ptr());
  
  unsigned char *source_buf = cairo_image_surface_get_data (scaled_icon.ptr());
  gint src_stride       = cairo_image_surface_get_stride (scaled_icon.ptr());
  
  for (y = 0; y < height; y ++) 
    {
      for (x = 0; x < width; x ++)
        {
          // Following code may be dependent on the endian.
          dest_buf[x * 4    ] = source_buf[x * 4 + 2];
          dest_buf[x * 4 + 1] = source_buf[x * 4 + 1];
          dest_buf[x * 4 + 2] = source_buf[x * 4 + 0];
          dest_buf[x * 4 + 3] = source_buf[x * 4 + 3];
        }
      source_buf += src_stride;
      dest_buf   += dest_stride;
    }
#else
  GimpDataFactory*    b_factory = context->gimp->brush_factory;
  GimpDataFactory*    p_factory = context->gimp->pattern_factory;
  guchar              color[]   = { 255, 255, 255, 255 };
  TempBuf*            temp_buf  = temp_buf_new(256, 256, 4, 0, 0, color);
  GimpMypaintSurface* surface   = GimpMypaintSurface_TempBuf_new (temp_buf);
  Brush*              brush     = new Brush();
  brush->reset();
  guchar* temp = temp_buf_get_data(temp_buf);
  for (int y = 0; y < 256; y ++) {
    int x;
    for (x = 0; x < y; x ++) {
      temp[4 * (x)  ] = 255; // R
      temp[4 * (x)+1] = 192; // G
      temp[4 * (x)+2] = 192; // B
      temp[4 * (x)+3] = 255; // A
    }
    for (; x < 256; x ++) {
      temp[4 * (x)  ] = 255; // R
      temp[4 * (x)+1] = 255; // G
      temp[4 * (x)+2] = 255; // B
      temp[4 * (x)+3] = 255; // A
    }
    temp += temp_buf->bytes * x;
  }

  g_print("Brush name: %s\n", get_parent_brush_name ());
  if (1) {
    // update color values
    // FIXME: Color should be updated when Foreground color is selected
    GimpRGB rgb;
    rgb.a = 1.0;
    rgb.b = 1.0;
    rgb.g = 0.0;
    rgb.r = 0.0;
    GimpHSV hsv;
    gimp_rgb_to_hsv (&rgb, &hsv);
    brush->set_base_value(BRUSH_COLOR_H, hsv.h);
    brush->set_base_value(BRUSH_COLOR_S, hsv.s);
    brush->set_base_value(BRUSH_COLOR_V, hsv.v);

    rgb.a = rgb.b = rgb.g = rgb.r = 1.0;
    surface->set_bg_color(&rgb);

    // Attach gimp brush to the surface object.
    gboolean use_gimp_brushmark = get_bool_value(BRUSH_USE_GIMP_BRUSHMARK);
    GimpBrush* brushmark = NULL;
    if (use_gimp_brushmark) {
      gchar* brush_name = get_text_value(BRUSH_BRUSHMARK_NAME);
      if (brush_name) {
        GimpContainer* container = gimp_data_factory_get_container(b_factory);
        brushmark = GIMP_BRUSH(gimp_container_get_child_by_name(container, brush_name));
      } else {
        brushmark = gimp_context_get_brush (GIMP_CONTEXT(context));
      }
    }
    surface->set_brushmark(brushmark);

    gboolean use_gimp_texture = get_bool_value(BRUSH_USE_GIMP_TEXTURE);
    GimpPattern* pattern = NULL;
    if (use_gimp_texture) {
      gchar* pattern_name = get_text_value(BRUSH_TEXTURE_NAME);
      if (pattern_name) {
        GimpContainer* container = gimp_data_factory_get_container(p_factory);
        pattern = GIMP_PATTERN(gimp_container_get_child_by_name(container, pattern_name));
      } else {
        pattern = gimp_context_get_pattern (GIMP_CONTEXT(context));
      }
    }
    surface->set_texture(pattern);

    gboolean floating_stroke = get_bool_value(BRUSH_NON_INCREMENTAL);
    surface->set_floating_stroke(floating_stroke);
    gdouble stroke_opacity = get_base_value(BRUSH_STROKE_OPACITY);
    surface->set_stroke_opacity(stroke_opacity);

    for (int i = 0; i < BRUSH_MAPPING_COUNT; i ++) {
      Mapping* m = get_setting(i)->mapping;
      if (m)
        brush->copy_mapping(i, m);
      else {
        brush->set_mapping_n(i, 0, 0);
        brush->set_base_value(i, get_setting(i)->base_value);
      }
    }

    g_print("begin surface session...\n");

    GimpCoords coords;
    coords.x = coords.y = 128;
    coords.pressure = 0.0;
    surface->set_coords(&coords);
    brush->reset();
    surface->begin_session();
    brush->stroke_to(surface, coords.x, coords.y, coords.pressure, 0, 0, 0.001);

    for (int i = 0; i < 256; i ++) {
      gdouble theta = 2.0 * i * M_PI / 256.0;
      gdouble x = cos(theta) * exp(0.2 * theta) * 30;
      gdouble y = sin(theta) * exp(0.2 * theta) * 30;

      coords.x = 128 + x, coords.y = 128 + y;
      coords.pressure = (256.0 - i)/256.0;
      surface->set_coords(&coords);
      /// from Layer#stroke_to
      brush->stroke_to(surface, coords.x, coords.y, coords.pressure, 0, 0, (i + 2) / 1000.);
    };
        
    surface->end_session();
    g_print("end surface session...\n");
  }
  delete brush;
  delete surface;
  TempBuf* scaled_buf = temp_buf_scale(temp_buf, width, height);
  temp_buf_free(temp_buf);
  guchar* source_buf = temp_buf_get_data(scaled_buf);
  for (int y = 0; y < height; y ++) 
    {
      for (int x = 0; x < width; x ++)
        {
          // Following code may be dependent on the endian.
          dest_buf[x * 4    ] = source_buf[x * 4];
          dest_buf[x * 4 + 1] = source_buf[x * 4 + 1];
          dest_buf[x * 4 + 2] = source_buf[x * 4 + 2];
          dest_buf[x * 4 + 3] = source_buf[x * 4 + 3];
        }
      source_buf += scaled_buf->bytes * width;
      dest_buf   += dest_stride;
    }
  temp_buf_free(scaled_buf);
#endif
}

void
GimpMypaintBrushPrivate::set_icon_image(cairo_surface_t* image) {
  if (icon_image) {
    cairo_surface_destroy (icon_image);
    icon_image = NULL;
  }
  if (image) {
    cairo_format_t format = cairo_image_surface_get_format(image);
    int            width  = cairo_image_surface_get_width(image);
    int            height = cairo_image_surface_get_height(image);
    int            stride = cairo_image_surface_get_stride(image);
    guchar         *src_data = cairo_image_surface_get_data(image);
    int            buf_size = cairo_format_stride_for_width (format, width) * height;
    guchar         *data = g_new(guchar, buf_size);
    memcpy(data, src_data, buf_size);
    icon_image = cairo_image_surface_create_for_data(data, format, width, height,stride);
    mark_as_dirty();
  }
}

cairo_surface_t*
GimpMypaintBrushPrivate::get_icon_image() {
  return icon_image;
}

GimpMypaintBrushPrivate*
GimpMypaintBrushPrivate::duplicate() {
  GimpMypaintBrushPrivate* priv = new GimpMypaintBrushPrivate();
  priv->set_parent_brush_name(parent_brush_name);
  priv->set_group(group);
  for (int i = 0; i < BRUSH_MAPPING_COUNT; i ++) {
    priv->settings[i].base_value = settings[i].base_value;
    if (settings[i].mapping) {
      priv->allocate_mapping(i);
      *(priv->settings[i].mapping) = *(settings[i].mapping);
    }
  }
  for (int i = 0; i < BRUSH_BOOL_COUNT; i ++) {
    priv->switches[i] = switches[i];
  }
  for (int i = 0; i < BRUSH_TEXT_COUNT; i ++) {
    priv->text[i] = text[i] ? g_strdup(text[i]): NULL;
  }
  priv->set_icon_image(icon_image);
  return priv;
}
