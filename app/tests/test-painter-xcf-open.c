/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpcontainer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-new.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimppickable.h"
#include "core/gimpprogress.h"
#include "core/gimpselection.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginmanager-file.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-open.h"
#include "file/file-save.h"
#include "xcf/xcf.h"
#include "xcf/painter-xcf-load.h"
#include "xcf/painter-xcf-preserve.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static gchar *fixture_path (const gchar *name)
{
  return g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "migration", "fixtures", name, NULL);
}
static GimpImage *open_file (GFile *file, const gchar *procedure, GimpProgress *progress, GError **error)
{
  GimpPlugInProcedure *proc;
  GimpPDBStatusType status;
  if (procedure)
    proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, procedure));
  else
    {
      proc = gimp_plug_in_manager_file_procedure_find (gimp->plug_in_manager,
                GIMP_FILE_PROCEDURE_GROUP_OPEN, file, error);
      g_assert_nonnull (proc);
      g_assert_cmpstr (gimp_object_get_name (proc), ==, "gimp-xcf-load");
    }
  g_assert_nonnull (proc);
  return file_open_image (gimp, gimp_get_user_context (gimp), progress, file,
                           0, 0, FALSE, proc, GIMP_RUN_NONINTERACTIVE, &status, NULL, error);
}
static GimpImage *open_fixture (const gchar *name)
{
  gchar *path = fixture_path (name);
  GFile *file = g_file_new_for_path (path);
  GError *error = NULL;
  GimpImage *image = open_file (file, NULL, NULL, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  g_object_unref (file); g_free (path);
  return image;
}
static GimpLayer *find_layer (GimpImage *image, const gchar *name)
{
  GList *layers = gimp_image_get_layer_list (image);
  GimpLayer *result = NULL;
  for (GList *p = layers; p; p = p->next)
    if (!g_strcmp0 (gimp_object_get_name (p->data), name)) { result = p->data; break; }
  g_list_free (layers); return result;
}
static void pixel (GimpLayer *layer, gint x, gint y, guint8 r, guint8 g, guint8 b, guint8 a)
{
  guint8 data[4];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), GEGL_RECTANGLE (x, y, 1, 1),
                   1.0, babl_format ("R'G'B'A u8"), data, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpuint (data[0], ==, r); g_assert_cmpuint (data[1], ==, g);
  g_assert_cmpuint (data[2], ==, b); g_assert_cmpuint (data[3], ==, a);
}
static void original_exact (GimpImage *image, const gchar *name)
{
  GBytes *bytes = xcf_painter_ref_original (image);
  gchar *path = fixture_path (name), *contents;
  gsize len, actual_len;
  const void *actual;
  g_assert_true (g_file_get_contents (path, &contents, &len, NULL));
  g_assert_nonnull (bytes); actual = g_bytes_get_data (bytes, &actual_len);
  g_assert_cmpmem (contents, len, actual, actual_len);
  g_bytes_unref (bytes); g_free (path); g_free (contents);
}
static void ordinary (void)
{
  GimpImage *image = open_fixture ("legacy-runtime/ordinary-layers.xcf");
  GimpLayer *group = find_layer (image, "fixture group");
  GimpLayer *paint = find_layer (image, "painted stroke");
  gdouble xres, yres;
  gint x1, y1, x2, y2;
  guint64 offset;
  g_assert_cmpint (gimp_image_get_width (image), ==, 96);
  g_assert_cmpint (gimp_image_get_height (image), ==, 80);
  g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_layers (image)), ==, 2);
  g_assert_true (GIMP_IS_GROUP_LAYER (group)); g_assert_nonnull (paint);
  g_assert_true (gimp_item_get_parent (GIMP_ITEM (paint)) == GIMP_ITEM (group));
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (paint)), ==, 80);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (paint)), ==, 64);
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (paint)), ==, 9);
  g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (paint)), ==, 7);
  g_assert_cmpfloat (gimp_layer_get_opacity (paint), ==, 191.0 / 255.0);
  g_assert_cmpint (gimp_layer_get_mode (paint), ==, GIMP_LAYER_MODE_PAINTER_MULTIPLY);
  g_assert_nonnull (gimp_layer_get_mask (paint));
  g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_channels (image)), ==, 1);
  gimp_image_get_resolution (image, &xres, &yres);
  g_assert_cmpfloat (xres, ==, 143); g_assert_cmpfloat (yres, ==, 167);
  g_assert_true (gimp_item_bounds (GIMP_ITEM (gimp_image_get_mask (image)), &x1, &y1, &x2, &y2));
  g_assert_cmpint (x1, ==, 8); g_assert_cmpint (y1, ==, 10);
  g_assert_cmpint (x2, ==, 20); g_assert_cmpint (y2, ==, 18);
  g_assert_true (xcf_painter_original_offset (G_OBJECT (paint), &offset));
  g_assert_cmpuint (offset, ==, 661);
  original_exact (image, "legacy-runtime/ordinary-layers.xcf");
  g_object_unref (image);
}
static void ordinary_projection_exact_gate (void)
{
  GimpImage *image = open_fixture ("legacy-runtime/ordinary-layers.xcf");
  guint8 *projection = g_malloc (96 * 80 * 4);
  gchar *path = fixture_path ("legacy-runtime/ordinary-layers.png");
  GdkPixbuf *reference = gdk_pixbuf_new_from_file (path, NULL);
  guint differences = 0, maximum = 0;
  g_assert_nonnull (reference);
  g_assert_cmpint (gdk_pixbuf_get_n_channels (reference), ==, 4);
  gimp_pickable_flush (GIMP_PICKABLE (image));
  gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (image)), GEGL_RECTANGLE (0, 0, 96, 80),
                   1, babl_format ("R'G'B'A u8"), projection, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  for (guint y = 0; y < 80; y++)
    for (guint x = 0; x < 96; x++)
      {
        const guint8 *expected = gdk_pixbuf_get_pixels (reference) + y * gdk_pixbuf_get_rowstride (reference) + x * 4;
        const guint8 *actual = projection + (y * 96 + x) * 4;
        if (memcmp (expected, actual, 4)) differences++;
        for (guint c = 0; c < 4; c++) maximum = MAX (maximum, ABS ((gint) expected[c] - actual[c]));
        g_assert_cmpuint (expected[3], ==, actual[3]);
      }
  g_test_message ("Exact ordinary projection comparison: %u/7680 different pixels, max channel delta %u",
                   differences, maximum);
  g_assert_cmpuint (maximum, ==, 0);
  g_assert_cmpuint (differences, ==, 0);
  g_object_unref (reference); g_free (path); g_free (projection); g_object_unref (image);
}
static void clones (void)
{
  const gchar *names[] = {"legacy-runtime/clone-normal-in-group.xcf", "legacy-runtime/clone-group.xcf"};
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      GimpImage *image = open_fixture (names[i]);
      GList *layers = gimp_image_get_layer_list (image);
      GimpCloneLayer *clone = NULL;
      for (GList *p = layers; p; p = p->next)
        if (GIMP_IS_CLONE_LAYER (p->data)) clone = p->data;
      g_assert_nonnull (clone);
      g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_LIVE);
      g_assert_cmpstr (gimp_object_get_name (gimp_clone_layer_get_source (clone)), ==,
                       i ? "source group" : "source child");
      g_assert_cmpstr (g_object_get_data (G_OBJECT (clone), "gimp-painter-xcf-original-name"), ==,
                       i ? "source group" : "source child");
      pixel (GIMP_LAYER (clone), 0, 0, 191, 32, 64, 255);
      original_exact (image, names[i]);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
      {
        GimpLayer *child = find_layer (image, "source child");
        guint8 edited[4] = {31, 127, 225, 255};
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (child)), GEGL_RECTANGLE (0, 0, 1, 1),
                         0, babl_format ("R'G'B'A u8"), edited, GEGL_AUTO_ROWSTRIDE);
        gimp_drawable_update (GIMP_DRAWABLE (child), 0, 0, 1, 1);
        if (i) gimp_pickable_flush (GIMP_PICKABLE (gimp_clone_layer_get_source (clone)));
        pixel (GIMP_LAYER (clone), 0, 0, 31, 127, 225, 255);
        original_exact (image, names[i]);
      }
      g_list_free (layers); g_object_unref (image);
    }
}
static void filter_recovery (void)
{
  GimpImage *image = open_fixture ("legacy-runtime/filter-edge.xcf");
  GList *layers = gimp_image_get_layer_list (image);
  GimpFilterLayer *filter = NULL;
  gchar *name;
  GBytes *raw;
  for (GList *p = layers; p; p = p->next)
    if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_assert_nonnull (filter);
  name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-edge"); g_free (name);
  raw = gimp_filter_layer_ref_definition (filter);
  g_assert_cmpuint (g_bytes_get_size (raw), ==, 89); g_bytes_unref (raw);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  pixel (GIMP_LAYER (filter), 12, 12, 0, 0, 0, 255);
  original_exact (image, "legacy-runtime/filter-edge.xcf");
  g_list_free (layers); g_object_unref (image);
  while (g_main_context_iteration (NULL, FALSE));
}
static void standard (void)
{
  GimpImage *image = open_fixture ("gimp3-baseline.xcf");
  original_exact (image, "gimp3-baseline.xcf");
  g_assert_cmpint (gimp_image_get_precision (image), ==, GIMP_PRECISION_U8_NON_LINEAR);
  g_object_unref (image);
}
static void recovery_handlers (void)
{
  const gchar *names[] = {"gimp-xcf-load-standard", "gimp-xcf-load-painter"};
  gchar *path = fixture_path ("legacy-runtime/ordinary-layers.xcf");
  GFile *file = g_file_new_for_path (path);
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      GError *error = NULL;
      GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, names[i]));
      GimpImage *image;
      g_assert_nonnull (proc);
      g_assert_nonnull (g_slist_find (gimp->plug_in_manager->display_load_procs, proc));
      g_assert_null (proc->extensions_list); g_assert_null (proc->magics_list);
      image = open_file (file, names[i], NULL, &error);
      g_assert_no_error (error); g_assert_nonnull (image); g_object_unref (image);
    }
  g_object_unref (file); g_free (path);
}
typedef struct { GObject parent; gint count, cancel_at; gboolean ended; } TestProgress;
typedef struct { GObjectClass parent; } TestProgressClass;
static GimpProgress *progress_start (GimpProgress *p, gboolean cancellable, const gchar *message)
{ if (((TestProgress *) p)->cancel_at == 0) gimp_progress_cancel (p); return p; }
static void progress_pulse (GimpProgress *p)
{ TestProgress *self = (TestProgress *) p; if (++self->count == self->cancel_at) gimp_progress_cancel (p); }
static void progress_end (GimpProgress *p) { ((TestProgress *) p)->ended = TRUE; }
static gboolean progress_active (GimpProgress *p) { return !((TestProgress *) p)->ended; }
static void progress_iface (GimpProgressInterface *iface)
{ iface->start = progress_start; iface->end = progress_end; iface->pulse = progress_pulse; iface->is_active = progress_active; }
GType test_progress_get_type (void);
G_DEFINE_TYPE_WITH_CODE (TestProgress, test_progress, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_PROGRESS, progress_iface))
static void test_progress_class_init (TestProgressClass *klass) {}
static void test_progress_init (TestProgress *self) {}
static void cancel (void)
{
  gchar *path = fixture_path ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file = g_file_new_for_path (path);
  for (gint point = 0; point < 9; point++)
    {
      gint before = gimp_container_get_n_children (gimp->images);
      GError *error = NULL;
      GInputStream *input = G_INPUT_STREAM (g_file_read (file, NULL, &error));
      TestProgress *progress = g_object_new (test_progress_get_type (), NULL);
      GimpImage *image;
      progress->cancel_at = point;
      image = xcf_load_stream (gimp, input, file, GIMP_PROGRESS (progress), &error);
      g_assert_null (image); g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_assert_true (progress->ended); g_assert_true (g_input_stream_is_closed (input));
      g_clear_error (&error); g_object_unref (progress); g_object_unref (input);
      while (g_main_context_iteration (NULL, FALSE));
      g_assert_cmpint (gimp_container_get_n_children (gimp->images), ==, before);
    }
  g_object_unref (file); g_free (path);
}
static guint temporary_snapshots (void)
{
  GDir *directory = g_dir_open (g_get_tmp_dir (), 0, NULL);
  const gchar *name;
  guint count = 0;
  while ((name = g_dir_read_name (directory)))
    if (g_str_has_prefix (name, "gimp-xcf-read-")) count++;
  g_dir_close (directory); return count;
}
static GimpImage *open_bytes (const guint8 *data, gsize size, XcfPainterDialect dialect, GError **error)
{
  GInputStream *input = g_memory_input_stream_new_from_data (data, size, NULL);
  GimpImage *image = xcf_load_stream_with_dialect (gimp, input, NULL, NULL, dialect, error);
  g_assert_true (g_input_stream_is_closed (input)); g_object_unref (input); return image;
}
static void ambiguity_and_failures (void)
{
  guint8 dual[46] = {0};
  guint snapshots = temporary_snapshots ();
  gint before = gimp_container_get_n_children (gimp->images);
  GError *error = NULL;
  GimpImage *image;
  memcpy (dual, "gimp xcf v004", 13); dual[17] = dual[21] = 1;
  image = open_bytes (dual, sizeof dual, XCF_PAINTER_DIALECT_AUTO, &error);
  g_assert_null (image); g_assert_nonnull (strstr (error->message, "dialect")); g_clear_error (&error);
  for (gint dialect = XCF_PAINTER_DIALECT_STANDARD; dialect <= XCF_PAINTER_DIALECT_LEGACY; dialect++)
    {
      image = open_bytes (dual, sizeof dual, dialect, &error);
      g_assert_no_error (error); g_assert_nonnull (image); g_object_unref (image);
    }
  for (gsize prefix = 0; prefix < 34; prefix++)
    {
      image = open_bytes (dual, prefix, XCF_PAINTER_DIALECT_LEGACY, &error);
      g_assert_null (image); g_assert_nonnull (error); g_clear_error (&error);
    }
  g_assert_cmpint (gimp_container_get_n_children (gimp->images), ==, before);
  g_assert_cmpuint (temporary_snapshots (), ==, snapshots);
}
static void put32 (guint8 *data, guint offset, guint32 value)
{ value = GUINT32_TO_BE (value); memcpy (data + offset, &value, 4); }
static void absent_mode_historical_default (void)
{
  guint8 data[120] = {0};
  GError *error = NULL;
  GimpImage *image;
  memcpy (data, "gimp xcf v003", 13);
  put32 (data, 14, 1); put32 (data, 18, 1); put32 (data, 34, 46);
  put32 (data, 46, 1); put32 (data, 50, 1); put32 (data, 54, 1);
  put32 (data, 58, 2); data[62] = 'L'; put32 (data, 72, 80);
  put32 (data, 80, 1); put32 (data, 84, 1); put32 (data, 88, 4); put32 (data, 92, 100);
  put32 (data, 100, 1); put32 (data, 104, 1); put32 (data, 108, 116);
  data[116] = 255; data[119] = 128;
  image = open_bytes (data, sizeof data, XCF_PAINTER_DIALECT_AUTO, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  g_assert_cmpint (gimp_layer_get_mode (find_layer (image, "L")), ==, GIMP_LAYER_MODE_PAINTER_NORMAL);
  g_object_unref (image);
  image = open_bytes (data, sizeof data, XCF_PAINTER_DIALECT_STANDARD, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  g_assert_cmpint (gimp_layer_get_mode (find_layer (image, "L")), ==, GIMP_LAYER_MODE_NORMAL);
  g_object_unref (image);
}
static void dual_valid_short_extension (void)
{
  /* Synthetic v3: standard33/float0 and legacy33/NULL source both have valid
   * subsequent metadata and pixels. Legacy consumes END beyond declared size4.
   * Standard hierarchy words become an unknown legacy property before its END. */
  guint8 data[148] = {0};
  GError *error = NULL;
  GimpImage *image;
  memcpy (data, "gimp xcf v003", 13);
  put32 (data, 14, 1); put32 (data, 18, 1); put32 (data, 34, 46);
  put32 (data, 46, 1); put32 (data, 50, 1); put32 (data, 54, 1);
  put32 (data, 58, 2); data[62] = 'L';
  put32 (data, 64, 33); put32 (data, 68, 4);
  put32 (data, 84, 108); put32 (data, 100, 108);
  put32 (data, 108, 1); put32 (data, 112, 1); put32 (data, 116, 4); put32 (data, 120, 128);
  put32 (data, 128, 1); put32 (data, 132, 1); put32 (data, 136, 144);
  data[144] = 255; data[147] = 255;
  image = open_bytes (data, sizeof data, XCF_PAINTER_DIALECT_AUTO, &error);
  g_assert_null (image); g_assert_nonnull (strstr (error->message, "dialect")); g_clear_error (&error);
  for (gint dialect = XCF_PAINTER_DIALECT_STANDARD; dialect <= XCF_PAINTER_DIALECT_LEGACY; dialect++)
    {
      image = open_bytes (data, sizeof data, dialect, &error);
      g_assert_no_error (error); g_assert_nonnull (image);
      g_assert_cmpint (GIMP_IS_CLONE_LAYER (find_layer (image, "L")), ==, dialect == XCF_PAINTER_DIALECT_LEGACY);
      pixel (find_layer (image, "L"), 0, 0, 255, 0, 0, 255);
      g_object_unref (image);
    }
}
static void explicit_provenance_resolves_ambiguity (void)
{
  guint8 original[148]={0};GError *error=NULL;GimpImage *image,*empty;
  XcfPainterSave *snapshot;GimpParasite *marker;const guint8 *blob;guint32 blob_size;
  const gchar *name="gimp-painter-image";guint32 name_size=strlen(name)+1;gsize extra;guint8 *data;
  memcpy(original,"gimp xcf v003",13);put32(original,14,1);put32(original,18,1);put32(original,34,46);
  put32(original,46,1);put32(original,50,1);put32(original,54,1);put32(original,58,2);original[62]='L';
  put32(original,64,33);put32(original,68,4);put32(original,84,108);put32(original,100,108);
  put32(original,108,1);put32(original,112,1);put32(original,116,4);put32(original,120,128);
  put32(original,128,1);put32(original,132,1);put32(original,136,144);original[144]=original[147]=255;
  empty=gimp_image_new(gimp,1,1,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  snapshot=xcf_painter_prepare_save(empty,&error);g_assert_no_error(error);g_assert_nonnull(snapshot);
  marker=xcf_painter_image_parasite(snapshot);blob=gimp_parasite_get_data(marker,&blob_size);
  extra=8+4+name_size+4+4+blob_size;data=g_malloc0(sizeof original+extra);
  memcpy(data,original,26);memcpy(data+26+extra,original+26,sizeof original-26);
  put32(data,26,21);put32(data,30,extra-8);put32(data,34,name_size);memcpy(data+38,name,name_size);
  put32(data,38+name_size,GIMP_PARASITE_PERSISTENT);put32(data,42+name_size,blob_size);memcpy(data+46+name_size,blob,blob_size);
  put32(data,34+extra,46+extra);put32(data,84+extra,108+extra);put32(data,100+extra,108+extra);
  put32(data,120+extra,128+extra);put32(data,136+extra,144+extra);
  image=open_bytes(data,sizeof original+extra,XCF_PAINTER_DIALECT_AUTO,&error);g_assert_no_error(error);g_assert_nonnull(image);
  g_assert_false(GIMP_IS_CLONE_LAYER(find_layer(image,"L")));g_object_unref(image);
  image=open_bytes(data,sizeof original+extra,XCF_PAINTER_DIALECT_LEGACY,&error);g_assert_no_error(error);g_assert_nonnull(image);
  g_assert_true(GIMP_IS_CLONE_LAYER(find_layer(image,"L")));g_object_unref(image);
  data[46+name_size+8]=2; /* Unsupported marker cannot manufacture evidence. */
  image=open_bytes(data,sizeof original+extra,XCF_PAINTER_DIALECT_AUTO,&error);
  g_assert_null(image);g_assert_nonnull(strstr(error->message,"dialect"));g_clear_error(&error);
  g_free(data);gimp_parasite_free(marker);xcf_painter_free_save(snapshot);g_object_unref(empty);
}
static void unresolved_and_length_recovery (void)
{
  gchar *path = fixture_path ("legacy-runtime/clone-normal-in-group.xcf"), *bytes;
  gsize size;
  GError *error = NULL;
  GimpImage *image;
  GimpCloneLayer *clone;
  gchar *name;
  g_assert_true (g_file_get_contents (path, &bytes, &size, &error)); g_assert_no_error (error);
  memcpy (bytes + 573, "missing name", 13);
  image = open_bytes ((guint8 *) bytes, size, XCF_PAINTER_DIALECT_AUTO, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  clone = GIMP_CLONE_LAYER (find_layer (image, "clone"));
  g_assert_null (gimp_clone_layer_get_source (clone));
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_PENDING);
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "missing name"); g_free (name); g_object_unref (image);
  /* Old readers consume the actual name+END even when outer size is wrong. */
  memset (bytes + 565, 0, 4); bytes[568] = 4;
  image = open_bytes ((guint8 *) bytes, size, XCF_PAINTER_DIALECT_LEGACY, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  clone = GIMP_CLONE_LAYER (find_layer (image, "clone"));
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "missing name"); g_free (name); g_object_unref (image);
  g_free (bytes); g_free (path);
}
static void large_source (void)
{
  const goffset size = (goffset) 256 * 1024 * 1024 + 1;
  gchar *path = fixture_path ("legacy-runtime/ordinary-layers.xcf"), *bytes;
  gsize length;
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-large-source-XXXXXX", &io, &error);
  GOutputStream *output;
  GInputStream *input;
  GimpImage *image;
  GBytes *original;
  guint snapshots = temporary_snapshots ();
  g_assert_no_error (error);
  g_assert_true (g_file_get_contents (path, &bytes, &length, &error));
  output = g_io_stream_get_output_stream (G_IO_STREAM (io));
  g_assert_true (g_output_stream_write_all (output, bytes, length, NULL, NULL, &error));
  g_assert_true (g_seekable_truncate (G_SEEKABLE (io), size, NULL, &error));
  g_assert_true (g_output_stream_flush (output, NULL, &error));
  input = G_INPUT_STREAM (g_file_read (file, NULL, &error));
  image = xcf_load_stream (gimp, input, file, NULL, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  original = xcf_painter_ref_original (image);
  g_assert_cmpuint (g_bytes_get_size (original), ==, size);
  g_bytes_unref (original); g_object_unref (image); g_object_unref (input);
  g_object_unref (io); g_file_delete (file, NULL, NULL); g_object_unref (file);
  g_assert_cmpuint (temporary_snapshots (), ==, snapshots);
  g_free (bytes); g_free (path);
}
static void unknown_extension_preserves_destination (void)
{
  GimpImage *image = open_fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFileIOStream *io;
  GError *error = NULL;
  GFile *file = g_file_new_tmp ("painter-save-sentinel-XXXXXX", &io, &error);
  GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  GimpLayer *clone = find_layer (image, "clone");
  GimpParasite *unknown;
  const gchar sentinel[] = "unchanged existing destination";
  gchar *contents;
  gsize size;
  g_assert_no_error (error);
  g_assert_true (g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (io)),
                                            sentinel, sizeof sentinel, NULL, NULL, &error));
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error)); g_object_unref (io);
  /* Never overwrite an unknown version's custom semantics. */
  unknown = gimp_parasite_new ("gimp-painter-item", GIMP_PARASITE_PERSISTENT, 3, "v99");
  gimp_item_parasite_attach (GIMP_ITEM (clone), unknown, FALSE); gimp_parasite_free (unknown);
  g_assert_cmpint (file_save (gimp, image, NULL, file, proc, GIMP_RUN_NONINTERACTIVE,
                              FALSE, FALSE, FALSE, &error), !=, GIMP_PDB_SUCCESS);
  g_assert_nonnull (strstr (error->message, "Unknown Painter extension")); g_clear_error (&error);
  g_assert_true (g_file_load_contents (file, NULL, &contents, &size, NULL, &error));
  g_assert_cmpmem (contents, size, sentinel, sizeof sentinel);
  g_free (contents); g_file_delete (file, NULL, NULL); g_object_unref (file); g_object_unref (image);
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
#define ADD(name) g_test_add_func ("/painter-xcf-open/" #name, name)
  ADD (absent_mode_historical_default); ADD (unknown_extension_preserves_destination); ADD (dual_valid_short_extension); ADD (explicit_provenance_resolves_ambiguity); ADD (ambiguity_and_failures); ADD (unresolved_and_length_recovery); ADD (large_source);
  ADD (ordinary); ADD (ordinary_projection_exact_gate); ADD (clones); ADD (filter_recovery); ADD (standard); ADD (recovery_handlers); ADD (cancel);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
