/* Convolution Matrix plug-in for GIMP
 * Copyright (C) 1997 Lauri Alanko <la@iki.fi>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This private, noninteractive GIMP 3 procedure implements the old Painter
 * Filter contract without exposing a second public convolution operation.
 */

#include "config.h"

#include <string.h>

#include "libgimp/gimp.h"

#include "convolution-matrix-kernel.h"

#include "libgimp/stdplugins-intl.h"

#define PAINTER_PROC "plug-in-painter-convmatrix"

typedef struct _Convolution      Convolution;
typedef struct _ConvolutionClass ConvolutionClass;

struct _Convolution
{
  GimpPlugIn parent_instance;
};

struct _ConvolutionClass
{
  GimpPlugInClass parent_class;
};

#define CONVOLUTION_TYPE (convolution_get_type ())

GType                   convolution_get_type         (void) G_GNUC_CONST;

static GList          * convolution_query_procedures (GimpPlugIn           *plug_in);
static GimpProcedure  * convolution_create_procedure (GimpPlugIn           *plug_in,
                                                     const gchar          *name);
static GimpValueArray * convolution_run              (GimpProcedure        *procedure,
                                                     GimpRunMode           run_mode,
                                                     GimpImage            *image,
                                                     GimpDrawable        **drawables,
                                                     GimpProcedureConfig  *config,
                                                     gpointer              run_data);

G_DEFINE_TYPE (Convolution, convolution, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (CONVOLUTION_TYPE)
DEFINE_STD_SET_I18N

static void
convolution_class_init (ConvolutionClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = convolution_query_procedures;
  plug_in_class->create_procedure = convolution_create_procedure;
  plug_in_class->set_i18n         = STD_SET_I18N;
}

static void
convolution_init (Convolution *convolution)
{
}

static GList *
convolution_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup (PAINTER_PROC));
}

static GimpProcedure *
convolution_create_procedure (GimpPlugIn  *plug_in,
                              const gchar *name)
{
  GimpProcedure *procedure;

  if (strcmp (name, PAINTER_PROC))
    return NULL;

  procedure = gimp_image_procedure_new (plug_in, name,
                                        GIMP_PDB_PROC_TYPE_PLUGIN,
                                        convolution_run, NULL, NULL);

  gimp_procedure_set_image_types (procedure, "RGB*, GRAY*");
  gimp_procedure_set_sensitivity_mask (procedure,
                                       GIMP_PROCEDURE_SENSITIVE_DRAWABLE);
  gimp_procedure_set_documentation (procedure,
                                    "Apply the Painter 5x5 convolution contract",
                                    "Private noninteractive Painter Filter procedure. "
                                    "Uses legacy float arithmetic for native nonlinear "
                                    "8-bit drawables and native double arithmetic for "
                                    "64-bit drawables. Alpha weighting is disabled, "
                                    "as in the original noninteractive procedure.",
                                    name);
  gimp_procedure_set_attribution (procedure, "Lauri Alanko", "Lauri Alanko", "1997");

  gimp_procedure_add_double_array_argument (procedure, "matrix", "Matrix",
                                            "The 25 coefficients in x-major order",
                                            G_PARAM_READWRITE);
  gimp_procedure_add_int_argument (procedure, "alpha-alg", "Alpha algorithm",
                                   "Preserved legacy argument; weighting is disabled",
                                   G_MININT, G_MAXINT, 1, G_PARAM_READWRITE);
  gimp_procedure_add_double_argument (procedure, "divisor", "Divisor", NULL,
                                      -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
                                      G_PARAM_READWRITE);
  gimp_procedure_add_double_argument (procedure, "offset", "Offset", NULL,
                                      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                                      G_PARAM_READWRITE);
  gimp_procedure_add_int32_array_argument (procedure, "channels", "Channels",
                                           "Five nonzero-enabled gray, red, green, blue, alpha flags",
                                           G_PARAM_READWRITE);
  gimp_procedure_add_int_argument (procedure, "border-mode", "Border mode",
                                   "Extend (0), wrap (1), clear (2); no-alpha uses extend",
                                   0, 2, 2, G_PARAM_READWRITE);

  return procedure;
}

static gboolean
convolution_format_supported (const Babl *format,
                              gboolean   *bytes)
{
  static const gchar * const byte_formats[] =
  {
    "R'G'B' u8", "R'G'B'A u8", "Y' u8", "Y'A u8"
  };
  static const gchar * const double_formats[] =
  {
    "RGB double",    "RGBA double",    "Y double",  "YA double",
    "R'G'B' double", "R'G'B'A double", "Y' double", "Y'A double",
    "R~G~B~ double", "R~G~B~A double", "Y~ double", "Y~A double"
  };
  const gchar *encoding;
  gsize        i;

  if (! format)
    return FALSE;

  encoding = babl_format_get_encoding (format);

  for (i = 0; i < G_N_ELEMENTS (byte_formats); i++)
    if (! strcmp (encoding, byte_formats[i]))
      {
        *bytes = TRUE;
        return TRUE;
      }

  for (i = 0; i < G_N_ELEMENTS (double_formats); i++)
    if (! strcmp (encoding, double_formats[i]))
      {
        *bytes = FALSE;
        return TRUE;
      }

  return FALSE;
}

static gboolean
convolution_read_row (GeglBuffer      *source,
                      const Babl      *format,
                      GeglAbyssPolicy  abyss,
                      gint             x,
                      gint             y,
                      gint             width,
                      gint             components,
                      gboolean         bytes,
                      gpointer         row)
{
  gsize i;

  gegl_buffer_get (source, GEGL_RECTANGLE (x, y, width, 1), 1.0,
                  format, row, GEGL_AUTO_ROWSTRIDE, abyss);

  if (! bytes)
    for (i = 0; i < (gsize) width * components; i++)
      if (! isfinite (((const gdouble *) row)[i]))
        return FALSE;

  return TRUE;
}

static gboolean
convolution_apply (GimpDrawable             *drawable,
                   const Babl               *format,
                   const GeglRectangle      *roi,
                   const PainterConvolution *kernel,
                   const gint32              channels[5],
                   gint                      border,
                   gboolean                  bytes,
                   GError                  **error)
{
  GeglBuffer      *source = NULL;
  GeglBuffer      *shadow = NULL;
  gpointer         storage = NULL;
  gpointer         destination = NULL;
  gpointer         rows[5];
  gboolean         enabled[4] = { FALSE, FALSE, FALSE, FALSE };
  gboolean         success = FALSE;
  gboolean         alpha = gimp_drawable_has_alpha (drawable);
  gboolean         rgb = gimp_drawable_is_rgb (drawable);
  gint             components = (rgb ? 3 : 1) + alpha;
  gint             pixel_bytes = components * (bytes ? 1 : sizeof (gdouble));
  gint             row_width;
  gsize            row_bytes;
  GeglAbyssPolicy   abyss;
  gint             x, y, c, i;

  if (roi->width > G_MAXINT / pixel_bytes - 4)
    goto failed;

  row_width = roi->width + 4;
  row_bytes = (gsize) row_width * pixel_bytes;

  if (row_bytes > G_MAXSIZE / 5)
    goto failed;

  storage     = g_try_malloc_n (5, row_bytes);
  destination = g_try_malloc_n (roi->width, pixel_bytes);
  if (! storage || ! destination)
    goto failed;

  for (i = 0; i < 5; i++)
    rows[i] = (guchar *) storage + i * row_bytes;

  for (c = 0; c < components; c++)
    enabled[c] = channels[rgb ? c + 1 : 0] != 0;
  if (alpha)
    enabled[components - 1] = channels[4] != 0;

  /* The original check_config() forced EXTEND for images without alpha. */
  if (! alpha)
    border = 0;
  abyss = border == 0 ? GEGL_ABYSS_CLAMP :
          border == 1 ? GEGL_ABYSS_LOOP : GEGL_ABYSS_NONE;

  source = gimp_drawable_get_buffer (drawable);
  shadow = gimp_drawable_get_shadow_buffer (drawable);
  if (! source || ! shadow)
    goto failed;

  gegl_buffer_clear (shadow, NULL);

  /* Sampling is relative to the entire drawable, never the selection.
   * GEGL's CLEAR sampling is defined at the right edge. The old plug-in's
   * positive-x partial-right CLEAR row read could exceed drawable bounds.
   * Safe zero padding deliberately replaces that undefined legacy read.
   */
  for (i = 0; i < 5; i++)
    if (! convolution_read_row (source, format, abyss, roi->x - 2,
                                roi->y - 2 + i, row_width, components,
                                bytes, rows[i]))
      goto failed;

  for (y = 0; y < roi->height; y++)
    {
      for (x = 0; x < roi->width; x++)
        for (c = 0; c < components; c++)
          {
            gsize offset = (gsize) x * components + c;

            if (bytes)
              {
                guchar *out = (guchar *) destination + offset;

                if (enabled[c])
                  {
                    const guchar *input[5] = { rows[0], rows[1], rows[2], rows[3], rows[4] };

                    if (! painter_convolution_pixel_u8 (kernel, input,
                                                        offset, components, out))
                      goto failed;
                  }
                else
                  *out = ((const guchar *) rows[2])[offset + 2 * components];
              }
            else
              {
                gdouble *out = (gdouble *) destination + offset;

                if (enabled[c])
                  {
                    const gdouble *input[5] = { rows[0], rows[1], rows[2], rows[3], rows[4] };

                    if (! painter_convolution_pixel_double (kernel, input,
                                                            offset, components, out))
                      goto failed;
                  }
                else
                  *out = ((const gdouble *) rows[2])[offset + 2 * components];

                if (alpha && c == components - 1)
                  *out = CLAMP (*out, 0.0, 1.0);
              }
          }

      gegl_buffer_set (shadow, GEGL_RECTANGLE (roi->x, roi->y + y, roi->width, 1),
                       0, format, destination, GEGL_AUTO_ROWSTRIDE);

      if (y + 1 < roi->height)
        {
          gpointer first = rows[0];

          for (i = 0; i < 4; i++)
            rows[i] = rows[i + 1];
          rows[4] = first;

          if (! convolution_read_row (source, format, abyss, roi->x - 2,
                                      roi->y + y + 3, row_width, components,
                                      bytes, rows[4]))
            goto failed;
        }

      if ((y % 10) == 0)
        gimp_progress_update ((gdouble) y / roi->height);
    }

  /* Publication happens only after every row and arithmetic result passes.
   * An error discards the entire shadow, including already staged rows.
   */
  gegl_buffer_flush (shadow);
  g_clear_object (&shadow);

  if (! gimp_drawable_merge_shadow (drawable, TRUE))
    goto failed;

  success = gimp_drawable_update (drawable, roi->x, roi->y, roi->width, roi->height);
  if (success)
    gimp_progress_update (1.0);

failed:
  g_clear_object (&shadow);
  g_clear_object (&source);
  gimp_drawable_free_shadow (drawable);
  g_free (storage);
  g_free (destination);

  if (! success)
    g_set_error_literal (error, GIMP_PLUG_IN_ERROR, 0,
                         "Convolution could not allocate rows, access the drawable, "
                         "or represent a finite arithmetic result.");

  return success;
}

static GimpValueArray *
convolution_run (GimpProcedure        *procedure,
                  GimpRunMode           run_mode,
                  GimpImage            *image,
                  GimpDrawable        **drawables,
                  GimpProcedureConfig  *config,
                  gpointer              run_data)
{
  PainterConvolution kernel = { { 0 }, };
  GimpArray         *matrix = NULL;
  GimpArray         *channel_array = NULL;
  gint32             channels[5];
  gint               border;
  GimpDrawable      *drawable;
  const Babl        *format;
  GeglRectangle      roi;
  gboolean           bytes;
  gboolean           valid;
  GError            *error = NULL;

  if (run_mode != GIMP_RUN_NONINTERACTIVE ||
      gimp_core_object_array_get_length ((GObject **) drawables) != 1)
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_CALLING_ERROR, NULL);

  g_object_get (config,
                "matrix",      &matrix,
                "divisor",     &kernel.divisor,
                "offset",      &kernel.offset,
                "channels",    &channel_array,
                "border-mode", &border,
                NULL);

  /* GimpArray.length is a byte count. Do not use a rounded element count:
   * malformed, trailing-byte, missing and NULL-data arrays all fail before
   * any coefficient or channel is read. memcpy also avoids alignment
   * assumptions about externally supplied boxed arrays.
   */
  valid = matrix && matrix->data &&
          matrix->length == sizeof (kernel.matrix) &&
          channel_array && channel_array->data &&
          channel_array->length == sizeof (channels) &&
          border >= 0 && border <= 2;

  if (valid)
    {
      memcpy (kernel.matrix, matrix->data, sizeof (kernel.matrix));
      memcpy (channels, channel_array->data, sizeof (channels));
    }

  gimp_array_free (matrix);
  gimp_array_free (channel_array);

  if (! valid)
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_CALLING_ERROR, NULL);

  gegl_init (NULL, NULL);
  drawable = drawables[0];
  format = gimp_drawable_get_format (drawable);

  if ((! gimp_drawable_is_rgb (drawable) && ! gimp_drawable_is_gray (drawable)) ||
      ! convolution_format_supported (format, &bytes))
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_EXECUTION_ERROR, NULL);

  if (! painter_convolution_validate (&kernel, bytes))
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_CALLING_ERROR, NULL);

  if (gimp_drawable_get_width (drawable) < 3 ||
      gimp_drawable_get_height (drawable) < 3 ||
      ! gimp_drawable_mask_intersect (drawable, &roi.x, &roi.y, &roi.width, &roi.height) ||
      roi.width < 1 || roi.height < 1)
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_EXECUTION_ERROR, NULL);

  gimp_progress_init ("Convolution Matrix");

  if (! convolution_apply (drawable, format, &roi, &kernel, channels, border, bytes, &error))
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_EXECUTION_ERROR, error);

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}
