/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimp-painter-provenance.h"
#include "core/gimpcontainer.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-new.h"
#include "core/gimpimage-duplicate.h"
#include "vectors/gimppath.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimppickable.h"
#include "core/gimpprogress.h"
#include "core/gimpparamspecs.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-open.h"
#include "file/file-save.h"
#include "xcf/xcf.h"
#include "xcf/painter-xcf-preserve.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static gchar *path (const gchar *name)
{ return g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "migration/fixtures", name, NULL); }
static GFile *temporary_file (void)
{
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-roundtrip-XXXXXX.xcf", &io, NULL);
  g_assert_nonnull (file); g_io_stream_close (G_IO_STREAM (io), NULL, NULL); g_object_unref (io); return file;
}
static GimpImage *load (GFile *file)
{
  GError *error = NULL;
  GimpPDBStatusType status;
  GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-load"));
  GimpImage *image = file_open_image (gimp, gimp_get_user_context (gimp), NULL, file, 0, 0,
                                      FALSE, proc, GIMP_RUN_NONINTERACTIVE, &status, NULL, &error);
  g_assert_no_error (error); g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS); g_assert_nonnull (image); return image;
}
static GimpImage *fixture (const gchar *name)
{ gchar *filename = path (name); GFile *file = g_file_new_for_path (filename); GimpImage *image = load (file); g_object_unref (file); g_free (filename); return image; }
static void save (GimpImage *image, GFile *file)
{
  GError *error = NULL;
  GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  GimpPDBStatusType status = file_save (gimp, image, NULL, file, proc, GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error);
  if (error) g_test_message ("save error: %s", error->message);
  g_assert_no_error (error); g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS);
}
static GimpLayer *layer (GimpImage *image, const gchar *name)
{
  GList *layers = gimp_image_get_layer_list (image); GimpLayer *found = NULL;
  for (GList *p = layers; p; p = p->next) if (!g_strcmp0 (gimp_object_get_name (p->data), name)) found = p->data;
  g_list_free (layers); return found;
}
static void pixel (GimpLayer *layer, guint8 r, guint8 g, guint8 b, guint8 a)
{
  guint8 actual[4], expected[] = {r,g,b,a};
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), GEGL_RECTANGLE (0,0,1,1), 1,
                   babl_format ("R'G'B'A u8"), actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  if(memcmp(actual,expected,4))
    g_test_message("Pixel in %s: actual %u,%u,%u,%u expected %u,%u,%u,%u",gimp_object_get_name(layer),
                   actual[0],actual[1],actual[2],actual[3],r,g,b,a);
  g_assert_cmpmem (actual, 4, expected, 4);
}
static void clones_edit_save_reopen (void)
{
  const gchar *fixtures[] = {"legacy-runtime/clone-normal-in-group.xcf", "legacy-runtime/clone-group.xcf"};
  for (guint i = 0; i < 2; ++i)
    {
      GimpImage *image = fixture (fixtures[i]); GFile *file = temporary_file ();
      GimpImage *copy;
      GimpCloneLayer *clone;
      GimpLayer *source;
      save (image, file); copy = load (file);
      clone = GIMP_CLONE_LAYER (layer (copy, "clone")); g_assert_true (GIMP_IS_CLONE_LAYER (clone));
      g_assert_true (gimp_clone_layer_get_source (clone) == layer (copy, i ? "source group" : "source child"));
      pixel (GIMP_LAYER (clone), 191,32,64,255);
      source = layer (copy, "source child");
      {
        guint8 edit[] = {13,77,199,255};
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), GEGL_RECTANGLE (0,0,1,1), 0,
                         babl_format ("R'G'B'A u8"), edit, GEGL_AUTO_ROWSTRIDE);
      }
      gimp_drawable_update (GIMP_DRAWABLE (source), 0,0,1,1);
      if (i) gimp_pickable_flush (GIMP_PICKABLE (gimp_clone_layer_get_source (clone)));
      pixel (GIMP_LAYER (clone), 13,77,199,255);
      gimp_object_set_name (GIMP_OBJECT (gimp_clone_layer_get_source (clone)), "renamed source");
      gimp_item_set_offset (GIMP_ITEM (clone), 12, 9); gimp_layer_set_opacity (GIMP_LAYER (clone), .37, FALSE);
      save (copy, file); g_object_unref (copy); copy = load (file);
      clone = GIMP_CLONE_LAYER (layer (copy, "clone"));
      g_assert_true (gimp_clone_layer_get_source (clone) == layer (copy, "renamed source"));
      g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 12);
      g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone)), ==, 9);
      g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (GIMP_LAYER (clone)), .37, 1e-7);
      pixel (GIMP_LAYER (clone), 13,77,199,255);
      g_object_unref (copy); g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
    }
}
static void filter_cache_and_arguments (void)
{
  GimpImage *image = fixture ("legacy-runtime/filter-edge.xcf"), *copy;
  GFile *file = temporary_file ();
  GimpFilterLayer *filter;
  GList *layers;
  GBytes *raw;
  GimpValueArray *args;
  save (image, file); copy = load (file); layers = gimp_image_get_layer_list (copy); filter = NULL;
  for (GList *p = layers; p; p = p->next) if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_list_free (layers); g_assert_nonnull (filter);
  raw = gimp_filter_layer_ref_definition (filter); g_assert_cmpuint (g_bytes_get_size (raw), ==, 89);
  args = gimp_filter_layer_dup_args (filter); g_assert_nonnull (args); g_assert_cmpint (gimp_value_array_length (args), ==, 6);
  g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (args, 3)), ==, 2);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_value_set_double (gimp_value_array_index (args, 3), 3.25);
  g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-edge", raw, args, NULL));
  save (copy, file); g_object_unref (copy); copy = load (file);
  layers = gimp_image_get_layer_list (copy); filter = NULL;
  for (GList *p = layers; p; p = p->next) if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_list_free (layers); g_assert_nonnull (filter);
  gimp_value_array_unref (args); args = gimp_filter_layer_dup_args (filter);
  g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (args, 3)), ==, 3.25);
  {
    gint64 deadline = g_get_monotonic_time () + 3000000;
    while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_CLEAN && g_get_monotonic_time () < deadline)
      { while (g_main_context_iteration (NULL, FALSE)); g_usleep (1000); }
    g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
    g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), >, 0);
  }
  save (copy, file); g_object_unref (copy); copy = load (file);
  layers = gimp_image_get_layer_list (copy); filter = NULL;
  for (GList *p = layers; p; p = p->next) if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_list_free (layers); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_bytes_unref (raw); gimp_value_array_unref (args);
  g_object_unref (copy); g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static GVariant *parasite_capsule (const GimpParasite *parasite)
{
  const guint8 *data;
  guint32 size;
  GBytes *bytes;
  GVariant *value;
  g_assert_nonnull (parasite); data = gimp_parasite_get_data (parasite, &size);
  g_assert_cmpuint (size, >, 12); g_assert_cmpmem (data, 5, "GPXCF", 5);
  bytes = g_bytes_new (data + 12, size - 12);
  value = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, bytes, FALSE));
  g_bytes_unref (bytes); g_assert_true (g_variant_is_normal_form (value)); return value;
}
static GVariant *capsule (GimpItem *item)
{ return parasite_capsule (gimp_item_parasite_find (item,"gimp-painter-item")); }
static void compare_projection (GimpImage *first, GimpImage *second)
{
  const gint w = gimp_image_get_width (first), h = gimp_image_get_height (first);
  guint8 *a = g_malloc ((gsize) w*h*4), *b = g_malloc ((gsize) w*h*4);
  g_assert_cmpint (gimp_image_get_width (second), ==, w); g_assert_cmpint (gimp_image_get_height (second), ==, h);
  gimp_pickable_flush (GIMP_PICKABLE (first)); gimp_pickable_flush (GIMP_PICKABLE (second));
  gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (first)), GEGL_RECTANGLE (0,0,w,h),1,babl_format ("R'G'B'A u8"),a,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (second)),GEGL_RECTANGLE (0,0,w,h),1,babl_format ("R'G'B'A u8"),b,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (a,(gsize) w*h*4,b,(gsize) w*h*4); g_free (a); g_free (b);
}
static void ordinary_exact_roundtrip (void)
{
  GimpImage *image = fixture ("legacy-runtime/ordinary-layers.xcf");
  GFile *file = temporary_file (); GimpImage *copy;
  save (image,file); copy = load (file); compare_projection (image,copy);
  g_assert_nonnull (gimp_layer_get_mask (layer (copy,"painted stroke")));
  g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_channels (copy)), ==, 1);
  g_assert_cmpint (gimp_layer_get_mode (layer (copy,"painted stroke")), ==, GIMP_LAYER_MODE_PAINTER_MULTIPLY);
  g_object_unref (copy); g_object_unref (image); g_file_delete (file,NULL,NULL); g_object_unref (file);
}
static void duplicate_unknown_records (void)
{
  gchar *filename = path ("legacy-runtime/clone-normal-in-group.xcf"), *data;
  gsize size, found = 0;
  const guint8 tattoo_header[] = {0,0,0,20,0,0,0,4};
  const guint8 unknown_record[] = {0xf0,0x0d,0xba,0xbe,0,0,0,4,0x11,0x22,0x33,0x44};
  GInputStream *input;
  GimpImage *image,*reopened;
  GimpLayer *copy;
  GimpItem *original;
  GBytes *expected;
  GFile *file = temporary_file ();
  GError *error = NULL;
  goffset first_size = 0;
  g_assert_true (g_file_get_contents (filename,&data,&size,NULL));
  for (gsize i = 395; i + sizeof unknown_record <= size; ++i)
    if (!memcmp (data+i,tattoo_header,sizeof tattoo_header)) { found=i; break; }
  g_assert_cmpuint (found, >, 0); memcpy (data+found,unknown_record,sizeof unknown_record);
  input = g_memory_input_stream_new_from_data (data,size,NULL);
  image = xcf_load_stream (gimp,input,NULL,NULL,&error); g_assert_no_error (error); g_assert_nonnull (image);
  g_object_unref (input); original = GIMP_ITEM (layer (image,"clone"));
  expected = gimp_painter_provenance_ref_bytes (G_OBJECT (original), GIMP_PAINTER_PROVENANCE_PROPERTIES);
  copy = GIMP_LAYER (gimp_item_duplicate (original,GIMP_TYPE_CLONE_LAYER));
  { GBytes *actual = gimp_painter_provenance_ref_bytes (G_OBJECT (copy), GIMP_PAINTER_PROVENANCE_PROPERTIES);
    g_assert_true (g_bytes_equal (expected, actual)); g_bytes_unref (actual); }
  gimp_object_set_name (GIMP_OBJECT (copy),"edited duplicate");
  gimp_layer_set_opacity (copy,.25,FALSE); gimp_image_add_layer (image,copy,NULL,0,FALSE);
  copy = GIMP_LAYER (gimp_item_duplicate (original,GIMP_TYPE_LAYER));
  g_assert_false (GIMP_IS_CLONE_LAYER (copy));
  gimp_object_set_name (GIMP_OBJECT (copy),"converted ordinary");
  gimp_image_add_layer (image,copy,NULL,0,FALSE);
  for (guint repeat=0; repeat<3; ++repeat)
    {
      GVariant *record,*raw;
      gconstpointer actual,wanted;
      gsize actual_size,wanted_size;
      GFileInfo *stat;
      save (image,file); reopened=load(file);
      copy=layer(reopened,"edited duplicate"); g_assert_true (GIMP_IS_CLONE_LAYER (copy));
      g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == layer (reopened,"source child"));
      g_assert_cmpfloat (gimp_layer_get_opacity (copy), ==, .25);
      record=capsule (GIMP_ITEM(copy)); raw=g_variant_lookup_value(record,"original-properties",G_VARIANT_TYPE_BYTESTRING);
      g_assert_nonnull(raw); actual=g_variant_get_fixed_array(raw,&actual_size,1); wanted=g_bytes_get_data(expected,&wanted_size);
      g_assert_cmpmem(actual,actual_size,wanted,wanted_size);
      g_variant_unref(raw);g_variant_unref(record);
      copy=layer(reopened,"converted ordinary"); g_assert_false(GIMP_IS_CLONE_LAYER(copy));
      record=capsule(GIMP_ITEM(copy));
      { const gchar *kind,*owner; g_assert_true(g_variant_lookup(record,"kind","&s",&kind));
        g_assert_cmpstr(kind,==,"ordinary");
        g_assert_true(g_variant_lookup(record,"original-owner","&s",&owner));
        g_assert_nonnull(strstr(owner,"layer:")); }
      raw=g_variant_lookup_value(record,"original-properties",G_VARIANT_TYPE_BYTESTRING);
      actual=g_variant_get_fixed_array(raw,&actual_size,1);wanted=g_bytes_get_data(expected,&wanted_size);
      g_assert_cmpmem(actual,actual_size,wanted,wanted_size);g_variant_unref(raw);g_variant_unref(record);
      stat=g_file_query_info(file,G_FILE_ATTRIBUTE_STANDARD_SIZE,G_FILE_QUERY_INFO_NONE,NULL,NULL);
      if(!repeat) first_size=g_file_info_get_size(stat);
      else g_assert_cmpint(g_file_info_get_size(stat),<=,first_size+128);
      g_object_unref(stat);g_object_unref(image);image=reopened;
    }
  g_object_unref(image);g_bytes_unref(expected);g_file_delete(file,NULL,NULL);g_object_unref(file);g_free(data);g_free(filename);
}

static void newly_unknown_records_and_future_capsule (void)
{
  GimpImage *image=gimp_image_new(gimp,4,4,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR),*copy;
  GimpLayer *item=gimp_layer_new(image,4,4,babl_format("R'G'B'A u8"),"future layer",1,GIMP_LAYER_MODE_NORMAL);
  GFile *file=temporary_file();gchar *data;gsize length,raw_length,begin=G_MAXSIZE,record_offset=G_MAXSIZE;
  g_autoptr(GBytes) properties = NULL;const guint8 *raw;
  const guint8 future[]={ 'G','P','X','C','F',0,0,0,2,0,0,0,0xab,0xcd,0xef };
  const guint8 unknown[]={0xf0,0x0d,0xba,0xbf,0,0,0,4,0,0,0,7};
  gimp_image_add_layer(image,item,NULL,0,FALSE);gimp_item_set_color_tag(GIMP_ITEM(item),GIMP_COLOR_TAG_GRAY,FALSE);
  save(image,file);g_object_unref(image);image=load(file);item=layer(image,"future layer");
  properties=gimp_painter_provenance_ref_bytes (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_PROPERTIES);raw=g_bytes_get_data(properties,&raw_length);
  g_assert_true(g_file_load_contents(file,NULL,&data,&length,NULL,NULL));
  for(gsize i=0;i+raw_length<=length;++i)if(!memcmp(data+i,raw,raw_length)){begin=i;break;}
  g_assert_cmpuint(begin,!=,G_MAXSIZE);
  for(gsize i=0;i+8<=raw_length;)
    {
      guint32 type,size;memcpy(&type,raw+i,4);memcpy(&size,raw+i+4,4);type=GUINT32_FROM_BE(type);size=GUINT32_FROM_BE(size);
      if(type==34&&size==4){record_offset=i;break;}
      g_assert_cmpuint(size,<=,raw_length-i-8);i+=8+size;
    }
  g_assert_cmpuint(record_offset,!=,G_MAXSIZE);memcpy(data+begin+record_offset,unknown,sizeof unknown);
  g_assert_true(g_file_replace_contents(file,data,length,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,NULL));g_free(data);
  g_object_unref(image);image=load(file);
  for(guint repeat=0;repeat<2;++repeat)
    {
      GVariant *record,*sequences,*sequence;gsize size;const void *bytes;
      save(image,file);copy=load(file);item=layer(copy,"future layer");record=capsule(GIMP_ITEM(item));
      sequences=g_variant_lookup_value(record,"opaque-record-sequences",G_VARIANT_TYPE("aay"));g_assert_nonnull(sequences);
      g_assert_cmpuint(g_variant_n_children(sequences),==,1);sequence=g_variant_get_child_value(sequences,0);
      bytes=g_variant_get_fixed_array(sequence,&size,1);g_assert_cmpmem(bytes,size,unknown,sizeof unknown);
      g_variant_unref(sequence);g_variant_unref(sequences);g_variant_unref(record);g_object_unref(image);image=copy;
    }
  item=layer(image,"future layer");
  {GimpParasite *p=gimp_parasite_new("gimp-painter-item",GIMP_PARASITE_PERSISTENT,sizeof future,future);
   gimp_item_parasite_attach(GIMP_ITEM(item),p,FALSE);gimp_parasite_free(p);}
  save(image,file);copy=load(file);item=layer(copy,"future layer");
  {const GimpParasite *p=gimp_item_parasite_find(GIMP_ITEM(item),"gimp-painter-item");guint32 size;const void *bytes=gimp_parasite_get_data(p,&size);
   g_assert_cmpmem(bytes,size,future,sizeof future);g_assert_nonnull(gimp_item_parasite_find(GIMP_ITEM(item),"gimp-painter-origin"));}
  g_object_unref(copy);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}

static void typed_nested_expired_arguments (void)
{
  GimpImage *image = gimp_image_new (gimp,4,4,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR), *copy;
  GimpLayer *source = gimp_layer_new (image,4,4,babl_format("R'G'B'A u8"),"typed source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpLayer *filter = gimp_filter_layer_new (image,4,4,"typed filter",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpPath *vector = gimp_path_new (image,"typed path");
  GFile *file = temporary_file ();
  GValue values[16] = {G_VALUE_INIT}, nested_values[2] = {G_VALUE_INIT};
  GimpFilterArgumentSpec specs[16] = {{0}}, children[3] = {{0}};
  GimpFilterArgumentReference refs[4] = {{0}}, expired = {GIMP_TYPE_LAYER,70707,TRUE,TRUE};
  GObject *targets[4] = {G_OBJECT(image),G_OBJECT(source),NULL,G_OBJECT(vector)};
  GimpFilterArgumentsSnapshot *args,*restored,*nested;
  GError *error=NULL;
  const guint8 raw_data[]={'o','p','a','q','u','e',0,'d','e','f'};
  const guint8 bytes_data[]={0,255,4,0};
  const gint32 ints[]={G_MININT32,0,G_MAXINT32};
  const gdouble doubles[]={1.0,-0.0,3.25};
  gchar *strings[]={"alpha","beta",NULL}, *empty_strings[]={NULL};
  GBytes *raw=g_bytes_new(raw_data,sizeof raw_data), *bytes=g_bytes_new(bytes_data,sizeof bytes_data);
  GType types[]={G_TYPE_INT,G_TYPE_DOUBLE,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRV,GIMP_TYPE_INT32_ARRAY,GIMP_TYPE_DOUBLE_ARRAY,G_TYPE_BYTES,
                 GIMP_TYPE_CORE_OBJECT_ARRAY,GIMP_TYPE_CORE_OBJECT_ARRAY,GIMP_TYPE_CORE_OBJECT_ARRAY,GIMP_TYPE_VALUE_ARRAY,
                 G_TYPE_VARIANT,G_TYPE_FLOAT,G_TYPE_STRV,G_TYPE_STRV};
  guint64 nan_bits=G_GUINT64_CONSTANT(0x7ff8000011223344); gdouble nan_value;
  guint32 float_bits=0x7fc01234; gfloat float_value;
  gimp_image_add_layer(image,source,NULL,0,FALSE); gimp_image_add_layer(image,filter,NULL,0,FALSE);
  gimp_image_add_path(image,vector,NULL,0,FALSE);
  for(guint i=0;i<16;++i)
    { specs[i].value_type=types[i]; if(i<8||i>=12) { g_value_init(&values[i],types[i]); specs[i].value=&values[i]; } }
  g_value_set_int(&values[0],-123); memcpy(&nan_value,&nan_bits,8); g_value_set_double(&values[1],nan_value);
  g_value_set_string(&values[2],"opaque\xff string"); specs[3].is_null=TRUE;
  g_value_set_boxed(&values[4],strings); gimp_value_set_int32_array(&values[5],ints,3);
  gimp_value_set_double_array(&values[6],doubles,3); g_value_set_boxed(&values[7],bytes);
  refs[0]=(GimpFilterArgumentReference){GIMP_TYPE_IMAGE,gimp_image_get_id(image),TRUE,FALSE};
  refs[1]=(GimpFilterArgumentReference){GIMP_TYPE_LAYER,gimp_item_get_id(GIMP_ITEM(source)),TRUE,FALSE};
  refs[2]=(GimpFilterArgumentReference){GIMP_TYPE_LAYER,919191,TRUE,TRUE};
  refs[3]=(GimpFilterArgumentReference){GIMP_TYPE_PATH,gimp_item_get_id(GIMP_ITEM(vector)),TRUE,FALSE};
  specs[8].n_references=4;specs[8].references=refs;specs[8].targets=targets;
  specs[9].is_null=TRUE;
  g_value_init(&nested_values[0],G_TYPE_BOOLEAN);g_value_set_boolean(&nested_values[0],TRUE);
  g_value_init(&nested_values[1],G_TYPE_UINT64);g_value_set_uint64(&nested_values[1],G_MAXUINT64);
  children[0]=(GimpFilterArgumentSpec){.value_type=G_TYPE_BOOLEAN,.value=&nested_values[0]};
  children[1]=(GimpFilterArgumentSpec){.value_type=G_TYPE_UINT64,.value=&nested_values[1]};
  children[2]=(GimpFilterArgumentSpec){.value_type=G_TYPE_OBJECT,.n_references=1,.references=&expired};
  specs[11].n_children=3;specs[11].children=children;
  g_value_set_variant(&values[12],g_variant_new("(st)","nested variant",G_MAXUINT64));
  memcpy(&float_value,&float_bits,4);g_value_set_float(&values[13],float_value);
  specs[14].is_null=TRUE;g_value_set_boxed(&values[15],empty_strings);
  args=gimp_filter_arguments_snapshot_import(16,specs,&error);g_assert_no_error(error);g_assert_nonnull(args);
  g_assert_true(gimp_filter_layer_set_definition_with_snapshot(GIMP_FILTER_LAYER(filter),"unavailable-test-procedure",raw,args,&error));
  g_assert_no_error(error);gimp_filter_layer_mark_as_loaded(GIMP_FILTER_LAYER(filter));
  for(guint repeat=0;repeat<2;++repeat)
    {
      GValue value=G_VALUE_INIT;
      GimpFilterArgumentReference reference;
      GBytes *definition;
      save(image,file);copy=load(file);filter=layer(copy,"typed filter");g_assert_true(GIMP_IS_FILTER_LAYER(filter));
      g_assert_null(gimp_filter_layer_dup_args(GIMP_FILTER_LAYER(filter))); /* expired array remains typed */
      restored=gimp_filter_layer_snapshot_arguments(GIMP_FILTER_LAYER(filter));g_assert_nonnull(restored);
      g_assert_cmpuint(gimp_filter_arguments_snapshot_count(restored),==,16);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,0,&value));g_assert_cmpint(g_value_get_int(&value),==,-123);g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,1,&value));nan_value=g_value_get_double(&value);{guint64 bits;memcpy(&bits,&nan_value,8);g_assert_cmpuint(bits,==,nan_bits);}g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,2,&value));g_assert_cmpstr(g_value_get_string(&value),==,"opaque\xff string");g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_is_null(restored,3));
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,4,&value));
      {gchar **actual=g_value_get_boxed(&value);g_assert_cmpstr(actual[0],==,"alpha");g_assert_cmpstr(actual[1],==,"beta");g_assert_null(actual[2]);}g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,5,&value));
      {gsize length;const gint32 *actual=gimp_value_get_int32_array(&value,&length);g_assert_cmpuint(length,==,3);g_assert_cmpmem(actual,length*sizeof*actual,ints,sizeof ints);}g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,6,&value));
      {gsize length;const gdouble *actual=gimp_value_get_double_array(&value,&length);g_assert_cmpuint(length,==,3);g_assert_cmpmem(actual,length*sizeof*actual,doubles,sizeof doubles);}g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,7,&value));g_assert_true(g_bytes_equal(bytes,g_value_get_boxed(&value)));g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,0,&reference));g_assert_cmpint(reference.id,==,gimp_image_get_id(copy));g_assert_false(reference.expired);
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,1,&reference));g_assert_cmpint(reference.id,==,gimp_item_get_id(GIMP_ITEM(layer(copy,"typed source"))));
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,2,&reference));g_assert_true(reference.expired);g_assert_cmpint(reference.id,==,919191);
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,3,&reference));
      {GList *paths=gimp_image_get_path_list(copy);g_assert_cmpuint(g_list_length(paths),==,1);
       g_assert_cmpint(reference.id,==,gimp_item_get_id(GIMP_ITEM(paths->data)));g_assert_false(reference.expired);
       g_assert_nonnull(gimp_item_parasite_find(GIMP_ITEM(paths->data),"gimp-painter-item"));g_list_free(paths);}
      g_assert_true(gimp_filter_arguments_snapshot_is_null(restored,9));g_assert_false(gimp_filter_arguments_snapshot_is_null(restored,10));
      g_assert_cmpuint(gimp_filter_arguments_snapshot_reference_count(restored,10),==,0);
      nested=gimp_filter_arguments_snapshot_nested(restored,11);g_assert_nonnull(nested);
      g_assert_cmpuint(gimp_filter_arguments_snapshot_count(nested),==,3);
      g_assert_true(gimp_filter_arguments_snapshot_reference(nested,2,0,&reference));g_assert_true(reference.expired);g_assert_cmpint(reference.id,==,70707);
      g_assert_true(gimp_filter_arguments_snapshot_value(nested,0,&value));g_assert_true(g_value_get_boolean(&value));g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(nested,1,&value));g_assert_cmpuint(g_value_get_uint64(&value),==,G_MAXUINT64);g_value_unset(&value);
      gimp_filter_arguments_snapshot_free(nested);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,12,&value));g_assert_true(g_variant_equal(g_value_get_variant(&value),g_value_get_variant(&values[12])));g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,13,&value));float_value=g_value_get_float(&value);{guint32 bits;memcpy(&bits,&float_value,4);g_assert_cmpuint(bits,==,float_bits);}g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_is_null(restored,14));g_assert_false(gimp_filter_arguments_snapshot_is_null(restored,15));
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,15,&value));
      {gchar **actual=g_value_get_boxed(&value);g_assert_nonnull(actual);g_assert_null(actual[0]);}g_value_unset(&value);
      definition=gimp_filter_layer_ref_definition(GIMP_FILTER_LAYER(filter));g_assert_true(g_bytes_equal(raw,definition));g_bytes_unref(definition);
      gimp_filter_arguments_snapshot_free(restored);g_object_unref(image);image=copy;
    }
  for(guint i=0;i<16;++i)if(G_VALUE_TYPE(&values[i]))g_value_unset(&values[i]);
  g_value_unset(&nested_values[0]);g_value_unset(&nested_values[1]);gimp_filter_arguments_snapshot_free(args);
  g_bytes_unref(bytes);g_bytes_unref(raw);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}

typedef struct { GObject parent; gint count, cancel_at; gboolean close_cancel, mutate, ended; gint mutation_kind; GimpLayer *layer; GObject *release_on_start[3]; } SaveProgress;
typedef struct { GObjectClass parent; } SaveProgressClass;
static GimpProgress *save_progress_start (GimpProgress *progress, gboolean cancellable, const gchar *text)
{
  SaveProgress *self=(SaveProgress*)progress;
  g_assert_true(cancellable);if(self->cancel_at==0)gimp_progress_cancel(progress);
  for (guint i = 0; i < G_N_ELEMENTS (self->release_on_start); ++i)
    g_clear_object (&self->release_on_start[i]);
  return progress;
}
static void save_progress_value (GimpProgress *progress, gdouble value)
{
  SaveProgress *self=(SaveProgress*)progress;
  if(++self->count==self->cancel_at)
    {
      if(self->mutation_kind==1)gimp_clone_layer_set_source(GIMP_CLONE_LAYER(self->layer),NULL);
      else if(self->mutation_kind==2)g_assert_true(gimp_filter_layer_set_definition(GIMP_FILTER_LAYER(self->layer),"edited-unavailable",NULL,NULL,NULL));
      else if(self->mutate)gimp_layer_set_opacity(self->layer,.42,FALSE);
      else gimp_progress_cancel(progress);
    }
}
static void save_progress_text (GimpProgress *progress, const gchar *text)
{
  SaveProgress *self=(SaveProgress*)progress;
  if(self->close_cancel)
    {
      if(self->mutate)gimp_layer_set_opacity(self->layer,.31,FALSE);
      else gimp_progress_cancel(progress);
    }
}
static void save_progress_end (GimpProgress *progress) { ((SaveProgress*)progress)->ended=TRUE; }
static gboolean save_progress_active (GimpProgress *progress) { return !((SaveProgress*)progress)->ended; }
static void save_progress_iface (GimpProgressInterface *iface)
{ iface->start=save_progress_start;iface->set_value=save_progress_value;iface->set_text=save_progress_text;iface->end=save_progress_end;iface->is_active=save_progress_active; }
GType save_progress_get_type (void);
G_DEFINE_TYPE_WITH_CODE(SaveProgress,save_progress,G_TYPE_OBJECT,G_IMPLEMENT_INTERFACE(GIMP_TYPE_PROGRESS,save_progress_iface))
static void save_progress_class_init (SaveProgressClass *klass) {}
static void save_progress_init (SaveProgress *self) { self->cancel_at=-1; }
static void sentinel_write (GFile *file)
{ g_assert_true(g_file_replace_contents(file,"KEEP ORIGINAL",13,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,NULL)); }
static void sentinel_check (GFile *file)
{
  gchar *bytes;gsize size;
  g_assert_true(g_file_load_contents(file,NULL,&bytes,&size,NULL,NULL));g_assert_cmpmem(bytes,size,"KEEP ORIGINAL",13);g_free(bytes);
}
static void cancel_and_reentrant_edit (void)
{
  GimpImage *image=fixture("legacy-runtime/clone-normal-in-group.xcf");
  GFile *file=temporary_file();
  for(guint scene=0;scene<7;++scene)
    {
      GError *error=NULL;
      GOutputStream *output;
      SaveProgress *progress=g_object_new(save_progress_get_type(),NULL);
      progress->layer=layer(image,"clone");
      progress->cancel_at=scene<4?(gint)scene:1;
      progress->close_cancel=scene==4||scene==6;
      if(progress->close_cancel)progress->cancel_at=-1;
      progress->mutate=scene>=5;
      gimp_layer_set_opacity(progress->layer,1,FALSE);
      sentinel_write(file);output=G_OUTPUT_STREAM(g_file_replace(file,NULL,FALSE,G_FILE_CREATE_NONE,NULL,&error));
      g_assert_no_error(error);
      g_assert_false(xcf_save_stream(gimp,image,output,file,GIMP_PROGRESS(progress),&error));
      if(progress->mutate)g_assert_error(error,G_IO_ERROR,G_IO_ERROR_BUSY);
      else g_assert_error(error,G_IO_ERROR,G_IO_ERROR_CANCELLED);
      g_assert_true(progress->ended);g_assert_true(g_output_stream_is_closed(output));
      g_clear_error(&error);g_object_unref(output);g_object_unref(progress);sentinel_check(file);
    }
  g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
static void low_level_reentrant_definitions (void)
{
  const gchar *names[]={"legacy-runtime/clone-normal-in-group.xcf","legacy-runtime/filter-edge.xcf"};
  for(guint scene=0;scene<2;++scene)
    {
      GimpImage *image=fixture(names[scene]);GFile *file=temporary_file();GError *error=NULL;
      SaveProgress *progress=g_object_new(save_progress_get_type(),NULL);GList *layers=gimp_image_get_layer_list(image);
      GOutputStream *output;
      for(GList *p=layers;p;p=p->next)
        if(scene?GIMP_IS_FILTER_LAYER(p->data):GIMP_IS_CLONE_LAYER(p->data))progress->layer=p->data;
      g_list_free(layers);g_assert_nonnull(progress->layer);progress->cancel_at=1;progress->mutation_kind=scene+1;
      sentinel_write(file);output=G_OUTPUT_STREAM(g_file_replace(file,NULL,FALSE,G_FILE_CREATE_NONE,NULL,&error));g_assert_no_error(error);
      g_assert_false(xcf_save_stream(gimp,image,output,file,GIMP_PROGRESS(progress),&error));
      g_assert_error(error,G_IO_ERROR,G_IO_ERROR_BUSY);g_assert_true(progress->ended);sentinel_check(file);
      g_clear_error(&error);g_object_unref(output);g_object_unref(progress);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
    }
}
typedef struct { GFilterOutputStream parent; gsize limit,written; gboolean cancelled_close, fail_final_close; } FailOutput;
typedef struct { GFilterOutputStreamClass parent; } FailOutputClass;
static GOutputStream *fail_base (gpointer output) { return g_filter_output_stream_get_base_stream(G_FILTER_OUTPUT_STREAM(output)); }
static gssize fail_write (GOutputStream *output,const void *buffer,gsize count,GCancellable *cancel,GError **error)
{
  FailOutput *self=(FailOutput*)output;gssize n;
  if(self->written>=self->limit) { g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE,"Injected write failure");return -1; }
  n=g_output_stream_write(fail_base(output),buffer,MIN(count,self->limit-self->written),cancel,error);
  if(n>0)self->written+=n;
  return n;
}
static goffset fail_tell (GSeekable *stream) { return g_seekable_tell(G_SEEKABLE(fail_base(stream))); }
static gboolean fail_can_seek (GSeekable *stream) { return TRUE; }
static gboolean fail_seek (GSeekable *stream,goffset offset,GSeekType type,GCancellable *cancel,GError **error)
{ return g_seekable_seek(G_SEEKABLE(fail_base(stream)),offset,type,cancel,error); }
static gboolean fail_can_truncate (GSeekable *stream) { return FALSE; }
static gboolean fail_truncate (GSeekable *stream,goffset offset,GCancellable *cancel,GError **error)
{ return FALSE; }
static void fail_seek_iface (GSeekableIface *iface)
{ iface->tell=fail_tell;iface->can_seek=fail_can_seek;iface->seek=fail_seek;iface->can_truncate=fail_can_truncate;iface->truncate_fn=fail_truncate; }
GType fail_output_get_type (void);
G_DEFINE_TYPE_WITH_CODE(FailOutput,fail_output,G_TYPE_FILTER_OUTPUT_STREAM,G_IMPLEMENT_INTERFACE(G_TYPE_SEEKABLE,fail_seek_iface))
static gboolean fail_close (GOutputStream *output,GCancellable *cancel,GError **error)
{
  ((FailOutput*)output)->cancelled_close=g_cancellable_is_cancelled(cancel);
  if (((FailOutput*)output)->fail_final_close && !g_cancellable_is_cancelled (cancel))
    {
      GCancellable *abandon = g_cancellable_new ();
      g_cancellable_cancel (abandon);
      G_OUTPUT_STREAM_CLASS(fail_output_parent_class)->close_fn (output, abandon, NULL);
      g_object_unref (abandon);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Injected final close failure");
      return FALSE;
    }
  return G_OUTPUT_STREAM_CLASS(fail_output_parent_class)->close_fn(output,cancel,error);
}
static void fail_output_class_init (FailOutputClass *klass)
{ G_OUTPUT_STREAM_CLASS(klass)->write_fn=fail_write;G_OUTPUT_STREAM_CLASS(klass)->close_fn=fail_close; }
static void fail_output_init (FailOutput *self) {}
static void injected_write_failures (void)
{
  GimpImage *image=fixture("legacy-runtime/ordinary-layers.xcf");GFile *file=temporary_file();
  const gsize limits[]={0,40,128,800,2000};
  for(guint i=0;i<G_N_ELEMENTS(limits);++i)
    {
      GError *error=NULL;
      GOutputStream *base;
      FailOutput *output;
      sentinel_write(file);base=G_OUTPUT_STREAM(g_file_replace(file,NULL,FALSE,G_FILE_CREATE_NONE,NULL,&error));g_assert_no_error(error);
      output=g_object_new(fail_output_get_type(),"base-stream",base,"close-base-stream",TRUE,NULL);g_object_unref(base);output->limit=limits[i];
      g_assert_false(xcf_save_stream(gimp,image,G_OUTPUT_STREAM(output),file,NULL,&error));
      g_assert_error(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE);g_assert_nonnull(strstr(error->message,"Injected write failure"));
      g_assert_true(output->cancelled_close);g_assert_true(g_output_stream_is_closed(G_OUTPUT_STREAM(output)));
      g_clear_error(&error);g_object_unref(output);sentinel_check(file);
    }
  g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
static void save_observer_ownership (void)
{
  GimpImage *image=fixture("legacy-runtime/clone-normal-in-group.xcf");
  GimpLayer *clone=layer(image,"clone");GError *error=NULL;
  GObject *objects[]={G_OBJECT(image),G_OBJECT(clone),G_OBJECT(gimp_image_get_layers(image)),
                      G_OBJECT(gimp_drawable_get_buffer(GIMP_DRAWABLE(clone)))};
  for(guint repeat=0;repeat<16;++repeat)
    {
      XcfPainterSave *snapshot=xcf_painter_prepare_save(image,&error);g_assert_no_error(error);g_assert_nonnull(snapshot);
      g_assert_true(xcf_painter_save_unchanged(snapshot));
      for(guint i=0;i<G_N_ELEMENTS(objects);++i)
        g_assert_cmpuint(g_signal_handler_find(objects[i],G_SIGNAL_MATCH_DATA,0,0,NULL,NULL,snapshot),!=,0);
      xcf_painter_free_save(snapshot);
      for(guint i=0;i<G_N_ELEMENTS(objects);++i)
        g_assert_cmpuint(g_signal_handler_find(objects[i],G_SIGNAL_MATCH_DATA,0,0,NULL,NULL,snapshot),==,0);
      /* A stale callback would dereference the released transaction here. */
      gimp_layer_set_opacity(clone,(repeat%2)?0.5:0.75,FALSE);
    }
  g_object_unref(image);
}
static void duplicate_image_unknown_origin (void)
{
  gchar *filename=path("legacy-runtime/ordinary-layers.xcf"),*data;gsize size;gboolean changed=FALSE;
  GError *error=NULL;GInputStream *input;GimpImage *image,*copy,*reopened;GBytes *expected;GFile *file=temporary_file();
  g_assert_true(g_file_get_contents(filename,&data,&size,NULL));
  for(gsize i=26;i+8<=size;)
    {
      guint32 type,length;memcpy(&type,data+i,4);memcpy(&length,data+i+4,4);type=GUINT32_FROM_BE(type);length=GUINT32_FROM_BE(length);
      if(type==19&&length==8){guint32 unknown=GUINT32_TO_BE(0xf00dbabc);memcpy(data+i,&unknown,4);changed=TRUE;break;}
      g_assert_cmpuint(length,<=,size-i-8);if(!type)break;i+=8+length;
    }
  g_assert_true(changed);input=g_memory_input_stream_new_from_data(data,size,NULL);
  image=xcf_load_stream(gimp,input,NULL,NULL,&error);g_assert_no_error(error);g_assert_nonnull(image);g_object_unref(input);
  expected=gimp_painter_provenance_ref_bytes (G_OBJECT (image), GIMP_PAINTER_PROVENANCE_PROPERTIES);
  copy=gimp_image_duplicate(image);g_assert_nonnull(copy);
  { GBytes *actual = gimp_painter_provenance_ref_bytes (G_OBJECT (copy), GIMP_PAINTER_PROVENANCE_PROPERTIES);
    g_assert_true (g_bytes_equal (expected, actual)); g_bytes_unref (actual); }
  save(copy,file);reopened=load(file);
  {GVariant *record=parasite_capsule(gimp_image_parasite_find(reopened,"gimp-painter-image"));
   GVariant *raw=g_variant_lookup_value(record,"original-properties",G_VARIANT_TYPE_BYTESTRING);gsize actual_size,wanted_size;
   const void *actual=g_variant_get_fixed_array(raw,&actual_size,1),*wanted=g_bytes_get_data(expected,&wanted_size);
   g_assert_cmpmem(actual,actual_size,wanted,wanted_size);g_variant_unref(raw);g_variant_unref(record);}
  g_object_unref(reopened);g_object_unref(copy);g_object_unref(image);g_bytes_unref(expected);
  g_free(data);g_free(filename);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
static void missing_clone_id_and_pending_name (void)
{
  GimpImage *image=fixture("legacy-runtime/clone-normal-in-group.xcf"),*copy;GFile *file=temporary_file();
  GimpCloneLayer *clone;GVariant *record,*id;guint32 size;const guint8 *capsule_bytes;gsize length,found=G_MAXSIZE;gchar *data;
  save(image,file);copy=load(file);clone=GIMP_CLONE_LAYER(layer(copy,"clone"));record=capsule(GIMP_ITEM(clone));
  id=g_variant_lookup_value(record,"source-id",G_VARIANT_TYPE_UINT32);g_assert_nonnull(id);
  capsule_bytes=gimp_parasite_get_data(gimp_item_parasite_find(GIMP_ITEM(clone),"gimp-painter-item"),&size);
  g_assert_true(g_file_load_contents(file,NULL,&data,&length,NULL,NULL));
  for(gsize i=0;i+size<=length;++i)if(!memcmp(data+i,capsule_bytes,size)){found=i;break;}
  g_assert_cmpuint(found,!=,G_MAXSIZE);
  {const guint8 *base=g_variant_get_data(record),*field=g_variant_get_data(id);gsize offset=field-base;guint32 missing=GUINT32_TO_LE(0xffffffff);
   g_assert_cmpuint(offset+4,<=,g_variant_get_size(record));memcpy(data+found+12+offset,&missing,4);}
  g_assert_true(g_file_replace_contents(file,data,length,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,NULL));
  g_free(data);g_variant_unref(id);g_variant_unref(record);g_object_unref(copy);copy=load(file);
  clone=GIMP_CLONE_LAYER(layer(copy,"clone"));g_assert_nonnull(layer(copy,"source child"));g_assert_null(gimp_clone_layer_get_source(clone));
  for(guint repeat=0;repeat<2;++repeat)
    {
      GimpCloneLayerReference *reference=gimp_clone_layer_dup_reference(clone,NULL);g_assert_nonnull(reference);
      g_assert_true(reference->source_expired);g_assert_false(reference->allow_name_lookup);g_assert_cmpstr(reference->source_name,==,"source child");
      gimp_clone_layer_reference_free(reference);pixel(GIMP_LAYER(clone),191,32,64,255);
      save(copy,file);g_object_unref(copy);copy=load(file);clone=GIMP_CLONE_LAYER(layer(copy,"clone"));g_assert_null(gimp_clone_layer_get_source(clone));
      {GVariant *saved=capsule(GIMP_ITEM(clone));guint32 unresolved,active;
       g_assert_true(g_variant_lookup(saved,"unresolved-source-id","u",&unresolved));g_assert_cmpuint(unresolved,==,0xffffffff);
       g_assert_true(g_variant_lookup(saved,"source-id","u",&active));g_assert_cmpuint(active,==,0);g_variant_unref(saved);}
    }
  gimp_clone_layer_set_source_by_name(clone,"unresolved pending source");g_assert_null(gimp_clone_layer_get_source(clone));
  save(copy,file);g_object_unref(copy);copy=load(file);clone=GIMP_CLONE_LAYER(layer(copy,"clone"));
  {GimpCloneLayerReference *reference=gimp_clone_layer_dup_reference(clone,NULL);g_assert_null(reference->source);
   g_assert_cmpstr(reference->pending_name,==,"unresolved pending source");g_assert_true(reference->allow_name_lookup);gimp_clone_layer_reference_free(reference);}
  g_object_unref(copy);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
static void assert_active_argument_type (GimpItem *item, const gchar *expected)
{
  GVariant *record=capsule(item);gboolean present=FALSE;
  g_assert_true(g_variant_lookup(record,"has-arguments","b",&present));g_assert_cmpint(present,==,expected!=NULL);
  if(expected)
    {
      GVariant *args=g_variant_lookup_value(record,"arguments",G_VARIANT_TYPE("a(sbv)")),*entry,*type;
      g_assert_nonnull(args);entry=g_variant_get_child_value(args,0);type=g_variant_get_child_value(entry,0);
      g_assert_cmpstr(g_variant_get_string(type,NULL),==,expected);
      g_variant_unref(type);g_variant_unref(entry);g_variant_unref(args);
    }
  g_variant_unref(record);
}
static void replace_uninterpreted_arguments (void)
{
  GimpImage *image=gimp_image_new(gimp,4,4,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR),*copy;
  GimpLayer *filter=gimp_filter_layer_new(image,4,4,"opaque filter",1,GIMP_LAYER_MODE_NORMAL);
  GFile *file=temporary_file();GValue value=G_VALUE_INIT;GimpValueArray *args=gimp_value_array_new(1);
  GBytes *definition=g_bytes_new_static("RAW_ORIGIN",10);GVariant *record,*arguments,*entry,*type;
  const guint8 *capsule_bytes;guint32 size;gchar *data;gsize length,found=G_MAXSIZE;
  g_value_init(&value,G_TYPE_INT);g_value_set_int(&value,17);gimp_value_array_append(args,&value);g_value_unset(&value);
  gimp_image_add_layer(image,filter,NULL,0,FALSE);
  g_assert_true(gimp_filter_layer_set_definition(GIMP_FILTER_LAYER(filter),"unavailable-opaque",definition,args,NULL));
  {const guint8 cache[]={31,47,93,255};gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(0,0,1,1),0,babl_format("R'G'B'A u8"),cache,GEGL_AUTO_ROWSTRIDE);}
  gimp_filter_layer_mark_as_loaded(GIMP_FILTER_LAYER(filter));save(image,file);copy=load(file);filter=layer(copy,"opaque filter");
  record=capsule(GIMP_ITEM(filter));arguments=g_variant_lookup_value(record,"arguments",G_VARIANT_TYPE("a(sbv)"));
  entry=g_variant_get_child_value(arguments,0);type=g_variant_get_child_value(entry,0);g_assert_cmpstr(g_variant_get_string(type,NULL),==,"gint");
  capsule_bytes=gimp_parasite_get_data(gimp_item_parasite_find(GIMP_ITEM(filter),"gimp-painter-item"),&size);
  g_assert_true(g_file_load_contents(file,NULL,&data,&length,NULL,NULL));
  for(gsize i=0;i+size<=length;++i)if(!memcmp(data+i,capsule_bytes,size)){found=i;break;}
  g_assert_cmpuint(found,!=,G_MAXSIZE);
  {const guint8 *base=g_variant_get_data(record),*field=g_variant_get_data(type);gsize offset=field-base;
   g_assert_cmpuint(offset+5,<=,g_variant_get_size(record));memcpy(data+found+12+offset,"xxxx",4);}
  g_assert_true(g_file_replace_contents(file,data,length,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,NULL));g_free(data);
  g_variant_unref(type);g_variant_unref(entry);g_variant_unref(arguments);g_variant_unref(record);g_object_unref(copy);copy=load(file);
  filter=layer(copy,"opaque filter");g_assert_null(gimp_filter_layer_snapshot_arguments(GIMP_FILTER_LAYER(filter)));
  /* Dependency topology may change cache state but does not replace a definition. */
  {guint64 revision=gimp_filter_layer_get_definition_revision(GIMP_FILTER_LAYER(filter));
   GimpLayer *lower=gimp_layer_new(copy,4,4,babl_format("R'G'B'A u8"),"lower",1,GIMP_LAYER_MODE_NORMAL);
   gimp_image_add_layer(copy,lower,NULL,1,FALSE);g_assert_cmpuint(gimp_filter_layer_get_definition_revision(GIMP_FILTER_LAYER(filter)),==,revision);}
  save(copy,file);g_object_unref(copy);copy=load(file);filter=layer(copy,"opaque filter");
  record=capsule(GIMP_ITEM(filter));{gboolean present=FALSE;g_assert_true(g_variant_lookup(record,"has-arguments","b",&present));g_assert_true(present);}g_variant_unref(record);
  {GimpLayer *duplicate=GIMP_LAYER(gimp_item_duplicate(GIMP_ITEM(filter),GIMP_TYPE_FILTER_LAYER));
   GBytes *first=gimp_filter_layer_ref_opaque_arguments(GIMP_FILTER_LAYER(filter));
   GBytes *second=gimp_filter_layer_ref_opaque_arguments(GIMP_FILTER_LAYER(duplicate));
   g_assert_nonnull(first);g_assert_nonnull(second);g_assert_true(g_bytes_equal(first,second));g_bytes_unref(first);g_bytes_unref(second);
   gimp_object_set_name(GIMP_OBJECT(duplicate),"unreadable duplicate");gimp_image_add_layer(copy,duplicate,NULL,0,FALSE);}
  /* Duplication alone must preserve the active opaque model, not only archive it. */
  save(copy,file);g_object_unref(copy);copy=load(file);filter=layer(copy,"opaque filter");
  assert_active_argument_type(GIMP_ITEM(filter),"xxxx");
  assert_active_argument_type(GIMP_ITEM(layer(copy,"unreadable duplicate")),"xxxx");
  pixel(filter,31,47,93,255);pixel(layer(copy,"unreadable duplicate"),31,47,93,255);
  /* Explicit clear is undoable. Each state is saved and reopened independently
   * while the original image retains its Undo/Redo history. */
  gimp_image_undo_free(copy);
  g_assert_true(gimp_filter_layer_edit_definition(GIMP_FILTER_LAYER(filter),"unavailable-opaque",definition,NULL,NULL));
  save(copy,file);
  {GimpImage *cleared=load(file);assert_active_argument_type(GIMP_ITEM(layer(cleared,"opaque filter")),NULL);
   assert_active_argument_type(GIMP_ITEM(layer(cleared,"unreadable duplicate")),"xxxx");g_object_unref(cleared);}
  g_assert_true(gimp_image_undo(copy));save(copy,file);
  {GimpImage *undone=load(file);assert_active_argument_type(GIMP_ITEM(layer(undone,"opaque filter")),"xxxx");
   pixel(layer(undone,"opaque filter"),31,47,93,255);g_object_unref(undone);}
  g_assert_true(gimp_image_redo(copy));save(copy,file);g_object_unref(copy);copy=load(file);filter=layer(copy,"opaque filter");
  record=capsule(GIMP_ITEM(filter));
  {gboolean present=TRUE;g_assert_true(g_variant_lookup(record,"has-arguments","b",&present));g_assert_false(present);}
  arguments=g_variant_lookup_value(record,"original-argument-model",G_VARIANT_TYPE("a(sbv)"));g_assert_nonnull(arguments);
  entry=g_variant_get_child_value(arguments,0);type=g_variant_get_child_value(entry,0);g_assert_cmpstr(g_variant_get_string(type,NULL),==,"xxxx");
  g_variant_unref(type);g_variant_unref(entry);g_variant_unref(arguments);g_variant_unref(record);
  g_assert_null(gimp_filter_layer_snapshot_arguments(GIMP_FILTER_LAYER(filter)));
  assert_active_argument_type(GIMP_ITEM(layer(copy,"unreadable duplicate")),"xxxx");
  {GBytes *raw=gimp_filter_layer_ref_definition(GIMP_FILTER_LAYER(filter));g_assert_true(g_bytes_equal(raw,definition));g_bytes_unref(raw);}
  /* A completed supported edit overwrites the cache. Undo back to opaque
   * semantics must restore its committed pixels because it cannot recompute. */
  {
    GBytes *opaque=gimp_filter_layer_ref_opaque_arguments(GIMP_FILTER_LAYER(layer(copy,"unreadable duplicate")));
    GimpValueArray *edge=gimp_value_array_new(6);GValue option=G_VALUE_INIT;
    g_assert_nonnull(opaque);
    g_assert_true(gimp_filter_layer_set_definition_with_opaque_arguments(GIMP_FILTER_LAYER(filter),"unavailable-opaque",definition,opaque,NULL));
    g_bytes_unref(opaque);gimp_filter_layer_mark_as_loaded(GIMP_FILTER_LAYER(filter));gimp_image_undo_free(copy);
    for(guint i=0;i<6;++i)
      {g_value_init(&option,i==3?G_TYPE_DOUBLE:G_TYPE_INT);
       if(i==3)g_value_set_double(&option,2);else g_value_set_int(&option,i==4?1:0);
       gimp_value_array_append(edge,&option);g_value_unset(&option);}
    g_assert_true(gimp_filter_layer_edit_definition(GIMP_FILTER_LAYER(filter),"plug-in-edge",definition,edge,NULL));
    gimp_value_array_unref(edge);
    {gint64 deadline=g_get_monotonic_time()+3000000;
     while(gimp_filter_layer_get_state(GIMP_FILTER_LAYER(filter))!=GIMP_FILTER_LAYER_CLEAN&&g_get_monotonic_time()<deadline)
       {while(g_main_context_iteration(NULL,FALSE));g_usleep(1000);}
     g_assert_cmpint(gimp_filter_layer_get_state(GIMP_FILTER_LAYER(filter)),==,GIMP_FILTER_LAYER_CLEAN);}
    g_assert_cmpuint(gimp_filter_layer_get_run_count(GIMP_FILTER_LAYER(filter)),>,0);pixel(filter,0,0,0,0);
    g_assert_true(gimp_image_undo(copy));pixel(filter,31,47,93,255);
    save(copy,file);
    {GimpImage *undone=load(file);GimpLayer *restored=layer(undone,"opaque filter");
     assert_active_argument_type(GIMP_ITEM(restored),"xxxx");pixel(restored,31,47,93,255);g_object_unref(undone);}
  }
  g_value_set_int(gimp_value_array_index(args,0),55);
  g_assert_true(gimp_filter_layer_set_definition(GIMP_FILTER_LAYER(filter),"unavailable-opaque",definition,args,NULL));
  save(copy,file);g_object_unref(copy);copy=load(file);filter=layer(copy,"opaque filter");
  {GimpValueArray *restored=gimp_filter_layer_dup_args(GIMP_FILTER_LAYER(filter));g_assert_nonnull(restored);
   g_assert_cmpint(g_value_get_int(gimp_value_array_index(restored,0)),==,55);gimp_value_array_unref(restored);}
  g_object_unref(copy);g_object_unref(image);g_bytes_unref(definition);gimp_value_array_unref(args);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
static void opaque_argument_shapes (void)
{
  for(guint scene=0;scene<2;++scene)
    {
      GimpImage *image=gimp_image_new(gimp,2,2,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR),*copy;
      GimpLayer *filter=gimp_filter_layer_new(image,2,2,"opaque shape",1,GIMP_LAYER_MODE_NORMAL);
      GVariant *wrapper=g_variant_ref_sink(g_variant_new_maybe(G_VARIANT_TYPE_VARIANT,
                          scene?g_variant_new_variant(g_variant_new_string("retained opaque value")):NULL));
      GBytes *opaque;GFile *file=temporary_file();
#if G_BYTE_ORDER == G_BIG_ENDIAN
      {GVariant *swapped=g_variant_byteswap(wrapper);g_variant_unref(wrapper);wrapper=swapped;}
#endif
      opaque=g_variant_get_data_as_bytes(wrapper);g_variant_unref(wrapper);
      gimp_image_add_layer(image,filter,NULL,0,FALSE);
      g_assert_true(gimp_filter_layer_set_definition_with_opaque_arguments(GIMP_FILTER_LAYER(filter),"unavailable-shape",NULL,opaque,NULL));
      gimp_filter_layer_mark_as_loaded(GIMP_FILTER_LAYER(filter));
      for(guint repeat=0;repeat<2;++repeat)
        {
          GBytes *restored;GVariant *record,*model;gboolean present=FALSE;
          save(image,file);copy=load(file);filter=layer(copy,"opaque shape");
          restored=gimp_filter_layer_ref_opaque_arguments(GIMP_FILTER_LAYER(filter));g_assert_nonnull(restored);
          g_assert_true(g_bytes_equal(restored,opaque));g_bytes_unref(restored);
          record=capsule(GIMP_ITEM(filter));g_assert_true(g_variant_lookup(record,"has-arguments","b",&present));g_assert_true(present);
          model=g_variant_lookup_value(record,"arguments",NULL);
          if(scene){g_assert_nonnull(model);g_assert_cmpstr(g_variant_get_string(model,NULL),==,"retained opaque value");g_variant_unref(model);}
          else g_assert_null(model);
          g_variant_unref(record);g_assert_cmpuint(gimp_filter_layer_get_run_count(GIMP_FILTER_LAYER(filter)),==,0);
          g_object_unref(image);image=copy;
        }
      /* Invalid caller-owned opaque bytes must fail before destination replacement. */
      {GBytes *invalid=g_bytes_new_static("BAD",3);GError *error=NULL;
       GimpPlugInProcedure *proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save"));
       g_assert_true(gimp_filter_layer_set_definition_with_opaque_arguments(GIMP_FILTER_LAYER(filter),"unavailable-shape",NULL,invalid,NULL));
       sentinel_write(file);
       g_assert_cmpint(file_save(gimp,image,NULL,file,proc,GIMP_RUN_NONINTERACTIVE,FALSE,FALSE,FALSE,&error),==,GIMP_PDB_EXECUTION_ERROR);
       g_assert_nonnull(error);g_clear_error(&error);sentinel_check(file);g_bytes_unref(invalid);}
      g_bytes_unref(opaque);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
    }
}
static gboolean contains_bytes (const gchar *data, gsize size, const gchar *needle)
{
  gsize length=strlen(needle);
  for(gsize i=0;i+length<=size;++i)if(!memcmp(data+i,needle,length))return TRUE;
  return FALSE;
}
static GVariant *external_origins (GimpItem *item)
{
  GVariant *record=capsule(item),*origins=g_variant_lookup_value(record,"external-reference-origins",G_VARIANT_TYPE("aa{sv}"));
  g_assert_nonnull(origins);g_variant_unref(record);return origins;
}
static void external_reference_scene (guint scene)
{
  GimpImage *external=gimp_image_new(gimp,2,2,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  GimpImage *image=gimp_image_new(gimp,2,2,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR),*copy;
  GimpLayer *source=gimp_layer_new(external,2,2,babl_format("R'G'B'A u8"),"shared source",1,GIMP_LAYER_MODE_NORMAL);
  GimpLayer *local=gimp_layer_new(image,2,2,babl_format("R'G'B'A u8"),"shared source",1,GIMP_LAYER_MODE_NORMAL);
  GimpLayer *clone,*filter;GFile *file=temporary_file(),*display_file;
  GimpFilterArgumentReference refs[4];GObject *targets[]={G_OBJECT(external),G_OBJECT(source),G_OBJECT(image),G_OBJECT(local)};
  GimpFilterArgumentSpec spec={0};GimpFilterArgumentsSnapshot *args;GError *error=NULL;
  GVariant *clone_origin,*filter_origin;gint64 external_id=gimp_image_get_id(external),source_id=gimp_item_get_id(GIMP_ITEM(source));
  const guint8 source_pixel[]={191,32,64,255},local_pixel[]={9,8,7,255},filter_pixel[]={17,18,19,255};
  display_file=g_file_new_for_uri(scene ? "https://example.invalid/?access_token=unit-secret" : "https://unit-user:unit-secret@example.invalid/private/source.xcf");
  gimp_image_set_file(external,display_file);g_object_unref(display_file); /* no file/network lookup */
  gimp_image_add_layer(external,source,NULL,0,FALSE);gimp_image_add_layer(image,local,NULL,0,FALSE);
  gimp_item_set_tattoo(GIMP_ITEM(local),(guint32)source_id); /* diagnostic-ID collision trap */
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(0,0,1,1),0,babl_format("R'G'B'A u8"),source_pixel,GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(local)),GEGL_RECTANGLE(0,0,1,1),0,babl_format("R'G'B'A u8"),local_pixel,GEGL_AUTO_ROWSTRIDE);
  clone=gimp_clone_layer_new(image,source,2,2,"external clone",1,GIMP_LAYER_MODE_NORMAL);gimp_image_add_layer(image,clone,NULL,0,FALSE);
  filter=gimp_filter_layer_new(image,2,2,"external filter",1,GIMP_LAYER_MODE_NORMAL);gimp_image_add_layer(image,filter,NULL,0,FALSE);
  refs[0]=(GimpFilterArgumentReference){GIMP_TYPE_IMAGE,external_id,TRUE,FALSE};
  refs[1]=(GimpFilterArgumentReference){GIMP_TYPE_LAYER,source_id,TRUE,FALSE};
  refs[2]=(GimpFilterArgumentReference){GIMP_TYPE_IMAGE,gimp_image_get_id(image),TRUE,FALSE};
  refs[3]=(GimpFilterArgumentReference){GIMP_TYPE_LAYER,gimp_item_get_id(GIMP_ITEM(local)),TRUE,FALSE};
  spec.value_type=GIMP_TYPE_CORE_OBJECT_ARRAY;spec.n_references=4;spec.references=refs;spec.targets=targets;
  args=gimp_filter_arguments_snapshot_import(1,&spec,&error);g_assert_no_error(error);g_assert_nonnull(args);
  g_assert_true(gimp_filter_layer_set_definition_with_snapshot(GIMP_FILTER_LAYER(filter),"unavailable-external",NULL,args,&error));g_assert_no_error(error);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(0,0,1,1),0,babl_format("R'G'B'A u8"),filter_pixel,GEGL_AUTO_ROWSTRIDE);
  gimp_filter_layer_mark_as_loaded(GIMP_FILTER_LAYER(filter));
  save(image,file);
  /* Serializing an unresolved-on-reload descriptor must not change live links. */
  g_assert_true(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(clone))==source);pixel(clone,191,32,64,255);
  {GimpFilterArgumentsSnapshot *live=gimp_filter_layer_snapshot_arguments(GIMP_FILTER_LAYER(filter));GimpFilterArgumentReference ref;
   g_assert_true(gimp_filter_arguments_snapshot_reference(live,0,0,&ref));g_assert_false(ref.expired);g_assert_cmpint(ref.id,==,external_id);
   gimp_filter_arguments_snapshot_free(live);}
  copy=load(file);clone_origin=external_origins(GIMP_ITEM(layer(copy,"external clone")));filter_origin=external_origins(GIMP_ITEM(layer(copy,"external filter")));
  g_assert_cmpuint(g_variant_n_children(clone_origin),==,1);g_assert_cmpuint(g_variant_n_children(filter_origin),==,2);
  for(guint repeat=0;repeat<3;++repeat)
    {
      GimpLayer *restored_clone=layer(copy,"external clone"),*restored_filter=layer(copy,"external filter");
      GimpCloneLayerReference *reference=gimp_clone_layer_dup_reference(GIMP_CLONE_LAYER(restored_clone),NULL);
      GimpFilterArgumentsSnapshot *model=gimp_filter_layer_snapshot_arguments(GIMP_FILTER_LAYER(restored_filter));
      GimpFilterArgumentReference ref;GVariant *origins;gchar *bytes;gsize size;
      g_assert_null(reference->source);g_assert_true(reference->source_expired);g_assert_false(reference->allow_name_lookup);
      g_assert_cmpstr(reference->source_name,==,"shared source");gimp_clone_layer_reference_free(reference);
      g_assert_null(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(restored_clone)));pixel(restored_clone,191,32,64,255);pixel(restored_filter,17,18,19,255);
      g_assert_nonnull(model);g_assert_true(gimp_filter_arguments_snapshot_reference(model,0,0,&ref));g_assert_true(ref.expired);g_assert_cmpint(ref.id,==,external_id);
      g_assert_true(gimp_filter_arguments_snapshot_reference(model,0,1,&ref));g_assert_true(ref.expired);g_assert_cmpint(ref.id,==,source_id);
      g_assert_true(gimp_filter_arguments_snapshot_reference(model,0,2,&ref));g_assert_false(ref.expired);g_assert_cmpint(ref.id,==,gimp_image_get_id(copy));
      g_assert_true(gimp_filter_arguments_snapshot_reference(model,0,3,&ref));g_assert_false(ref.expired);g_assert_cmpint(ref.id,==,gimp_item_get_id(GIMP_ITEM(layer(copy,"shared source"))));
      gimp_filter_arguments_snapshot_free(model);
      origins=external_origins(GIMP_ITEM(restored_clone));g_assert_true(g_variant_equal(origins,clone_origin));g_variant_unref(origins);
      origins=external_origins(GIMP_ITEM(restored_filter));g_assert_true(g_variant_equal(origins,filter_origin));g_variant_unref(origins);
      g_assert_true(g_file_load_contents(file,NULL,&bytes,&size,NULL,NULL));
      g_assert_false(contains_bytes(bytes,size,"unit-secret"));g_assert_false(contains_bytes(bytes,size,"example.invalid"));g_assert_false(contains_bytes(bytes,size,"/private/"));g_free(bytes);
      if(!repeat)
        {
          /* Retained successful-save provenance also survives expiry in the
           * original live image, not just reopening an already saved file. */
          g_object_unref(external);external=NULL;g_assert_null(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(clone)));
          save(image,file);
        }
      else save(copy,file);
      g_object_unref(copy);copy=load(file);
    }
  g_variant_unref(clone_origin);g_variant_unref(filter_origin);gimp_filter_arguments_snapshot_free(args);
  g_object_unref(copy);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
static void external_references_remain_unresolved (void)
{ external_reference_scene(0);external_reference_scene(1); }
static void wire32 (guint8 *bytes, gsize offset, guint32 value)
{ value=GUINT32_TO_BE(value);memcpy(bytes+offset,&value,4); }
static GFile *synthetic_capsule_file (GBytes *capsule_bytes)
{
  guint8 original[124]={0};gsize blob_size,extra;const guint8 *blob=g_bytes_get_data(capsule_bytes,&blob_size);
  const gchar *name="gimp-painter-item";guint32 name_size=strlen(name)+1;guint8 *bytes;GFile *file=temporary_file();
  memcpy(original,"gimp xcf v010",13);wire32(original,14,1);wire32(original,18,1);wire32(original,26,GIMP_PRECISION_U8_NON_LINEAR);
  wire32(original,38,50);wire32(original,50,1);wire32(original,54,1);wire32(original,58,1);wire32(original,62,2);original[66]='L';
  wire32(original,76,84);wire32(original,84,1);wire32(original,88,1);wire32(original,92,4);wire32(original,96,104);
  wire32(original,104,1);wire32(original,108,1);wire32(original,112,120);original[120]=9;original[121]=8;original[122]=7;original[123]=255;
  extra=20+name_size+blob_size;bytes=g_malloc0(sizeof original+extra);memcpy(bytes,original,68);memcpy(bytes+68+extra,original+68,sizeof original-68);
  wire32(bytes,68,21);wire32(bytes,72,extra-8);wire32(bytes,76,name_size);memcpy(bytes+80,name,name_size);
  wire32(bytes,80+name_size,GIMP_PARASITE_PERSISTENT);wire32(bytes,84+name_size,blob_size);memcpy(bytes+88+name_size,blob,blob_size);
  wire32(bytes,76+extra,84+extra);wire32(bytes,96+extra,104+extra);wire32(bytes,112+extra,120+extra);
  g_assert_true(g_file_replace_contents(file,(gchar*)bytes,sizeof original+extra,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,NULL));g_free(bytes);return file;
}
static GBytes *schema_capsule (guint scene)
{
  const guint8 magic[12]={'G','P','X','C','F',0,0,0,1,0,0,0};
  GVariantBuilder builder;GVariant *value;GByteArray *wire=g_byte_array_new();
  g_variant_builder_init(&builder,G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&builder,"{sv}","version",g_variant_new_uint32(1));
  if(scene==0)g_variant_builder_add(&builder,"{sv}","version",g_variant_new_uint32(2));
  if(scene==3)g_variant_builder_add(&builder,"{sv}","kind",g_variant_new_uint32(77));
  else g_variant_builder_add(&builder,"{sv}","kind",g_variant_new_string(scene==2?"future-layer":scene==4?"clone":"ordinary"));
  if(scene==4)g_variant_builder_add(&builder,"{sv}","kind",g_variant_new_string("ordinary"));
  g_variant_builder_add(&builder,"{sv}","id",g_variant_new_uint32(2));
  g_variant_builder_add(&builder,"{sv}","future-field",g_variant_new_uint32(11));
  if(scene==1)g_variant_builder_add(&builder,"{sv}","future-field",g_variant_new_uint32(22));
  value=g_variant_ref_sink(g_variant_builder_end(&builder));g_assert_true(g_variant_is_normal_form(value));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  {GVariant *swapped=g_variant_byteswap(value);g_variant_unref(value);value=swapped;}
#endif
  g_byte_array_append(wire,magic,sizeof magic);g_byte_array_append(wire,g_variant_get_data(value),g_variant_get_size(value));g_variant_unref(value);
  return g_byte_array_free_to_bytes(wire);
}
static void ambiguous_and_unknown_capsule_schema (void)
{
  for(guint scene=0;scene<5;++scene)
    {
      GBytes *expected=schema_capsule(scene);GFile *input=synthetic_capsule_file(expected),*output=temporary_file();
      GimpImage *image=load(input),*copy;GimpLayer *item=layer(image,"L");
      const GimpParasite *parasite;const void *actual,*wanted;guint32 actual_size;gsize wanted_size;GError *error=NULL;
      GimpPlugInProcedure *proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save"));
      g_assert_false(GIMP_IS_CLONE_LAYER(item));g_assert_false(GIMP_IS_FILTER_LAYER(item));pixel(item,9,8,7,255);
      parasite=gimp_item_parasite_find(GIMP_ITEM(item),"gimp-painter-item");actual=gimp_parasite_get_data(parasite,&actual_size);wanted=g_bytes_get_data(expected,&wanted_size);
      g_assert_cmpmem(actual,actual_size,wanted,wanted_size);
      if(scene==0||scene==1||scene==4)
        {
          sentinel_write(output);
          g_assert_cmpint(file_save(gimp,image,NULL,output,proc,GIMP_RUN_NONINTERACTIVE,FALSE,FALSE,FALSE,&error),==,GIMP_PDB_EXECUTION_ERROR);
          g_assert_nonnull(error);g_assert_nonnull(strstr(error->message,"Duplicate Painter metadata key"));g_clear_error(&error);sentinel_check(output);
        }
      else
        {
          gimp_object_set_name(GIMP_OBJECT(item),"opaque edit");gimp_layer_set_opacity(item,.5,FALSE);save(image,output);copy=load(output);item=layer(copy,"opaque edit");
          parasite=gimp_item_parasite_find(GIMP_ITEM(item),"gimp-painter-item");actual=gimp_parasite_get_data(parasite,&actual_size);
          g_assert_cmpmem(actual,actual_size,wanted,wanted_size);g_assert_cmpfloat(gimp_layer_get_opacity(item),==,.5);pixel(item,9,8,7,255);
          g_assert_nonnull(gimp_item_parasite_find(GIMP_ITEM(item),"gimp-painter-origin"));
          /* An opaque active kind must not silently swallow a new custom mode. */
          gimp_layer_set_mode(item,GIMP_LAYER_MODE_PAINTER_NORMAL,FALSE);sentinel_write(output);
          g_assert_cmpint(file_save(gimp,copy,NULL,output,proc,GIMP_RUN_NONINTERACTIVE,FALSE,FALSE,FALSE,&error),==,GIMP_PDB_EXECUTION_ERROR);
          g_assert_nonnull(error);g_clear_error(&error);sentinel_check(output);g_object_unref(copy);
        }
      g_object_unref(image);g_bytes_unref(expected);g_file_delete(input,NULL,NULL);g_file_delete(output,NULL,NULL);g_object_unref(input);g_object_unref(output);
    }
}
static void
compare_layer_cache (GimpLayer *first,
                     GimpLayer *second)
{
  const gint width = gimp_item_get_width (GIMP_ITEM (first));
  const gint height = gimp_item_get_height (GIMP_ITEM (first));
  const gsize size = (gsize) width * height * 4;
  guint8 *before = g_malloc (size);
  guint8 *after = g_malloc (size);

  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (second)), ==, width);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (second)), ==, height);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (first)),
                   GEGL_RECTANGLE (0, 0, width, height), 1,
                   babl_format ("R'G'B'A u8"), before, GEGL_AUTO_ROWSTRIDE,
                   GEGL_ABYSS_NONE);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (second)),
                   GEGL_RECTANGLE (0, 0, width, height), 1,
                   babl_format ("R'G'B'A u8"), after, GEGL_AUTO_ROWSTRIDE,
                   GEGL_ABYSS_NONE);
  g_assert_cmpmem (before, size, after, size);
  g_free (before);
  g_free (after);
}

static void
recovery_registered_save (void)
{
  const gchar *fixtures[] = {
    "legacy-runtime/clone-normal-in-group.xcf",
    "legacy-runtime/clone-group.xcf",
    "legacy-runtime/filter-edge.xcf"
  };

  for (guint scene = 0; scene < G_N_ELEMENTS (fixtures); ++scene)
    {
      GimpImage *image = fixture (fixtures[scene]);
      GFile *backup = temporary_file ();
      GFile *original = g_object_ref (gimp_image_get_file (image));
      GBytes *original_bytes = g_file_load_bytes (original, NULL, NULL, NULL);
      GError *error = NULL;
      GimpImage *copy;
      GList *layers = gimp_image_get_layer_list (image);
      GimpLayer *custom = NULL;
      GimpLayer *restored;
      gint dirty;
      gint64 dirty_time;

      for (GList *p = layers; p; p = p->next)
        if (scene == 2 ? GIMP_IS_FILTER_LAYER (p->data) : GIMP_IS_CLONE_LAYER (p->data))
          custom = p->data;
      g_list_free (layers);
      g_assert_nonnull (custom);
      gimp_layer_set_opacity (custom, .375, FALSE);
      gimp_image_dirty (image, GIMP_DIRTY_IMAGE);
      dirty = gimp_image_is_dirty (image);
      dirty_time = gimp_image_get_dirty_time (image);
      g_assert_cmpint (dirty, !=, 0);

      /* This is the same helper used by the fatal-error backup loop.  It must
       * invoke the actual registered three-argument procedure, not file_save.
       */
      g_assert_true (xcf_save_recovery_image (gimp, image, backup, &error));
      g_assert_no_error (error);
      g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
      g_assert_cmpint (gimp_image_get_dirty_time (image), ==, dirty_time);
      g_assert_true (gimp_image_get_file (image) == original);
      {
        GBytes *after = g_file_load_bytes (original, NULL, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (g_bytes_equal (original_bytes, after));
        g_bytes_unref (after);
      }
      copy = load (backup);
      restored = layer (copy, gimp_object_get_name (custom));
      g_assert_nonnull (restored);
      g_assert_cmpfloat (gimp_layer_get_opacity (restored), ==, .375);
      compare_layer_cache (custom, restored);
      compare_projection (image, copy);
      if (scene < 2)
        {
          g_assert_true (GIMP_IS_CLONE_LAYER (restored));
          g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (restored)) ==
                         layer (copy, scene ? "source group" : "source child"));
          pixel (restored, 191, 32, 64, 255);
        }
      else
        {
          GBytes *before, *after;
          GimpValueArray *args;
          gchar *procedure;
          g_assert_true (GIMP_IS_FILTER_LAYER (restored));
          procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (restored));
          g_assert_cmpstr (procedure, ==, "plug-in-edge");
          g_free (procedure);
          before = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (custom));
          after = gimp_filter_layer_ref_definition (GIMP_FILTER_LAYER (restored));
          g_assert_cmpuint (g_bytes_get_size (after), ==, 89);
          g_assert_true (g_bytes_equal (before, after));
          g_bytes_unref (before);
          g_bytes_unref (after);
          args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (restored));
          g_assert_nonnull (args);
          g_assert_cmpint (gimp_value_array_length (args), ==, 6);
          g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (args, 3)), ==, 2);
          gimp_value_array_unref (args);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (restored)), ==, 0);
          g_assert_cmpint (gimp_filter_layer_get_state (GIMP_FILTER_LAYER (restored)), ==,
                           GIMP_FILTER_LAYER_CLEAN);
        }
      g_object_unref (copy);
      g_bytes_unref (original_bytes);
      g_object_unref (original);
      g_object_unref (image);
      g_file_delete (backup, NULL, NULL);
      g_object_unref (backup);
    }
}

static void
recovery_failed_save (void)
{
  GimpImage *image = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *original = g_object_ref (gimp_image_get_file (image));
  GFile *backup = temporary_file ();
  GFile *invalid_child = g_file_get_child (backup, "cannot-save.xcf");
  GBytes *duplicate = schema_capsule (0);
  gsize size;
  const void *bytes = g_bytes_get_data (duplicate, &size);
  GimpParasite *parasite;
  GError *error = NULL;
  gint dirty;

  gimp_image_dirty (image, GIMP_DIRTY_IMAGE);
  dirty = gimp_image_is_dirty (image);
  sentinel_write (backup);
  /* Real destination-open failure through the registered procedure. */
  g_assert_false (xcf_save_recovery_image (gimp, image, invalid_child, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  sentinel_check (backup);
  g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
  g_assert_true (gimp_image_get_file (image) == original);

  /* Metadata refusal must also return failure with the original temporary
   * destination untouched, including the fatal caller's NULL-error path.
   */
  parasite = gimp_parasite_new ("gimp-painter-item", GIMP_PARASITE_PERSISTENT, size, bytes);
  gimp_item_parasite_attach (GIMP_ITEM (layer (image, "clone")), parasite, FALSE);
  gimp_parasite_free (parasite);
  g_assert_false (xcf_save_recovery_image (gimp, image, backup, &error));
  g_assert_nonnull (error);
  g_assert_nonnull (strstr (error->message, "Duplicate Painter metadata key"));
  g_clear_error (&error);
  g_assert_false (xcf_save_recovery_image (gimp, image, backup, NULL));
  sentinel_check (backup);
  g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
  g_assert_true (gimp_image_get_file (image) == original);
  g_bytes_unref (duplicate);
  g_object_unref (invalid_child);
  g_object_unref (original);
  g_object_unref (image);
  g_file_delete (backup, NULL, NULL);
  g_object_unref (backup);
}

static GBytes *
invalid_filter_capsule (GVariant *base,
                        guint     scene)
{
  static const guint8 magic[] = {'G','P','X','C','F',0,0,0,1,0,0,0};
  const gchar *fields[] = {"generation", "cache-generation", "cache-complete", "saved-state"};
  GVariantDict dict;
  GVariant *value;
  GByteArray *wire = g_byte_array_new ();

  g_variant_dict_init (&dict, base);
  g_variant_dict_insert (&dict, "generation", "t", G_GUINT64_CONSTANT (5));
  g_variant_dict_insert (&dict, "cache-generation", "t", G_GUINT64_CONSTANT (5));
  g_variant_dict_insert (&dict, "cache-complete", "b", TRUE);
  g_variant_dict_insert (&dict, "saved-state", "u", GIMP_FILTER_LAYER_CLEAN);
  if (scene < 8)
    {
      if (scene % 2) g_variant_dict_insert (&dict, fields[scene / 2], "s", "wrong type");
      else g_variant_dict_remove (&dict, fields[scene / 2]);
    }
  else if (scene == 8) g_variant_dict_insert (&dict, "saved-state", "u", GIMP_FILTER_LAYER_CLOSED + 1);
  else if (scene == 9) g_variant_dict_insert (&dict, "cache-generation", "t", G_GUINT64_CONSTANT (6));
  else if (scene == 10) g_variant_dict_insert (&dict, "generation", "t", G_MAXUINT64);
  else if (scene == 11) g_variant_dict_remove (&dict, "has-definition");
  else if (scene == 12) g_variant_dict_insert (&dict, "has-definition", "s", "wrong type");
  else if (scene == 13) g_variant_dict_remove (&dict, "has-arguments");
  else if (scene == 14) g_variant_dict_insert (&dict, "has-arguments", "s", "wrong type");
  else if (scene == 15) g_variant_dict_insert (&dict, "definition", "s", "wrong type");
  else if (scene == 16) g_variant_dict_remove (&dict, "definition");
  else if (scene == 17) g_variant_dict_insert (&dict, "procedure", "u", 77);
  else if (scene == 20) g_variant_dict_insert (&dict, "legacy-mode", "u", 9999);
  else if (scene == 21) g_variant_dict_insert (&dict, "legacy-mode", "s", "wrong type");
  else if (scene == 22) g_variant_dict_insert (&dict, "original-source-name", "u", 77);
  else if (scene >= 24)
    {
      g_variant_dict_insert (&dict, "generation", "t", (guint64) G_MAXINT64);
      g_variant_dict_insert (&dict, "cache-generation", "t", (guint64) G_MAXINT64 - (scene == 25));
      g_variant_dict_insert (&dict, "cache-complete", "b", scene != 26);
    }
  else
    {
      const guint8 bad_name[] = {'a', 0, 'b', 0};
      g_variant_dict_insert_value (&dict, scene == 23 ? "original-source-name" : "procedure",
                                    g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                                               bad_name, scene == 18 ? 1 : sizeof bad_name, 1));
    }
  value = g_variant_ref_sink (g_variant_dict_end (&dict));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  { GVariant *swapped = g_variant_byteswap (value); g_variant_unref (value); value = swapped; }
#endif
  g_byte_array_append (wire, magic, sizeof magic);
  g_byte_array_append (wire, g_variant_get_data (value), g_variant_get_size (value));
  g_variant_unref (value);
  return g_byte_array_free_to_bytes (wire);
}

static void
assert_opaque_capsule (GimpLayer *item,
                       GBytes    *expected)
{
  const GimpParasite *parasite;
  const void *actual, *wanted;
  guint32 actual_size;
  gsize wanted_size;

  g_assert_false (GIMP_IS_FILTER_LAYER (item));
  g_assert_false (GIMP_IS_CLONE_LAYER (item));
  parasite = gimp_item_parasite_find (GIMP_ITEM (item), "gimp-painter-item");
  g_assert_nonnull (parasite);
  actual = gimp_parasite_get_data (parasite, &actual_size);
  wanted = g_bytes_get_data (expected, &wanted_size);
  g_assert_cmpmem (actual, actual_size, wanted, wanted_size);
  pixel (item, 9, 8, 7, 255);
}

static void
malformed_filter_cache_stays_opaque (void)
{
  GimpImage *original = fixture ("legacy-runtime/filter-edge.xcf");
  GFile *seed_file = temporary_file ();
  GimpImage *seed;
  GimpLayer *filter = NULL;
  GVariant *base;
  GList *layers;

  save (original, seed_file);
  seed = load (seed_file);
  layers = gimp_image_get_layer_list (seed);
  for (GList *p = layers; p; p = p->next)
    if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_list_free (layers);
  g_assert_nonnull (filter);
  base = capsule (GIMP_ITEM (filter));

  for (guint scene = 0; scene < 24; ++scene)
    {
      GBytes *expected = invalid_filter_capsule (base, scene);
      GFile *input = synthetic_capsule_file (expected);
      GFile *output = temporary_file ();
      GimpImage *image = load (input);
      GimpLayer *item = layer (image, "L");
      GimpLayer *duplicate;
      GError *error = NULL;
      GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));

      g_test_message ("Malformed Filter scene %u remains an ordinary opaque proxy", scene);
      assert_opaque_capsule (item, expected);
      /* There is no executable Filter and no cache freshness to certify. */
      while (g_main_context_iteration (NULL, FALSE));
      assert_opaque_capsule (item, expected);
      duplicate = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (item), GIMP_TYPE_LAYER));
      gimp_object_set_name (GIMP_OBJECT (duplicate), "opaque duplicate");
      gimp_image_add_layer (image, duplicate, NULL, 0, FALSE);
      gimp_object_set_name (GIMP_OBJECT (item), "opaque edit");
      gimp_layer_set_opacity (item, .5, FALSE);
      for (guint repeat = 0; repeat < 2; ++repeat)
        {
          GimpImage *copy;
          save (image, output);
          copy = load (output);
          g_object_unref (image);
          image = copy;
          item = layer (image, "opaque edit");
          assert_opaque_capsule (item, expected);
          assert_opaque_capsule (layer (image, "opaque duplicate"), expected);
          g_assert_cmpfloat (gimp_layer_get_opacity (item), ==, .5);
          g_assert_nonnull (gimp_item_parasite_find (GIMP_ITEM (item), "gimp-painter-origin"));
        }
      /* A new custom mode must not overwrite the malformed active semantics. */
      gimp_layer_set_mode (item, GIMP_LAYER_MODE_PAINTER_NORMAL, FALSE);
      sentinel_write (output);
      g_assert_cmpint (file_save (gimp, image, NULL, output, proc, GIMP_RUN_NONINTERACTIVE,
                                  FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_EXECUTION_ERROR);
      g_assert_nonnull (error);
      g_clear_error (&error);
      sentinel_check (output);
      g_object_unref (image);
      g_bytes_unref (expected);
      g_file_delete (input, NULL, NULL);
      g_file_delete (output, NULL, NULL);
      g_object_unref (input);
      g_object_unref (output);
    }
  /* Valid boundary counters still load; only freshness relationships survive
   * normalization, not the persisted runtime token values themselves. */
  for (guint scene = 24; scene < 27; ++scene)
    {
      GBytes *bytes = invalid_filter_capsule (base, scene);
      GFile *file = synthetic_capsule_file (bytes);
      GimpImage *image = load (file);
      for (guint repeat = 0; repeat < 2; ++repeat)
        {
          GimpLayer *item = layer (image, "L");
          GimpFilterLayerSnapshot state;
          GimpImage *copy;
          g_assert_true (GIMP_IS_FILTER_LAYER (item));
          pixel (item, 9, 8, 7, 255);
          g_assert_true (gimp_filter_layer_get_snapshot_state (GIMP_FILTER_LAYER (item), &state));
          g_assert_cmpint (state.cache_complete && state.cache_generation == state.generation, ==, scene == 24);
          g_assert_cmpint (state.cache_complete, ==, scene != 26);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (item)), ==, 0);
          save (image, file);
          copy = load (file);
          g_object_unref (image);
          image = copy;
        }
      g_object_unref (image);
      g_bytes_unref (bytes);
      g_file_delete (file, NULL, NULL);
      g_object_unref (file);
    }
  g_variant_unref (base);
  g_object_unref (seed);
  g_object_unref (original);
  g_file_delete (seed_file, NULL, NULL);
  g_object_unref (seed_file);
}

static GBytes *
clone_schema_capsule (GVariant *base,
                      guint     scene)
{
  static const guint8 magic[] = {'G','P','X','C','F',0,0,0,1,0,0,0};
  const gchar *fields[] = {"source-id", "source-state", "source-expired", "allow-name-lookup"};
  GVariantDict dict;
  GVariant *value;
  GByteArray *wire = g_byte_array_new ();
  const guint8 pending[] = {'p', 0};
  const guint8 bad_name[] = {'a', 0, 'b', 0};

  g_variant_dict_init (&dict, base);
  if (scene < 8)
    {
      if (scene % 2) g_variant_dict_insert (&dict, fields[scene / 2], "s", "wrong type");
      else g_variant_dict_remove (&dict, fields[scene / 2]);
    }
  else if (scene == 8) g_variant_dict_insert (&dict, "source-state", "u", GIMP_CLONE_SOURCE_EXPIRED + 1);
  else if (scene == 9) g_variant_dict_insert (&dict, "source-id", "u", 0);
  else if (scene == 10) g_variant_dict_insert (&dict, "source-expired", "b", TRUE);
  else if (scene == 11) g_variant_dict_insert (&dict, "source-state", "u", GIMP_CLONE_SOURCE_NONE);
  else if (scene == 12 || scene == 13)
    g_variant_dict_insert (&dict, scene == 12 ? "pending-name" : "source-name", "u", 77);
  else if (scene == 14 || scene == 15)
    g_variant_dict_insert_value (&dict, scene == 14 ? "pending-name" : "source-name",
                                  g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, bad_name,
                                                             scene == 14 ? 1 : sizeof bad_name, 1));
  else if (scene == 16)
    g_variant_dict_insert_value (&dict, "pending-name",
                                  g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, pending, sizeof pending, 1));
  else if (scene == 17)
    {
      g_variant_dict_insert (&dict, "source-id", "u", 0);
      g_variant_dict_insert (&dict, "source-state", "u", GIMP_CLONE_SOURCE_EXPIRED);
    }
  else
    {
      g_variant_dict_remove (&dict, "source-name");
      if (scene > 18)
        {
          g_variant_dict_insert (&dict, "source-id", "u", 0);
          g_variant_dict_insert (&dict, "source-state", "u", scene == 20 ? GIMP_CLONE_SOURCE_NONE : GIMP_CLONE_SOURCE_PENDING);
          g_variant_dict_insert (&dict, "source-expired", "b", scene == 19);
          g_variant_dict_insert (&dict, "allow-name-lookup", "b", FALSE);
          if (scene != 20)
            g_variant_dict_insert_value (&dict, "pending-name",
                                          g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                                                     scene == 21 ? pending + 1 : pending,
                                                                     scene == 21 ? 1 : sizeof pending, 1));
        }
    }
  value = g_variant_ref_sink (g_variant_dict_end (&dict));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  { GVariant *swapped = g_variant_byteswap (value); g_variant_unref (value); value = swapped; }
#endif
  g_byte_array_append (wire, magic, sizeof magic);
  g_byte_array_append (wire, g_variant_get_data (value), g_variant_get_size (value));
  g_variant_unref (value);
  return g_byte_array_free_to_bytes (wire);
}

static void
malformed_clone_reference_stays_opaque (void)
{
  GimpImage *original = fixture ("legacy-runtime/clone-normal-in-group.xcf");
  GFile *seed_file = temporary_file ();
  GimpImage *seed;
  GVariant *base;

  save (original, seed_file);
  seed = load (seed_file);
  base = capsule (GIMP_ITEM (layer (seed, "clone")));
  for (guint scene = 0; scene < 22; ++scene)
    {
      GBytes *expected = clone_schema_capsule (base, scene);
      GFile *file = synthetic_capsule_file (expected);
      GimpImage *image = load (file);
      for (guint repeat = 0; repeat < 2; ++repeat)
        {
          GimpLayer *item = layer (image, "L");
          GimpImage *copy;
          if (scene < 18)
            assert_opaque_capsule (item, expected);
          else
            {
              GimpCloneLayerReference *reference;
              g_assert_true (GIMP_IS_CLONE_LAYER (item));
              reference = gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (item), NULL);
              g_assert_null (reference->source);
              /* The core snapshot represents an absent diagnostic name as
               * an empty string, while absence of a pending name stays NULL. */
              g_assert_cmpstr (reference->source_name, ==, "");
              g_assert_false (reference->allow_name_lookup);
              g_assert_cmpint (reference->source_expired, ==, scene == 18 || scene == 19);
              g_assert_cmpint (reference->state, ==, scene == 18 ? GIMP_CLONE_SOURCE_EXPIRED :
                               scene == 20 ? GIMP_CLONE_SOURCE_NONE : GIMP_CLONE_SOURCE_PENDING);
              g_assert_cmpstr (reference->pending_name, ==, scene == 19 ? "p" : scene == 21 ? "" : NULL);
              gimp_clone_layer_reference_free (reference);
              pixel (item, 9, 8, 7, 255);
            }
          gimp_layer_set_opacity (item, .5, FALSE);
          save (image, file);
          copy = load (file);
          g_object_unref (image);
          image = copy;
        }
      if (scene < 18)
        {
          GimpLayer *proxy = layer (image, "L");
          GimpLayer *duplicate = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (proxy), GIMP_TYPE_LAYER));
          GError *error = NULL;
          GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
          GimpImage *copy;
          gimp_object_set_name (GIMP_OBJECT (duplicate), "opaque duplicate");
          gimp_image_add_layer (image, duplicate, NULL, 0, FALSE);
          save (image, file);
          copy = load (file);
          assert_opaque_capsule (layer (copy, "L"), expected);
          assert_opaque_capsule (layer (copy, "opaque duplicate"), expected);
          gimp_layer_set_mode (proxy, GIMP_LAYER_MODE_PAINTER_NORMAL, FALSE);
          sentinel_write (file);
          g_assert_cmpint (file_save (gimp, image, NULL, file, proc, GIMP_RUN_NONINTERACTIVE,
                                      FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_EXECUTION_ERROR);
          g_assert_nonnull (error);
          g_clear_error (&error);
          sentinel_check (file);
          g_object_unref (copy);
        }
      g_object_unref (image);
      g_bytes_unref (expected);
      g_file_delete (file, NULL, NULL);
      g_object_unref (file);
    }
  g_variant_unref (base);
  g_object_unref (seed);
  g_object_unref (original);
  g_file_delete (seed_file, NULL, NULL);
  g_object_unref (seed_file);
}

#include "test-painter-xcf-fields.inc"
#include "test-painter-xcf-active.inc"
#include "test-painter-xcf-multipart.inc"
#include "test-painter-xcf-multipart-adversarial.inc"

int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-xcf-roundtrip/clones_edit_save_reopen", clones_edit_save_reopen);
  g_test_add_func ("/painter-xcf-roundtrip/filter_cache_and_arguments", filter_cache_and_arguments);
  g_test_add_func ("/painter-xcf-roundtrip/ordinary_exact_roundtrip", ordinary_exact_roundtrip);
  g_test_add_func ("/painter-xcf-roundtrip/duplicate_unknown_records", duplicate_unknown_records);
  g_test_add_func ("/painter-xcf-roundtrip/typed_nested_expired_arguments", typed_nested_expired_arguments);
  g_test_add_func ("/painter-xcf-roundtrip/newly_unknown_records_and_future_capsule", newly_unknown_records_and_future_capsule);
  g_test_add_func ("/painter-xcf-roundtrip/cancel_and_reentrant_edit", cancel_and_reentrant_edit);
  g_test_add_func ("/painter-xcf-roundtrip/injected_write_failures", injected_write_failures);
  g_test_add_func ("/painter-xcf-roundtrip/save_observer_ownership", save_observer_ownership);
  g_test_add_func ("/painter-xcf-roundtrip/low_level_reentrant_definitions", low_level_reentrant_definitions);
  g_test_add_func ("/painter-xcf-roundtrip/duplicate_image_unknown_origin", duplicate_image_unknown_origin);
  g_test_add_func ("/painter-xcf-roundtrip/missing_clone_id_and_pending_name", missing_clone_id_and_pending_name);
  g_test_add_func ("/painter-xcf-roundtrip/replace_uninterpreted_arguments", replace_uninterpreted_arguments);
  g_test_add_func ("/painter-xcf-roundtrip/opaque_argument_shapes", opaque_argument_shapes);
  g_test_add_func ("/painter-xcf-roundtrip/external_references_remain_unresolved", external_references_remain_unresolved);
  g_test_add_func ("/painter-xcf-roundtrip/ambiguous_and_unknown_capsule_schema", ambiguous_and_unknown_capsule_schema);
  g_test_add_func ("/painter-xcf-roundtrip/recovery_registered_save", recovery_registered_save);
  g_test_add_func ("/painter-xcf-roundtrip/recovery_failed_save", recovery_failed_save);
  g_test_add_func ("/painter-xcf-roundtrip/malformed_filter_cache_stays_opaque", malformed_filter_cache_stays_opaque);
  g_test_add_func ("/painter-xcf-roundtrip/malformed_clone_reference_stays_opaque", malformed_clone_reference_stays_opaque);
  g_test_add_func ("/painter-xcf-fields/all_owner_attributes", fields_all_owner_attributes);
  g_test_add_func ("/painter-xcf-fields/legacy_text_and_units", fields_legacy_text_and_units);
  g_test_add_func ("/painter-xcf-fields/profiles_palettes_metadata", fields_profiles_palettes_metadata);
  g_test_add_func ("/painter-xcf-fields/wrong_owner_capsules", fields_wrong_owner_capsules);
  g_test_add_func ("/painter-xcf-fields/retained_metadata_bound", fields_retained_metadata_bound);
  g_test_add_func ("/painter-xcf-active/running_save_close_reopen", filter_running_save_close_reopen);
  g_test_add_func ("/painter-xcf-active/importing_save_close_reopen", filter_importing_save_close_reopen);
  g_test_add_func ("/painter-xcf-fields/modern_effect_records", fields_modern_effect_records);
  g_test_add_func ("/painter-xcf-fields/unsupported_effect_refuses_save", fields_unsupported_effect_refuses_save);
  g_test_add_func ("/painter-xcf-fields/modern_effect_argument_families", fields_modern_effect_argument_families);
  g_test_add_func ("/painter-xcf-fields/clone_duplicate_name_identity", fields_clone_duplicate_name_identity);
  g_test_add_func ("/painter-xcf-fields/absent_native_properties", fields_absent_native_properties);
  g_test_add_func ("/painter-xcf-fields/native_precision_and_compression", fields_native_precision_and_compression);
  g_test_add_func ("/painter-xcf-multipart/native-small", multipart_native_small);
  g_test_add_func ("/painter-xcf-multipart/native-large", multipart_native_large);
  g_test_add_func ("/painter-xcf-multipart/native-cancellation", multipart_native_cancellation);
  g_test_add_func ("/painter-xcf-multipart/native-midprepare-cancel", multipart_native_midprepare_cancel);
  g_test_add_func ("/painter-xcf-multipart/native-disk-failure", multipart_native_disk_failure);
  g_test_add_func ("/painter-xcf-multipart/argument-snapshot-borrow", multipart_argument_snapshot_borrow);
  g_test_add_func ("/painter-xcf-multipart/native-owner-duplicate-edit", multipart_native_owner_duplicate_edit);
  g_test_add_func ("/painter-xcf-multipart/native-reordered", multipart_native_reordered);
  g_test_add_func ("/painter-xcf-multipart/native-grouped", multipart_native_grouped);
  g_test_add_func ("/painter-xcf-multipart/native-adversarial", multipart_native_adversarial);
  g_test_add_func ("/painter-xcf-multipart/native-opaque-capsules", multipart_native_opaque_capsules);
  g_test_add_func ("/painter-xcf-multipart/native-unclaimed-parasites", multipart_native_unclaimed_parasites);
  g_test_add_func ("/painter-xcf-multipart/native-inert-ownership-changes", multipart_native_inert_ownership_changes);
  g_test_add_func ("/painter-xcf-multipart/native-callback-argument-leases", multipart_native_callback_argument_leases);
  g_test_add_func ("/painter-xcf-multipart/native-stream-failures", multipart_native_stream_failures);
  g_test_add_func ("/painter-xcf-multipart/native-argument-budget", multipart_native_argument_budget);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
