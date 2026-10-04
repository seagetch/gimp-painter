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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#include "libgimp/stdplugins-intl.h"


#define PLUG_IN_PROC        "plug-in-retinex"
#define PAINTER_PROC        "plug-in-painter-retinex"
#define PLUG_IN_BINARY      "contrast-retinex"
#define PLUG_IN_ROLE        "gimp-contrast-retinex"
#define MAX_RETINEX_SCALES    8
#define MIN_GAUSSIAN_SCALE   16
#define MAX_GAUSSIAN_SCALE  250


typedef enum
{
  filter_uniform,
  filter_low,
  filter_high
} FilterMode;

/*
  Definit comment sont repartis les
  differents filtres en fonction de
  l'echelle (~= ecart type de la gaussienne)
 */
#define RETINEX_UNIFORM 0
#define RETINEX_LOW     1
#define RETINEX_HIGH    2

static gfloat RetinexScales[MAX_RETINEX_SCALES];

typedef struct
{
  gint    N;
  gfloat  sigma;
  gdouble B;
  gdouble b[4];
} gauss3_coefs;


typedef struct _Retinex      Retinex;
typedef struct _RetinexClass RetinexClass;

struct _Retinex
{
  GimpPlugIn parent_instance;
};

struct _RetinexClass
{
  GimpPlugInClass parent_class;
};


#define RETINEX_TYPE  (retinex_get_type ())
#define RETINEX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RETINEX_TYPE, Retinex))

GType                   retinex_get_type         (void) G_GNUC_CONST;

static GList          * retinex_query_procedures (GimpPlugIn           *plug_in);
static GimpProcedure  * retinex_create_procedure (GimpPlugIn           *plug_in,
                                                  const gchar          *name);

static GimpValueArray * retinex_run              (GimpProcedure        *procedure,
                                                  GimpRunMode           run_mode,
                                                  GimpImage            *image,
                                                  GimpDrawable        **drawables,
                                                  GimpProcedureConfig  *config,
                                                  gpointer              run_data);

static gboolean retinex_dialog              (GimpProcedure *procedure,
                                             GObject       *config,
                                             GimpDrawable  *drawable);
static gboolean retinex                     (GObject       *config,
                                             GimpDrawable  *drawable,
                                             GimpPreview   *preview,
                                             gboolean       painter_legacy);
static void     retinex_preview             (GtkWidget     *widget,
                                             GObject       *config);

static void     retinex_scales_distribution (gfloat       *scales,
                                             gint          nscales,
                                             gint          mode,
                                             gint          s);

static void     compute_mean_var            (gfloat       *src,
                                             gfloat       *mean,
                                             gfloat       *var,
                                             gint          size,
                                             gint          bytes);

static void     compute_coefs3              (gauss3_coefs *c,
                                             gfloat        sigma);

static gboolean gausssmooth                 (gfloat       *in,
                                             gfloat       *out,
                                             gint          size,
                                             gint          rowtride,
                                             gauss3_coefs *c);


/*
 * MSRCR = MultiScale Retinex with Color Restoration
 */
static gboolean MSRCR                       (GObject      *config,
                                             guchar       *src,
                                             gint          width,
                                             gint          height,
                                             gint          bytes,
                                             gboolean      preview_mode,
                                             gboolean      painter_legacy);


G_DEFINE_TYPE (Retinex, retinex, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (RETINEX_TYPE)
DEFINE_STD_SET_I18N


static void
retinex_class_init (RetinexClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = retinex_query_procedures;
  plug_in_class->create_procedure = retinex_create_procedure;
  plug_in_class->set_i18n         = STD_SET_I18N;
}

static void
retinex_init (Retinex *retinex)
{
}

static GList *
retinex_query_procedures (GimpPlugIn *plug_in)
{
  GList *procedures = g_list_append (NULL, g_strdup (PLUG_IN_PROC));

  return g_list_append (procedures, g_strdup (PAINTER_PROC));
}

static GimpProcedure *
retinex_create_procedure (GimpPlugIn  *plug_in,
                               const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (! strcmp (name, PLUG_IN_PROC) || ! strcmp (name, PAINTER_PROC))
    {
      gboolean painter_legacy = ! strcmp (name, PAINTER_PROC);

      procedure = gimp_image_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            retinex_run,
                                            GINT_TO_POINTER (painter_legacy), NULL);

      gimp_procedure_set_image_types (procedure, "RGB*");
      gimp_procedure_set_sensitivity_mask (procedure,
                                           GIMP_PROCEDURE_SENSITIVE_DRAWABLE);

      if (! painter_legacy)
        {
          gimp_procedure_set_menu_label (procedure, _("Retine_x..."));
          gimp_procedure_add_menu_path (procedure, "<Image>/Colors/Tone Mapping");
        }

      gimp_procedure_set_documentation (procedure,
                                        _("Enhance contrast using the "
                                          "Retinex method"),
                                        _("The Retinex Image Enhancement "
                                          "Algorithm is an automatic image "
                                          "enhancement method that enhances "
                                          "a digital image in terms of dynamic "
                                          "range compression, color independence "
                                          "from the spectral distribution of the "
                                          "scene illuminant, and color/lightness "
                                          "rendition."),
                                        name);
      gimp_procedure_set_attribution (procedure,
                                      "Fabien Pelisson",
                                      "Fabien Pelisson",
                                      "2003");

      gimp_procedure_add_int_argument (procedure, "scale",
                                       _("Scal_e"),
                                       _("Biggest scale value"),
                                       MIN_GAUSSIAN_SCALE,
                                       painter_legacy ? 256 : MAX_GAUSSIAN_SCALE, 240,
                                       G_PARAM_READWRITE);

      gimp_procedure_add_int_argument (procedure, "nscales",
                                       _("Scale _division"),
                                       _("Number of scales"),
                                       0, MAX_RETINEX_SCALES, 3,
                                       G_PARAM_READWRITE);

      gimp_procedure_add_choice_argument (procedure, "scales-mode",
                                          _("Le_vel"),
                                          _("Retinex distribution through scales"),
                                          gimp_choice_new_with_values ("uniform", filter_uniform,  _("Uniform"), NULL,
                                                                       "low",     filter_low,      _("Low"),     NULL,
                                                                       "high",    filter_high,     _("High"),    NULL,
                                                                       NULL),
                                          "uniform",
                                          G_PARAM_READWRITE);

      gimp_procedure_add_double_argument (procedure, "cvar",
                                          _("Dy_namic"),
                                          _("Variance value"),
                                          0, 4, 1.2,
                                          G_PARAM_READWRITE);
    }

  return procedure;
}

static GimpValueArray *
retinex_run (GimpProcedure        *procedure,
             GimpRunMode           run_mode,
             GimpImage            *image,
             GimpDrawable        **drawables,
             GimpProcedureConfig  *config,
             gpointer              run_data)
{
  GimpDrawable *drawable;
  gint          x, y, width, height;
  gboolean      painter_legacy = GPOINTER_TO_INT (run_data);

  /* The compatibility entry has no dialog or saved interactive state. */
  if (painter_legacy)
    {
      gint    scale, nscales, scales_mode;
      gdouble cvar;

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        return gimp_procedure_new_return_values (procedure,
                                                 GIMP_PDB_CALLING_ERROR, NULL);

      g_object_get (config,
                    "scale",   &scale,
                    "nscales", &nscales,
                    "cvar",    &cvar,
                    NULL);
      scales_mode = gimp_procedure_config_get_choice_id (config, "scales-mode");

      if (scale < MIN_GAUSSIAN_SCALE || scale > 256 ||
          nscales < 0 || nscales > MAX_RETINEX_SCALES ||
          scales_mode < RETINEX_UNIFORM || scales_mode > RETINEX_HIGH ||
          ! isfinite (cvar) || cvar < 0.0 || cvar > 4.0)
        return gimp_procedure_new_return_values (procedure,
                                                 GIMP_PDB_CALLING_ERROR, NULL);
    }

  gegl_init (NULL, NULL);

  if (gimp_core_object_array_get_length ((GObject **) drawables) != 1)
    {
      GError *error = NULL;

      g_set_error (&error, GIMP_PLUG_IN_ERROR, 0,
                   _("Procedure '%s' only works with one drawable."),
                   PLUG_IN_PROC);

      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_CALLING_ERROR,
                                               error);
    }
  else
    {
      drawable = drawables[0];
    }

  if (painter_legacy)
    {
      const Babl  *format   = gimp_drawable_get_format (drawable);
      const gchar *encoding = babl_format_get_encoding (format);

      /* Preserve native old RGB/RGBA bytes, without precision or model
       * conversion being silently admitted by the compatibility route. */
      if (! gimp_drawable_is_rgb (drawable) ||
          (strcmp (encoding, "R'G'B' u8") &&
           strcmp (encoding, "R'G'B'A u8")))
        return gimp_procedure_new_return_values (procedure,
                                                 GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  if (! gimp_drawable_mask_intersect (drawable, &x, &y, &width, &height) ||
      width  < MIN_GAUSSIAN_SCALE ||
      height < MIN_GAUSSIAN_SCALE)
    return gimp_procedure_new_return_values (procedure,
                                             GIMP_PDB_EXECUTION_ERROR,
                                             NULL);

  if (run_mode == GIMP_RUN_INTERACTIVE && ! retinex_dialog (procedure, G_OBJECT (config), drawable))
    return gimp_procedure_new_return_values (procedure,
                                             GIMP_PDB_CANCEL,
                                             NULL);

  if (gimp_drawable_is_rgb (drawable))
    {
      gimp_progress_init (_("Retinex"));

      if (! retinex (G_OBJECT (config), drawable, NULL, painter_legacy) &&
          painter_legacy)
        return gimp_procedure_new_return_values (procedure,
                                                 GIMP_PDB_EXECUTION_ERROR, NULL);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        gimp_displays_flush ();
    }
  else
    {
      return gimp_procedure_new_return_values (procedure,
                                               GIMP_PDB_EXECUTION_ERROR,
                                               NULL);
    }

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}


static gboolean
retinex_dialog (GimpProcedure *procedure,
                GObject       *config,
                GimpDrawable  *drawable)
{
  GtkWidget    *dialog;
  GtkWidget    *preview;
  GtkWidget    *combo;
  GtkWidget    *scale;
  gboolean      run;

  gimp_ui_init (PLUG_IN_BINARY);

  dialog = gimp_procedure_dialog_new (procedure,
                                      GIMP_PROCEDURE_CONFIG (config),
                                      _("Retinex Image Enhancement"));

  preview = gimp_zoom_preview_new_from_drawable (drawable);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      preview, TRUE, TRUE, 0);
  gtk_widget_set_margin_bottom (preview, 12);
  gtk_widget_show (preview);

  combo = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dialog),
                                            "scales-mode", G_TYPE_NONE);
  gtk_widget_set_margin_bottom (combo, 12);

  scale = gimp_procedure_dialog_get_spin_scale (GIMP_PROCEDURE_DIALOG (dialog),
                                                "scale", 1.0);
  gtk_widget_set_margin_bottom (scale, 12);

  scale = gimp_procedure_dialog_get_spin_scale (GIMP_PROCEDURE_DIALOG (dialog),
                                                "nscales", 1.0);
  gtk_widget_set_margin_bottom (scale, 12);

  scale = gimp_procedure_dialog_get_spin_scale (GIMP_PROCEDURE_DIALOG (dialog),
                                                "cvar", 1.0);
  gtk_widget_set_margin_bottom (scale, 12);

  gimp_procedure_dialog_fill_box (GIMP_PROCEDURE_DIALOG (dialog),
                                  "retinex-vbox", "scales-mode",
                                  "scale", "nscales", "cvar", NULL);

  g_object_set_data (config, "drawable", drawable);

  g_signal_connect (preview, "invalidated",
                    G_CALLBACK (retinex_preview),
                    config);

  g_signal_connect_swapped (config, "notify",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dialog),
                              "retinex-vbox",
                              NULL);

  gtk_widget_show (dialog);

  run = gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return run;
}

/*
 * Applies the algorithm
 */
static gboolean
retinex (GObject      *config,
         GimpDrawable *drawable,
         GimpPreview  *preview,
         gboolean      painter_legacy)
{
  GeglBuffer *src_buffer = NULL;
  GeglBuffer *dest_buffer;
  const Babl *format     = NULL;
  guchar     *src        = NULL;
  guchar     *psrc       = NULL;
  gint        x, y, width, height;
  gint        size, bytes;

  /*
   * Get the size of the current image or its selection.
   */
  if (preview)
    {
      src = gimp_zoom_preview_get_source (GIMP_ZOOM_PREVIEW (preview),
                                          &width, &height, &bytes);
    }
  else
    {
      if (! gimp_drawable_mask_intersect (drawable,
                                          &x, &y, &width, &height))
        return FALSE;

      if (painter_legacy)
        format = gimp_drawable_get_format (drawable);
      else if (gimp_drawable_has_alpha (drawable))
        format = babl_format ("R'G'B'A u8");
      else
        format = babl_format ("R'G'B' u8");

      bytes = babl_format_get_bytes_per_pixel (format);

      /* All pixel indices in the literal kernel are signed ints.  Bound
       * their product before multiplication, including the float storage. */
      if (width <= 0 || height <= 0 || (bytes != 3 && bytes != 4) ||
          (guint64) width * height * bytes > G_MAXINT - (bytes - 1) ||
          (guint64) width * height * bytes > G_MAXSIZE / sizeof (gfloat))
        return FALSE;

      /* Allocate memory */
      size = width * height * bytes;
      src  = g_try_malloc (sizeof (guchar) * size);

      if (src == NULL)
        {
          g_warning ("Failed to allocate memory");
          return FALSE;
        }

      memset (src, 0, sizeof (guchar) * size);

      /* Fill allocated memory with pixel data */
      src_buffer = gimp_drawable_get_buffer (drawable);

      gegl_buffer_get (src_buffer, GEGL_RECTANGLE (x, y, width, height), 1.0,
                       format, src,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    }

  /*
    Algorithm for Multi-scale Retinex with color Restoration (MSRCR).
   */
  psrc = src;
  if (! src ||
      ! MSRCR (config, psrc, width, height, bytes, preview != NULL, painter_legacy))
    {
      g_clear_object (&src_buffer);
      g_free (src);
      return FALSE;
    }

  if (preview)
    {
      gimp_preview_draw_buffer (preview, psrc, width * bytes);
    }
  else
    {
      dest_buffer = gimp_drawable_get_shadow_buffer (drawable);

      gegl_buffer_set (dest_buffer, GEGL_RECTANGLE (x, y, width, height), 0,
                       format, psrc,
                       GEGL_AUTO_ROWSTRIDE);

      if (src_buffer)
        g_object_unref (src_buffer);
      g_object_unref (dest_buffer);

      gimp_progress_update (1.0);

      gimp_drawable_merge_shadow (drawable, TRUE);
      gimp_drawable_update (drawable, x, y, width, height);
    }

  g_free (src);

  return TRUE;
}

static void
retinex_preview (GtkWidget *widget,
                 GObject   *config)
{
  GimpPreview  *preview  = GIMP_PREVIEW (widget);
  GimpDrawable *drawable = g_object_get_data (config, "drawable");

  retinex (config, drawable, preview, FALSE);
}

/*
 * calculate scale values for desired distribution.
 */
static void
retinex_scales_distribution (gfloat *scales,
                             gint    nscales,
                             gint    mode,
                             gint    s)
{
  if (nscales == 1)
    { /* For one filter we choose the median scale */
      scales[0] = (gint) s / 2;
    }
  else if (nscales == 2)
    { /* For two filters we choose the median and maximum scale */
      scales[0] = (gint) s / 2;
      scales[1] = (gint) s;
    }
  else
    {
      gfloat size_step = (gfloat) s / (gfloat) nscales;
      gint   i;

      switch(mode)
        {
        case RETINEX_UNIFORM:
          for(i = 0; i < nscales; ++i)
            scales[i] = 2. + (gfloat) i * size_step;
          break;

        case RETINEX_LOW:
          size_step = (gfloat) log(s - 2.0) / (gfloat) nscales;
          for (i = 0; i < nscales; ++i)
            scales[i] = 2. + pow (10, (i * size_step) / log (10));
          break;

        case RETINEX_HIGH:
          size_step = (gfloat) log(s - 2.0) / (gfloat) nscales;
          for (i = 0; i < nscales; ++i)
            scales[i] = s - pow (10, (i * size_step) / log (10));
          break;

        default:
          break;
        }
    }
}

/*
 * Calculate the coefficients for the recursive filter algorithm
 * Fast Computation of gaussian blurring.
 */
static void
compute_coefs3 (gauss3_coefs *c, gfloat sigma)
{
  /*
   * Papers:  "Recursive Implementation of the gaussian filter.",
   *          Ian T. Young , Lucas J. Van Vliet, Signal Processing 44, Elsevier 1995.
   * formula: 11b       computation of q
   *          8c        computation of b0..b1
   *          10        alpha is normalization constant B
   */
  gfloat q, q2, q3;

  if (sigma >= 2.5)
    {
      q = 0.98711 * sigma - 0.96330;
    }
  else if ((sigma >= 0.5) && (sigma < 2.5))
    {
      q = 3.97156 - 4.14554 * (gfloat) sqrt ((double) 1 - 0.26891 * sigma);
    }
  else
    {
      q = 0.1147705018520355224609375;
    }

  q2 = q * q;
  q3 = q * q2;
  c->b[0] = (1.57825+(2.44413*q)+(1.4281 *q2)+(0.422205*q3));
  c->b[1] = (        (2.44413*q)+(2.85619*q2)+(1.26661 *q3));
  c->b[2] = (                   -((1.4281*q2)+(1.26661 *q3)));
  c->b[3] = (                                 (0.422205*q3));
  c->B = 1.0-((c->b[1]+c->b[2]+c->b[3])/c->b[0]);
  c->sigma = sigma;
  c->N = 3;

/*
  g_printerr ("q %f\n", q);
  g_printerr ("q2 %f\n", q2);
  g_printerr ("q3 %f\n", q3);
  g_printerr ("c->b[0] %f\n", c->b[0]);
  g_printerr ("c->b[1] %f\n", c->b[1]);
  g_printerr ("c->b[2] %f\n", c->b[2]);
  g_printerr ("c->b[3] %f\n", c->b[3]);
  g_printerr ("c->B %f\n", c->B);
  g_printerr ("c->sigma %f\n", c->sigma);
  g_printerr ("c->N %d\n", c->N);
*/
}

static gboolean
gausssmooth (gfloat *in, gfloat *out, gint size, gint rowstride, gauss3_coefs *c)
{
  /*
   * Papers:  "Recursive Implementation of the gaussian filter.",
   *          Ian T. Young , Lucas J. Van Vliet, Signal Processing 44, Elsevier 1995.
   * formula: 9a        forward filter
   *          9b        backward filter
   *          fig7      algorithm
   */
  gint i,n, bufsize;
  gfloat *w1,*w2;

  if (size <= 0 || size > G_MAXINT - 3 ||
      (gsize) size + 3 > G_MAXSIZE / sizeof (gfloat))
    return FALSE;

  /* forward pass */
  bufsize = size+3;
  size -= 1;
  w1 = (gfloat *) g_try_malloc (bufsize * sizeof (gfloat));
  w2 = (gfloat *) g_try_malloc (bufsize * sizeof (gfloat));
  if (! w1 || ! w2)
    {
      g_free (w1);
      g_free (w2);
      g_warning ("Failed to allocate memory");
      return FALSE;
    }
  w1[0] = in[0];
  w1[1] = in[0];
  w1[2] = in[0];
  for ( i = 0 , n=3; i <= size ; i++, n++)
    {
      w1[n] = (gfloat)(c->B*in[i*rowstride] +
                       ((c->b[1]*w1[n-1] +
                         c->b[2]*w1[n-2] +
                         c->b[3]*w1[n-3] ) / c->b[0]));
    }

  /* backward pass */
  w2[size+1]= w1[size+3];
  w2[size+2]= w1[size+3];
  w2[size+3]= w1[size+3];
  for (i = size, n = i; i >= 0; i--, n--)
    {
      w2[n]= out[i * rowstride] = (gfloat)(c->B*w1[n+3] +
                                           ((c->b[1]*w2[n+1] +
                                             c->b[2]*w2[n+2] +
                                             c->b[3]*w2[n+3] ) / c->b[0]));
    }

  g_free (w1);
  g_free (w2);

  return TRUE;
}

/*
 * This function is the heart of the algo.
 * (a)  Filterings at several scales and sumarize the results.
 * (b)  Calculation of the final values.
 */
static gboolean
MSRCR (GObject *config,
       guchar  *src,
       gint     width,
       gint     height,
       gint     bytes,
       gboolean preview_mode,
       gboolean painter_legacy)
{

  gint          scale,row,col;
  gint          i,j;
  gint          size;
  gint          channel;
  guchar       *psrc = NULL;            /* backup pointer for src buffer */
  gfloat       *dst  = NULL;            /* float buffer for algorithm */
  gfloat       *pdst = NULL;            /* backup pointer for float buffer */
  gfloat       *in, *out;
  gint          channelsize;            /* Float memory cache for one channel */
  gfloat        weight;
  gauss3_coefs  coef;
  gfloat        mean, var;
  gfloat        mini, range, maxi;
  gfloat        alpha;
  gfloat        gain;
  gfloat        offset;
  gdouble       max_preview = 0.0;
  gint          scales_mode;
  gint          config_scale;
  gint          nscales;
  gdouble       cvar;

  if (width <= 0 || height <= 0 || (bytes != 3 && bytes != 4) ||
      (guint64) width * height * bytes > G_MAXINT - (bytes - 1) ||
      (guint64) width * height * bytes > G_MAXSIZE / sizeof (gfloat))
    return FALSE;

  g_object_get (config,
                "scale",   &config_scale,
                "nscales", &nscales,
                "cvar",    &cvar,
                NULL);
  scales_mode = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config),
                                                     "scales-mode");

  if (!preview_mode)
    {
      gimp_progress_init (_("Retinex: filtering"));
      max_preview = 3 * nscales;
    }

  /* Allocate all the memory needed for algorithm*/
  size = width * height * bytes;
  dst = g_try_malloc (size * sizeof (gfloat));
  if (dst == NULL)
    {
      g_warning ("Failed to allocate memory");
      return FALSE;
    }
  memset (dst, 0, size * sizeof (gfloat));

  channelsize  = (width * height);
  in  = (gfloat *) g_try_malloc (channelsize * sizeof (gfloat));
  if (in == NULL)
    {
      g_free (dst);
      g_warning ("Failed to allocate memory");
      return FALSE;
    }

  out  = (gfloat *) g_try_malloc (channelsize * sizeof (gfloat));
  if (out == NULL)
    {
      g_free (in);
      g_free (dst);
      g_warning ("Failed to allocate memory");
      return FALSE;
    }


  /*
     Calculate the scales of filtering according to the
     number of filter and their distribution.
   */

  retinex_scales_distribution (RetinexScales,
                               nscales, scales_mode, config_scale);

  /*
      Filtering according to the various scales.
      Summerize the results of the various filters according to a
      specific weight(here equivalent for all).
  */
  weight = 1./ (gfloat) nscales;

  /*
    The recursive filtering algorithm needs different coefficients according
    to the selected scale (~ = standard deviation of Gaussian).
   */
  for (channel = 0; channel < 3; channel++)
    {
      gint pos;

      for (i = 0, pos = channel; i < channelsize ; i++, pos += bytes)
         {
            /* 0-255 => 1-256 */
            in[i] = (gfloat)(src[pos] + 1.0);
         }
      for (scale = 0; scale < nscales; scale++)
        {
          compute_coefs3 (&coef, RetinexScales[scale]);
          /*
           *  Filtering (smoothing) Gaussian recursive.
           *
           *  Filter rows first
           */
          for (row=0 ;row < height; row++)
            {
              pos =  row * width;
              if (! gausssmooth (in + pos, out + pos, width, 1, &coef))
                goto smoothing_failed;
            }

          memcpy(in,  out, channelsize * sizeof(gfloat));
          memset(out, 0  , channelsize * sizeof(gfloat));

          /*
           *  Filtering (smoothing) Gaussian recursive.
           *
           *  Second columns
           */
          for (col=0; col < width; col++)
            {
              pos = col;
              if (! gausssmooth (in + pos, out + pos, height, width, &coef))
                goto smoothing_failed;
            }

          /*
             Summarize the filtered values.
             In fact one calculates a ratio between the original values and the filtered values.
           */
          for (i = 0, pos = channel; i < channelsize; i++, pos += bytes)
            {
              dst[pos] += weight * (log (src[pos] + 1.) - log (out[i]));
            }

           if (!preview_mode)
             gimp_progress_update ((channel * nscales + scale) /
                                   max_preview);
        }
    }
  g_free(in);
  g_free(out);

  /*
      Final calculation with original value and cumulated filter values.
      The parameters gain, alpha and offset are constants.
  */
  /* Ci(x,y)=log[a Ii(x,y)]-log[ Ei=1-s Ii(x,y)] */

  alpha  = 128.;
  gain   = 1.;
  offset = 0.;

  for (i = 0; i < size; i += bytes)
    {
      gfloat logl;

      psrc = src+i;
      pdst = dst+i;

      logl = log((gfloat)psrc[0] + (gfloat)psrc[1] + (gfloat)psrc[2] + 3.);

      pdst[0] = gain * ((log(alpha * (psrc[0]+1.)) - logl) * pdst[0]) + offset;
      pdst[1] = gain * ((log(alpha * (psrc[1]+1.)) - logl) * pdst[1]) + offset;
      pdst[2] = gain * ((log(alpha * (psrc[2]+1.)) - logl) * pdst[2]) + offset;
    }

/*  if (!preview_mode)
    gimp_progress_update ((2.0 + (nscales * 3)) /
                          ((nscales * 3) + 3));*/

  /*
      Adapt the dynamics of the colors according to the statistics of the first and second order.
      The use of the variance makes it possible to control the degree of saturation of the colors.
  */
  pdst = dst;

  compute_mean_var (pdst, &mean, &var, size, bytes);
  if (painter_legacy)
    {
      /* The old PDB double was stored in RetinexParams::cvar, a gfloat.
       * Preserve that conversion and the float product before arithmetic;
       * the public procedure retains its modern double calculation. */
      gfloat legacy_cvar = (gfloat) cvar;
      gfloat scaled_var = legacy_cvar * var;

      mini = mean - scaled_var;
      maxi = mean + scaled_var;
    }
  else
    {
      mini = mean - cvar * var;
      maxi = mean + cvar * var;
    }
  range = maxi - mini;

  if (!range)
    range = 1.0;

  for (i = 0; i < size; i+= bytes)
    {
      psrc = src + i;
      pdst = dst + i;

      for (j = 0 ; j < 3 ; j++)
        {
          gfloat c = 255 * ( pdst[j] - mini ) / range;

          psrc[j] = (guchar) CLAMP (c, 0, 255);
        }
    }

  g_free (dst);

  return TRUE;

smoothing_failed:
  g_free (out);
  g_free (in);
  g_free (dst);

  return FALSE;
}

/*
 * Calculate the average and variance in one go.
 */
static void
compute_mean_var (gfloat *src, gfloat *mean, gfloat *var, gint size, gint bytes)
{
  gfloat vsquared;
  gint i,j;
  gfloat *psrc;

  vsquared = 0;
  *mean = 0;
  for (i = 0; i < size; i+= bytes)
    {
       psrc = src+i;
       for (j = 0 ; j < 3 ; j++)
         {
            *mean += psrc[j];
            vsquared += psrc[j] * psrc[j];
         }
    }

  *mean /= (gfloat) size; /* mean */
  vsquared /= (gfloat) size; /* mean (x^2) */
  *var = ( vsquared - (*mean * *mean) );
  *var = sqrt(*var); /* var */
}
