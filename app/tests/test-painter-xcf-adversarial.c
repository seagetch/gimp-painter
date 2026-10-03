/* SPDX-License-Identifier: GPL-3.0-or-later
 * Synthetic wire-level regression scenes, not historical-writer fixtures.
 * All fixtures are constructed independently of the production writer. */
#include "config.h"
#include <string.h>
#include <zlib.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimp-painter-provenance.h"
#include "core/gimpchannel.h"
#include "core/gimpclonelayer.h"
#include "core/gimpdrawablefilter.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-duplicate.h"
#include "core/gimpitemlist.h"
#include "core/gimplayer.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-save.h"
#include "xcf/xcf.h"
#include "xcf/xcf-private.h"
#include "xcf/xcf-load.h"
#include "xcf/painter-xcf-load.h"
#include "xcf/painter-xcf-preserve.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;
static const guint8 colors[2][4] = {{17, 33, 65, 255}, {91, 72, 53, 255}};
typedef struct { guint ow, compression, kind, order[4]; } Scene;
typedef struct {
  GByteArray *bytes;
  guint ow, layer, hierarchy_field, hierarchy, level, tiles[2];
} Wire;
static void u32 (GByteArray *b, guint32 v)
{ v = GUINT32_TO_BE (v); g_byte_array_append (b, (guint8 *) &v, 4); }
static void offset (GByteArray *b, guint ow, guint64 v)
{ if (ow == 8) u32 (b, v >> 32); u32 (b, v); }
static void patch (GByteArray *b, guint at, guint ow, guint64 v)
{
  g_assert_cmpuint (at + ow, <=, b->len);
  for (guint i = 0; i < ow; ++i) b->data[at + ow - i - 1] = v >> (8 * i);
}
static void string (GByteArray *b, const gchar *v)
{ u32 (b, strlen (v) + 1); g_byte_array_append (b, (const guint8 *) v, strlen (v) + 1); }
static void prop (GByteArray *b, guint tag, GByteArray *value)
{ u32 (b, tag); u32 (b, value->len); g_byte_array_append (b, value->data, value->len); }
static void scalar (GByteArray *b, guint tag, guint32 value)
{ u32 (b, tag); u32 (b, 4); u32 (b, value); }
static void end (GByteArray *b) { u32 (b, 0); u32 (b, 0); }
static GByteArray *tile (guint compression, guint which, guint pixels)
{
  GByteArray *b = g_byte_array_new ();
  guint8 raw[256];
  for (guint i = 0; i < pixels; ++i) memcpy (raw + 4 * i, colors[which], 4);
  if (compression == COMPRESS_NONE) g_byte_array_append (b, raw, 4 * pixels);
  else if (compression == COMPRESS_RLE)
    for (guint c = 0; c < 4; ++c)
      { guint8 encoded[2] = {pixels - 1, colors[which][c]}; g_byte_array_append (b, encoded, 2); }
  else
    {
      guint8 encoded[512]; uLongf length = sizeof encoded;
      g_assert_cmpint (compress2 (encoded, &length, raw, 4 * pixels, Z_BEST_SPEED), ==, Z_OK);
      g_byte_array_append (b, encoded, length);
    }
  return b;
}
/* kind: 0 ordinary, 1 late group, 2 Clone, 3 Filter, 4 repeated Filter */
static Wire scene_wire (const Scene *scene)
{
  Wire w = {0};
  GByteArray *b = g_byte_array_new (), *p = g_byte_array_new ();
  GByteArray *blocks[4];
  guint position[4], table;
  const guint version = scene->kind >= 2 ? 3 : scene->ow == 8 ? 13 : 7;
  gchar header[14];
  g_snprintf (header, sizeof header, "gimp xcf v%03u", version);
  g_byte_array_append (b, (guint8 *) header, sizeof header);
  u32 (b, 128); u32 (b, 1); u32 (b, GIMP_RGB);
  if (version >= 4) u32 (b, GIMP_PRECISION_U8_NON_LINEAR);
  u32 (b, PROP_COMPRESSION); u32 (b, 1);
  { guint8 compression = scene->compression; g_byte_array_append (b, &compression, 1); }
  if (scene->kind == 1)
    {
      u32 (p, 0); u32 (p, G_MAXUINT32); string (p, "late-group-members");
      prop (b, PROP_ITEM_SET, p); g_byte_array_set_size (p, 0);
    }
  end (b); table = b->len;
  offset (b, scene->ow, 0); offset (b, scene->ow, 0); offset (b, scene->ow, 0);
  w.layer = b->len; patch (b, table, scene->ow, w.layer);
  u32 (b, 128); u32 (b, 1); u32 (b, GIMP_RGBA_IMAGE); string (b, "pixels");
  if (scene->kind)
    {
      u32 (b, PROP_ACTIVE_LAYER); u32 (b, 0);
      scalar (b, PROP_VISIBLE, FALSE); scalar (b, PROP_LINKED, TRUE);
      scalar (b, PROP_TATTOO, 701); scalar (b, PROP_OPACITY, 153);
      scalar (b, PROP_LOCK_CONTENT, TRUE); scalar (b, PROP_LOCK_ALPHA, TRUE);
      scalar (b, PROP_MODE, scene->kind >= 2 ? 3 : GIMP_LAYER_MODE_MULTIPLY);
      u32 (p, (guint32) -5); u32 (p, 7); prop (b, PROP_OFFSETS, p); g_byte_array_set_size (p, 0);
      string (p, "retained-parasite"); u32 (p, GIMP_PARASITE_PERSISTENT); u32 (p, 4); u32 (p, 0x12345678);
      prop (b, PROP_PARASITES, p); g_byte_array_set_size (p, 0);
      if (scene->kind == 1)
        {
          scalar (b, PROP_LOCK_POSITION, TRUE); scalar (b, PROP_LOCK_VISIBILITY, TRUE);
          scalar (b, PROP_COLOR_TAG, GIMP_COLOR_TAG_VIOLET);
          scalar (b, PROP_BLEND_SPACE, GIMP_LAYER_COLOR_SPACE_RGB_LINEAR);
          scalar (b, PROP_COMPOSITE_SPACE, GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR);
          scalar (b, PROP_COMPOSITE_MODE, GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP);
          scalar (b, PROP_ITEM_SET_ITEM, 0);
          u32 (b, PROP_GROUP_ITEM); u32 (b, 0);
        }
      else
        {
          string (p, scene->kind == 2 ? "unresolved-source" : "plug-in-edge");
          if (scene->kind == 3)
            {
              for (guint i = 0; i < 3; ++i) { u32 (p, 1); u32 (p, 0); }
              scalar (p, 5, 0x3f800000); scalar (p, 2, 1);
            }
          end (p); prop (b, scene->kind == 2 ? 33 : 32, p); g_byte_array_set_size (p, 0);
          if (scene->kind == 4)
            {
              string (p, "later-procedure"); end (p); prop (b, 32, p); g_byte_array_set_size (p, 0);
            }
        }
    }
  end (b); w.hierarchy_field = b->len;
  offset (b, scene->ow, 0); offset (b, scene->ow, 0);
  blocks[0] = g_byte_array_new (); u32 (blocks[0], 128); u32 (blocks[0], 1); u32 (blocks[0], 4);
  offset (blocks[0], scene->ow, 0); offset (blocks[0], scene->ow, 0);
  blocks[1] = g_byte_array_new (); u32 (blocks[1], 128); u32 (blocks[1], 1);
  for (guint i = 0; i < 3; ++i) offset (blocks[1], scene->ow, 0);
  blocks[2] = tile (scene->compression, 0, 64); blocks[3] = tile (scene->compression, 1, 64);
  for (guint i = 0; i < 4; ++i)
    {
      guint block = scene->order[i]; position[block] = b->len;
      g_byte_array_append (b, blocks[block]->data, blocks[block]->len);
    }
  w.hierarchy = position[0]; w.level = position[1]; w.tiles[0] = position[2]; w.tiles[1] = position[3];
  patch (b, w.hierarchy_field, scene->ow, w.hierarchy);
  patch (b, w.hierarchy + 12, scene->ow, w.level);
  for (guint i = 0; i < 2; ++i) patch (b, w.level + 8 + i * scene->ow, scene->ow, w.tiles[i]);
  for (guint i = 0; i < 4; ++i) g_byte_array_unref (blocks[i]);
  g_byte_array_unref (p); w.bytes = b; w.ow = scene->ow; return w;
}
static GimpImage *load_wire (Wire *w, gboolean legacy, gboolean expect_image)
{
  GError *error = NULL;
  GInputStream *input = g_memory_input_stream_new_from_data (w->bytes->data, w->bytes->len, NULL);
  GimpImage *image = xcf_load_stream_with_dialect (gimp, input, NULL, NULL,
    legacy ? XCF_PAINTER_DIALECT_LEGACY : XCF_PAINTER_DIALECT_STANDARD, &error);
  g_assert_true (g_input_stream_is_closed (input)); g_object_unref (input);
  if (expect_image) { g_assert_no_error (error); g_assert_nonnull (image); }
  else { g_assert_null (image); g_assert_nonnull (error); g_clear_error (&error); }
  return image;
}
static GimpLayer *first_layer (GimpImage *image)
{
  GList *layers = gimp_image_get_layer_list (image); GimpLayer *layer;
  g_assert_nonnull (layers); layer = layers->data; g_list_free (layers); return layer;
}
static void pixels (GimpImage *image, guint which, const guint8 *expected)
{
  guint8 actual[256];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (first_layer (image))),
    GEGL_RECTANGLE (which * 64, 0, 64, 1), 1, babl_format ("R'G'B'A u8"), actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual, sizeof actual, expected, sizeof actual);
}
static void solid (GimpImage *image, guint which, guint color)
{ guint8 expected[256]; for (guint i = 0; i < 64; ++i) memcpy (expected + 4 * i, colors[color], 4); pixels (image, which, expected); }
static void original (GimpImage *image, Wire *w)
{
  GBytes *raw = xcf_painter_ref_original (image); gsize size; const void *bytes;
  g_assert_nonnull (raw); bytes = g_bytes_get_data (raw, &size);
  g_assert_cmpmem (bytes, size, w->bytes->data, w->bytes->len); g_bytes_unref (raw);
}
static void complete (GimpImage *image)
{
  GError *error = NULL;
  XcfPainterSave *save = xcf_painter_prepare_save (image, &error);
  g_assert_no_error (error); g_assert_nonnull (save); xcf_painter_free_save (save);
}
static void refused (GimpImage *image)
{
  GError *error = NULL; gchar *contents; gsize length;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-adversarial-sentinel-XXXXXX", &io, &error);
  const gchar sentinel[] = "Never replace this existing destination";
  GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  g_assert_no_error (error);
  g_assert_true (g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (io)), sentinel, sizeof sentinel, NULL, NULL, &error));
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error)); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, proc, GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error), !=, GIMP_PDB_SUCCESS);
  g_assert_nonnull (error); g_assert_nonnull (strstr (error->message, "incomplete")); g_clear_error (&error);
  g_assert_true (g_file_load_contents (file, NULL, &contents, &length, NULL, &error)); g_assert_no_error (error);
  g_assert_cmpmem (contents, length, sentinel, sizeof sentinel);
  g_free (contents); g_file_delete (file, NULL, NULL); g_object_unref (file);
}
static void layout (gconstpointer data)
{
  const Scene *s = data; Wire w = scene_wire (s); GimpImage *image = load_wire (&w, FALSE, TRUE);
  solid (image, 0, 0); solid (image, 1, 1); complete (image); original (image, &w);
  g_object_unref (image); g_byte_array_unref (w.bytes);
}
static void alias (gconstpointer data)
{
  const Scene *s = data; Wire w = scene_wire (s); GimpImage *image;
  patch (w.bytes, w.level + 8 + w.ow, w.ow, w.tiles[0]);
  image = load_wire (&w, FALSE, TRUE); solid (image, 0, 0); solid (image, 1, 0); complete (image);
  g_object_unref (image); g_byte_array_unref (w.bytes);
}
static void empty_level_is_legal (void)
{
  const guint8 transparent[256] = {0};
  for (guint ow = 4; ow <= 8; ow += 4)
    for (guint compression = 0; compression < 3; ++compression)
      {
        Scene s = {ow, compression, 0, {0,1,2,3}}; Wire w = scene_wire (&s);
        GimpImage *image;
        patch (w.bytes, w.level + 8, ow, 0);
        image = load_wire (&w, FALSE, TRUE);
        pixels (image, 0, transparent); pixels (image, 1, transparent); complete (image);
        g_object_unref (image); g_byte_array_unref (w.bytes);
      }
}
static void structural_raw_alias (void)
{
  Scene s = {8, COMPRESS_NONE, 0, {0,1,2,3}}; Wire w = scene_wire (&s); GimpImage *image;
  patch (w.bytes, w.level + 8, w.ow, w.hierarchy);
  patch (w.bytes, w.level + 8 + w.ow, w.ow, w.hierarchy);
  image = load_wire (&w, FALSE, TRUE);
  pixels (image, 0, w.bytes->data + w.hierarchy); pixels (image, 1, w.bytes->data + w.hierarchy); complete (image);
  g_object_unref (image); g_byte_array_unref (w.bytes);
}
static void late_attributes (gconstpointer data)
{
  Scene s = {4, COMPRESS_NONE, GPOINTER_TO_UINT (data), {0,1,2,3}};
  Wire w = scene_wire (&s); GimpImage *image = load_wire (&w, s.kind >= 2, TRUE);
  GimpLayer *layer = first_layer (image); GimpItem *item = GIMP_ITEM (layer);
  const GimpParasite *parasite; const void *bytes; guint32 size;
  GList *selected = gimp_image_get_selected_layers (image), *sets;
  guint found = 0;
  g_assert_true (s.kind == 1 ? GIMP_IS_GROUP_LAYER (layer) : s.kind == 2 ? GIMP_IS_CLONE_LAYER (layer) : GIMP_IS_FILTER_LAYER (layer));
  g_assert_cmpstr (gimp_object_get_name (item), ==, "pixels");
  g_assert_false (gimp_item_get_visible (item)); g_assert_true (gimp_item_get_lock_content (item));
  g_assert_cmpuint (gimp_item_get_tattoo (item), ==, 701);
  g_assert_true (gimp_item_get_by_id (gimp, gimp_item_get_id (item)) == item);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (layer), .6, 1e-9);
  /* Empty groups derive their bounds from their empty child set on resize. */
  g_assert_cmpint (gimp_item_get_offset_x (item), ==, s.kind == 1 ? 0 : -5);
  g_assert_cmpint (gimp_item_get_offset_y (item), ==, s.kind == 1 ? 0 : 7);
  g_assert_true (g_list_find (selected, layer) != NULL);
  parasite = gimp_item_parasite_find (item, "retained-parasite"); g_assert_nonnull (parasite);
  bytes = gimp_parasite_get_data (parasite, &size); g_assert_cmpuint (size, ==, 4);
  { const guint8 expected[] = {0x12, 0x34, 0x56, 0x78}; g_assert_cmpmem (bytes, size, expected, sizeof expected); }
  sets = gimp_image_get_stored_item_sets (image, GIMP_TYPE_LAYER);
  for (GList *p = sets; p; p = p->next)
    {
      GList *members = gimp_item_list_get_items (p->data, NULL);
      g_assert_cmpuint (g_list_length (members), ==, 1); g_assert_true (members->data == layer); ++found; g_list_free (members);
    }
  g_assert_cmpuint (found, ==, s.kind == 1 ? 2 : 1);
  if (s.kind == 1)
    {
      g_assert_true (gimp_item_get_lock_position (item)); g_assert_true (gimp_item_get_lock_visibility (item));
      g_assert_cmpint (gimp_item_get_color_tag (item), ==, GIMP_COLOR_TAG_VIOLET);
      g_assert_cmpint (gimp_layer_get_mode (layer), ==, GIMP_LAYER_MODE_MULTIPLY);
      g_assert_cmpint (gimp_layer_get_blend_space (layer), ==, GIMP_LAYER_COLOR_SPACE_RGB_LINEAR);
      g_assert_cmpint (gimp_layer_get_composite_space (layer), ==, GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR);
      g_assert_cmpint (gimp_layer_get_composite_mode (layer), ==, GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP);
    }
  else { g_assert_true (gimp_layer_get_lock_alpha (layer)); solid (image, 0, 0); solid (image, 1, 1); }
  if (s.kind == 4)
    {
      gchar *name = gimp_painter_provenance_dup_text (G_OBJECT (layer), GIMP_PAINTER_PROVENANCE_NAME);
      GBytes *raw = gimp_painter_provenance_ref_bytes (G_OBJECT (layer), GIMP_PAINTER_PROVENANCE_EXTENSION);
      GByteArray *expected = g_byte_array_new (); gsize length; const void *bytes;
      g_assert_cmpstr (name, ==, "later-procedure"); g_free (name);
      name = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer));
      g_assert_cmpstr (name, ==, "later-procedure"); g_free (name);
      string (expected, "later-procedure"); end (expected);
      bytes = g_bytes_get_data (raw, &length);
      g_assert_cmpmem (bytes, length, expected->data, expected->len);
      g_bytes_unref (raw); g_byte_array_unref (expected);
    }
  original (image, &w); complete (image); g_object_unref (image); g_byte_array_unref (w.bytes);
  while (g_main_context_iteration (NULL, FALSE));
}
/* Damage classes deliberately distinguish total rejection from tile salvage. */
typedef enum {
  SHORT_HIERARCHY_POINTER, SHORT_HIERARCHY_HEADER, SHORT_LEVEL_POINTER, SHORT_LEVEL_HEADER,
  SHORT_FIRST_TILE_POINTER, SHORT_FIRST_TILE, EMPTY_FIRST_TILE, SHORT_SECOND_TILE,
  EMPTY_SECOND_TILE, SHORT_ZLIB_OUTPUT_FIRST, SHORT_ZLIB_OUTPUT_SECOND,
  EMPTY_ZLIB_OUTPUT_FIRST, EMPTY_ZLIB_OUTPUT_SECOND, ZERO_RLE_RUN_FIRST, ZERO_RLE_RUN_SECOND,
  MISSING_NEXT_TILE, EXTRA_TILE, SHORT_FINAL_TILE_POINTER
} Damage;
typedef struct { guint ow, compression, kind; Damage damage; } Damaged;
static void damaged (gconstpointer data)
{
  const Damaged *d = data; Scene s = {d->ow, d->compression, d->kind, {0,1,2,3}};
  Wire w; GimpImage *image;
  gboolean partial = d->damage == SHORT_SECOND_TILE || d->damage == EMPTY_SECOND_TILE ||
    d->damage == SHORT_ZLIB_OUTPUT_SECOND || d->damage == EMPTY_ZLIB_OUTPUT_SECOND ||
    d->damage == ZERO_RLE_RUN_SECOND || d->damage == MISSING_NEXT_TILE || d->damage == EXTRA_TILE ||
    d->damage == SHORT_FINAL_TILE_POINTER;
  if (d->damage == SHORT_FINAL_TILE_POINTER)
    { s.order[0] = 0; s.order[1] = 2; s.order[2] = 3; s.order[3] = 1; }
  w = scene_wire (&s);
  switch (d->damage)
    {
    case SHORT_HIERARCHY_POINTER: g_byte_array_set_size (w.bytes, w.hierarchy_field + w.ow - 1); break;
    case SHORT_HIERARCHY_HEADER: g_byte_array_set_size (w.bytes, w.hierarchy + 11); break;
    case SHORT_LEVEL_POINTER: g_byte_array_set_size (w.bytes, w.hierarchy + 12 + w.ow - 1); break;
    case SHORT_LEVEL_HEADER: g_byte_array_set_size (w.bytes, w.level + 7); break;
    case SHORT_FIRST_TILE_POINTER: g_byte_array_set_size (w.bytes, w.level + 8 + w.ow - 1); break;
    case SHORT_FIRST_TILE: g_byte_array_set_size (w.bytes, w.tiles[0] + 1); break;
    case EMPTY_FIRST_TILE: patch (w.bytes, w.level + 8, w.ow, w.bytes->len); break;
    case SHORT_SECOND_TILE: g_byte_array_set_size (w.bytes, w.tiles[1] + 1); break;
    case EMPTY_SECOND_TILE: g_byte_array_set_size (w.bytes, w.tiles[1]); break;
    case SHORT_ZLIB_OUTPUT_FIRST: case SHORT_ZLIB_OUTPUT_SECOND:
    case EMPTY_ZLIB_OUTPUT_FIRST: case EMPTY_ZLIB_OUTPUT_SECOND:
      {
        guint which = d->damage == SHORT_ZLIB_OUTPUT_SECOND || d->damage == EMPTY_ZLIB_OUTPUT_SECOND;
        GByteArray *t = tile (COMPRESS_ZLIB, which, d->damage >= EMPTY_ZLIB_OUTPUT_FIRST ? 0 : 1);
        g_byte_array_set_size (w.bytes, w.tiles[which]); g_byte_array_append (w.bytes, t->data, t->len); g_byte_array_unref (t); break;
      }
    case ZERO_RLE_RUN_FIRST: case ZERO_RLE_RUN_SECOND:
      { const guint8 bad[] = {127, 0, 0, 1}; guint which = d->damage == ZERO_RLE_RUN_SECOND;
        g_byte_array_set_size (w.bytes, w.tiles[which]); g_byte_array_append (w.bytes, bad, sizeof bad); break; }
    case MISSING_NEXT_TILE: patch (w.bytes, w.level + 8 + w.ow, w.ow, 0); break;
    case EXTRA_TILE: patch (w.bytes, w.level + 8 + 2 * w.ow, w.ow, w.tiles[0]); break;
    case SHORT_FINAL_TILE_POINTER: g_byte_array_set_size (w.bytes, w.level + 8 + 3 * w.ow - 1); break;
    }
  image = load_wire (&w, d->kind >= 2, partial);
  if (partial)
    {
      GimpImage *copy;
      gchar *reason = gimp_painter_provenance_dup_text (G_OBJECT (first_layer (image)), GIMP_PAINTER_PROVENANCE_INCOMPLETE_PIXELS);
      g_assert_nonnull (reason); g_free (reason); solid (image, 0, 0); original (image, &w); refused (image);
      if (d->kind == 3)
        {
          GimpFilterLayerSnapshot state; GimpFilterLayer *filter = GIMP_FILTER_LAYER (first_layer (image));
          while (g_main_context_iteration (NULL, FALSE));
          g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &state));
          g_assert_false (state.cache_complete); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0); solid (image, 0, 0);
        }
      copy = gimp_image_duplicate (image); g_assert_nonnull (copy); g_object_unref (image);
      original (copy, &w); refused (copy); solid (copy, 0, 0);
      if (d->kind == 3)
        {
          GimpFilterLayerSnapshot state;
          while (g_main_context_iteration (NULL, FALSE));
          g_assert_true (gimp_filter_layer_get_snapshot_state (GIMP_FILTER_LAYER (first_layer (copy)), &state));
          g_assert_false (state.cache_complete); solid (copy, 0, 0);
        }
      g_object_unref (copy);
      while (g_main_context_iteration (NULL, FALSE));
    }
  g_byte_array_unref (w.bytes);
}
static void partial_image (void)
{
  Scene s = {8, COMPRESS_NONE, 0, {0,1,2,3}}; Wire w = scene_wire (&s);
  GimpImage *image, *copy;
  /* Layer table's terminator becomes a second, out-of-file layer reference.
   * Its failed load leaves the complete first layer recoverable. */
  patch (w.bytes, w.layer - 2 * w.ow, w.ow, w.bytes->len + 1);
  image = load_wire (&w, FALSE, TRUE); solid (image, 0, 0); solid (image, 1, 1); refused (image);
  copy = gimp_image_duplicate (image); g_object_unref (image); original (copy, &w); refused (copy); g_object_unref (copy);
  g_byte_array_unref (w.bytes);
}
static guint64 read_offset (GByteArray *b, guint at, guint ow)
{
  guint64 value = 0; g_assert_cmpuint (at + ow, <=, b->len);
  for (guint i = 0; i < ow; ++i) value = (value << 8) | b->data[at + i];
  return value;
}
static guint skip_properties (GByteArray *b, guint at)
{
  for (;;)
    {
      guint tag = read_offset (b, at, 4), size = read_offset (b, at + 4, 4); at += 8;
      if (!tag) return at;
      g_assert_cmpuint (size, <=, b->len - at); at += size;
    }
}
/* Real native read fault after a complete effect and its mask have been
 * parsed, before ownership of the FilterData list transfers to the layer.
 * Truncating an ordinary file cannot isolate this fault: its offset table
 * precedes the effect data. The normal snapshot/read route is tested above. */
typedef struct {
  GMemoryInputStream parent;
  goffset fault, effect;
  gint first_id;
  guint partial, captured, finalized;
  gboolean armed, failed;
} EffectsFaultInput;
typedef struct { GMemoryInputStreamClass parent; } EffectsFaultInputClass;
static GType effects_fault_input_get_type (void);
G_DEFINE_TYPE (EffectsFaultInput, effects_fault_input, G_TYPE_MEMORY_INPUT_STREAM)
static void effects_mask_finalized (gpointer data, GObject *where)
{ ++((EffectsFaultInput *) data)->finalized; }
static gssize effects_fault_read (GInputStream *input, void *buffer, gsize count,
                                 GCancellable *cancel, GError **error)
{
  EffectsFaultInput *self = (EffectsFaultInput *) input;
  goffset position = g_seekable_tell (G_SEEKABLE (input));
  if (position >= self->effect) self->armed = TRUE;
  if (self->armed && position >= self->fault && position < self->fault + 8)
    {
      if (!self->captured)
        for (gint id = self->first_id + 1; id <= self->first_id + 16; ++id)
          {
            GimpItem *item = gimp_item_get_by_id (gimp, id);
            if (item && !g_strcmp0 (gimp_object_get_name (item), "effects-short-read-mask"))
              {
                g_object_weak_ref (G_OBJECT (item), effects_mask_finalized, self);
                ++self->captured;
              }
          }
      if (self->failed) return 0;
      self->failed = TRUE;
      count = MIN (count, self->partial);
      if (!count) return 0;
    }
  return G_INPUT_STREAM_CLASS (effects_fault_input_parent_class)->read_fn (input, buffer, count, cancel, error);
}
static void effects_fault_input_class_init (EffectsFaultInputClass *klass)
{ G_INPUT_STREAM_CLASS (klass)->read_fn = effects_fault_read; }
static void effects_fault_input_init (EffectsFaultInput *self) {}
static void native_effects_table_short_read (void)
{
  Scene s = {8, COMPRESS_NONE, 0, {0,1,2,3}}; Wire w = scene_wire (&s), native = {0};
  GimpImage *image = load_wire (&w, FALSE, TRUE);
  GimpLayer *base = first_layer (image);
  GeglNode *node = gegl_node_new_child (NULL, "operation", "gegl:brightness-contrast", NULL);
  GimpDrawableFilter *effect = gimp_drawable_filter_new (GIMP_DRAWABLE (base), "short table effect", node, "gimp-gegl");
  GFileIOStream *io; GError *error = NULL; gchar *contents; gsize size;
  GFile *file = g_file_new_tmp ("painter-effects-short-read-XXXXXX.xcf", &io, &error);
  GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  guint version, at, effect_offset;
  g_assert_no_error (error); g_io_stream_close (G_IO_STREAM (io), NULL, NULL); g_object_unref (io);
  g_object_unref (node); gimp_drawable_filter_apply (effect, NULL);
  g_assert_true (gimp_drawable_filter_commit (effect, TRUE, NULL, FALSE));
  gimp_drawable_filter_layer_mask_freeze (effect);
  gimp_object_set_name (GIMP_OBJECT (gimp_drawable_filter_get_mask (effect)), "effects-short-read-mask");
  g_object_unref (effect);
  g_assert_cmpint (file_save (gimp, image, NULL, file, proc, GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); g_object_unref (image);
  g_assert_true (g_file_load_contents (file, NULL, &contents, &size, NULL, &error)); g_assert_no_error (error);
  g_file_delete (file, NULL, NULL); g_object_unref (file);
  native.bytes = g_byte_array_new_take ((guint8 *) contents, size);
  version = g_ascii_strtoull (contents + 10, NULL, 10); g_assert_cmpuint (version, >=, 20);
  native.ow = 8; at = skip_properties (native.bytes, 30); at = read_offset (native.bytes, at, 8);
  at += 12; at += 4 + read_offset (native.bytes, at, 4); at = skip_properties (native.bytes, at);
  effect_offset = read_offset (native.bytes, at + 16, 8); g_assert_cmpuint (effect_offset, >, at + 24);
  /* The unmodified writer output also passes the ordinary Open route. */
  image = load_wire (&native, FALSE, TRUE); complete (image); g_object_unref (image);
  for (guint partial = 0; partial < 8; ++partial)
    {
      XcfInfo info = {0};
      GimpImage *sentinel = load_wire (&w, FALSE, TRUE);
      EffectsFaultInput *input = g_object_new (effects_fault_input_get_type (), NULL);
      input->first_id = gimp_item_get_id (GIMP_ITEM (first_layer (sentinel)));
      g_object_unref (sentinel);
      input->fault = at + 24; input->effect = effect_offset; input->partial = partial;
      g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (input), native.bytes->data, native.bytes->len, NULL);
      g_assert_true (g_seekable_seek (G_SEEKABLE (input), 14, G_SEEK_SET, NULL, &error));
      info.gimp = gimp; info.input = G_INPUT_STREAM (input); info.cp = 14;
      info.seekable = G_SEEKABLE (input);
      info.file_version = version; info.bytes_per_offset = 8;
      info.painter_source = g_bytes_new (native.bytes->data, native.bytes->len);
      info.painter_cancellable = g_cancellable_new ();
      image = xcf_load_image (gimp, &info, &error);
      g_assert_null (image); g_assert_nonnull (error); g_clear_error (&error);
      g_assert_true (input->failed); g_assert_cmpuint (input->captured, ==, 1);
      g_assert_cmpuint (input->finalized, ==, 1);
      g_bytes_unref (info.painter_source); g_object_unref (info.painter_cancellable);
      g_assert_null (info.selected_layers); g_assert_null (info.linked_layers);
      g_object_unref (input);
    }
  g_byte_array_unref (native.bytes); g_byte_array_unref (w.bytes);
}
static void native_partial_filter (void)
{
  Scene s = {4, COMPRESS_NONE, 3, {0,1,2,3}}; Wire legacy = scene_wire (&s), native = {0};
  GimpImage *image = load_wire (&legacy, TRUE, TRUE), *copy;
  GFileIOStream *io; GError *error = NULL;
  GFile *file = g_file_new_tmp ("painter-native-partial-XXXXXX.xcf", &io, &error);
  GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  gchar *contents; gsize size; guint version, at, hierarchy, level, tile2;
  GimpFilterLayerSnapshot state;
  g_assert_no_error (error); g_io_stream_close (G_IO_STREAM (io), NULL, NULL); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, proc, GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); g_object_unref (image);
  g_assert_true (g_file_load_contents (file, NULL, &contents, &size, NULL, &error)); g_assert_no_error (error);
  g_file_delete (file, NULL, NULL); g_object_unref (file);
  native.bytes = g_byte_array_new_take ((guint8 *) contents, size);
  version = g_ascii_strtoull (contents + 10, NULL, 10); native.ow = version >= 11 ? 8 : 4;
  at = skip_properties (native.bytes, version >= 4 ? 30 : 26);
  at = read_offset (native.bytes, at, native.ow);
  at += 12; at += 4 + read_offset (native.bytes, at, 4); at = skip_properties (native.bytes, at);
  hierarchy = read_offset (native.bytes, at, native.ow);
  level = read_offset (native.bytes, hierarchy + 12, native.ow);
  tile2 = read_offset (native.bytes, level + 8 + native.ow, native.ow);
  g_byte_array_set_size (native.bytes, tile2 + 1);
  image = load_wire (&native, FALSE, TRUE);
  g_assert_true (GIMP_IS_FILTER_LAYER (first_layer (image)));
  while (g_main_context_iteration (NULL, FALSE));
  g_assert_true (gimp_filter_layer_get_snapshot_state (GIMP_FILTER_LAYER (first_layer (image)), &state));
  g_assert_false (state.cache_complete); solid (image, 0, 0); original (image, &native); refused (image);
  copy = gimp_image_duplicate (image); g_object_unref (image);
  while (g_main_context_iteration (NULL, FALSE));
  g_assert_true (gimp_filter_layer_get_snapshot_state (GIMP_FILTER_LAYER (first_layer (copy)), &state));
  g_assert_false (state.cache_complete); original (copy, &native); refused (copy); solid (copy, 0, 0);
  g_object_unref (copy); g_byte_array_unref (native.bytes); g_byte_array_unref (legacy.bytes);
  while (g_main_context_iteration (NULL, FALSE));
}
static void short_empty_tables (void)
{
  for (guint version = 7; version <= 18; version += version == 7 ? 6 : 5)
    {
      guint ow = version >= 11 ? 8 : 4, count = version >= 18 ? 3 : 2;
      Wire w = {0}; gchar header[14]; guint start;
      w.bytes = g_byte_array_new ();
      g_snprintf (header, sizeof header, "gimp xcf v%03u", version);
      g_byte_array_append (w.bytes, (guint8 *) header, sizeof header);
      u32 (w.bytes, 1); u32 (w.bytes, 1); u32 (w.bytes, GIMP_RGB); u32 (w.bytes, GIMP_PRECISION_U8_NON_LINEAR);
      end (w.bytes); start = w.bytes->len;
      for (guint i = 0; i < count; ++i) offset (w.bytes, ow, 0);
      { GimpImage *image = load_wire (&w, FALSE, TRUE); complete (image); g_object_unref (image); }
      for (guint remaining = count * ow; remaining > 0; --remaining)
        {
          g_byte_array_set_size (w.bytes, start + remaining - 1);
          load_wire (&w, FALSE, FALSE);
        }
      g_byte_array_unref (w.bytes);
    }
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-xcf-adversarial/effects_table_short_read", native_effects_table_short_read);
  for (guint ow = 4; ow <= 8; ow += 4)
    for (guint compression = 0; compression < 3; ++compression)
      {
        for (guint a = 0; a < 4; ++a) for (guint b = 0; b < 4; ++b)
          for (guint c = 0; c < 4; ++c) for (guint d = 0; d < 4; ++d)
            if (a != b && a != c && a != d && b != c && b != d && c != d)
              {
                Scene *s = g_new (Scene, 1); *s = (Scene) {ow, compression, 0, {a,b,c,d}};
                gchar *name = g_strdup_printf ("/painter-xcf-adversarial/layout/%u/%u/%u%u%u%u", ow, compression, a,b,c,d);
                g_test_add_data_func_full (name, s, layout, g_free); g_free (name);
              }
        { Scene *s = g_new (Scene, 1); *s = (Scene) {ow, compression, 0, {0,1,2,3}};
          gchar *name = g_strdup_printf ("/painter-xcf-adversarial/alias/%u/%u", ow, compression);
          g_test_add_data_func_full (name, s, alias, g_free); g_free (name); }
        for (guint damage = 0; damage <= SHORT_FINAL_TILE_POINTER; ++damage)
          {
            if (damage >= SHORT_ZLIB_OUTPUT_FIRST && damage <= EMPTY_ZLIB_OUTPUT_SECOND && compression != COMPRESS_ZLIB) continue;
            if (damage >= ZERO_RLE_RUN_FIRST && damage <= ZERO_RLE_RUN_SECOND && compression != COMPRESS_RLE) continue;
            Damaged *d = g_new (Damaged, 1); *d = (Damaged) {ow, compression, 0, damage};
            gchar *name = g_strdup_printf ("/painter-xcf-adversarial/damage/%u/%u/%u", ow, compression, damage);
            g_test_add_data_func_full (name, d, damaged, g_free); g_free (name);
          }
      }
  for (guint compression = 0; compression < 3; ++compression)
    { Damaged *d = g_new (Damaged, 1); *d = (Damaged) {4, compression, 3, SHORT_SECOND_TILE};
      gchar *name = g_strdup_printf ("/painter-xcf-adversarial/partial-filter/%u", compression);
      g_test_add_data_func_full (name, d, damaged, g_free); g_free (name); }
  g_test_add_func ("/painter-xcf-adversarial/structural-raw-alias", structural_raw_alias);
  g_test_add_func ("/painter-xcf-adversarial/empty-level-is-legal", empty_level_is_legal);
  g_test_add_func ("/painter-xcf-adversarial/short-empty-tables", short_empty_tables);
  g_test_add_func ("/painter-xcf-adversarial/native-partial-filter", native_partial_filter);
  g_test_add_func ("/painter-xcf-adversarial/partial-image", partial_image);
  g_test_add_data_func ("/painter-xcf-adversarial/late-group", GUINT_TO_POINTER (1), late_attributes);
  g_test_add_data_func ("/painter-xcf-adversarial/late-clone", GUINT_TO_POINTER (2), late_attributes);
  g_test_add_data_func ("/painter-xcf-adversarial/late-filter", GUINT_TO_POINTER (3), late_attributes);
  g_test_add_data_func ("/painter-xcf-adversarial/late-filter-repeat", GUINT_TO_POINTER (4), late_attributes);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
