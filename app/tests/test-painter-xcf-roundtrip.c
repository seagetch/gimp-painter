/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-new.h"
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

static GVariant *capsule (GimpItem *item)
{
  const GimpParasite *parasite = gimp_item_parasite_find (item, "gimp-painter-item");
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
  expected = g_bytes_ref (g_object_get_data (G_OBJECT (original),"gimp-painter-xcf-property-records"));
  copy = GIMP_LAYER (gimp_item_duplicate (original,GIMP_TYPE_CLONE_LAYER));
  g_assert_true (g_bytes_equal (expected,g_object_get_data (G_OBJECT (copy),"gimp-painter-xcf-property-records")));
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
  GBytes *properties;const guint8 *raw;
  const guint8 future[]={ 'G','P','X','C','F',0,0,0,2,0,0,0,0xab,0xcd,0xef };
  const guint8 unknown[]={0xf0,0x0d,0xba,0xbf,0,0,0,4,0,0,0,7};
  gimp_image_add_layer(image,item,NULL,0,FALSE);gimp_item_set_color_tag(GIMP_ITEM(item),GIMP_COLOR_TAG_GRAY,FALSE);
  save(image,file);g_object_unref(image);image=load(file);item=layer(image,"future layer");
  properties=g_object_get_data(G_OBJECT(item),"gimp-painter-xcf-property-records");raw=g_bytes_get_data(properties,&raw_length);
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
  GFile *file = temporary_file ();
  GValue values[16] = {G_VALUE_INIT}, nested_values[2] = {G_VALUE_INIT};
  GimpFilterArgumentSpec specs[16] = {{0}}, children[3] = {{0}};
  GimpFilterArgumentReference refs[3] = {{0}}, expired = {GIMP_TYPE_LAYER,70707,TRUE,TRUE};
  GObject *targets[3] = {G_OBJECT(image),G_OBJECT(source),NULL};
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
  for(guint i=0;i<16;++i)
    { specs[i].value_type=types[i]; if(i<8||i>=12) { g_value_init(&values[i],types[i]); specs[i].value=&values[i]; } }
  g_value_set_int(&values[0],-123); memcpy(&nan_value,&nan_bits,8); g_value_set_double(&values[1],nan_value);
  g_value_set_string(&values[2],"opaque\xff string"); specs[3].is_null=TRUE;
  g_value_set_boxed(&values[4],strings); gimp_value_set_int32_array(&values[5],ints,3);
  gimp_value_set_double_array(&values[6],doubles,3); g_value_set_boxed(&values[7],bytes);
  refs[0]=(GimpFilterArgumentReference){GIMP_TYPE_IMAGE,gimp_image_get_id(image),TRUE,FALSE};
  refs[1]=(GimpFilterArgumentReference){GIMP_TYPE_LAYER,gimp_item_get_id(GIMP_ITEM(source)),TRUE,FALSE};
  refs[2]=(GimpFilterArgumentReference){GIMP_TYPE_LAYER,919191,TRUE,TRUE};
  specs[8].n_references=3;specs[8].references=refs;specs[8].targets=targets;
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
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,0,&reference));g_assert_cmpint(reference.id,==,gimp_image_get_id(copy));g_assert_false(reference.expired);
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,1,&reference));g_assert_cmpint(reference.id,==,gimp_item_get_id(GIMP_ITEM(layer(copy,"typed source"))));
      g_assert_true(gimp_filter_arguments_snapshot_reference(restored,8,2,&reference));g_assert_true(reference.expired);g_assert_cmpint(reference.id,==,919191);
      g_assert_true(gimp_filter_arguments_snapshot_is_null(restored,9));g_assert_false(gimp_filter_arguments_snapshot_is_null(restored,10));
      g_assert_cmpuint(gimp_filter_arguments_snapshot_reference_count(restored,10),==,0);
      nested=gimp_filter_arguments_snapshot_nested(restored,11);g_assert_nonnull(nested);
      g_assert_cmpuint(gimp_filter_arguments_snapshot_count(nested),==,3);
      g_assert_true(gimp_filter_arguments_snapshot_reference(nested,2,0,&reference));g_assert_true(reference.expired);g_assert_cmpint(reference.id,==,70707);
      gimp_filter_arguments_snapshot_free(nested);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,12,&value));g_assert_true(g_variant_equal(g_value_get_variant(&value),g_value_get_variant(&values[12])));g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_value(restored,13,&value));float_value=g_value_get_float(&value);{guint32 bits;memcpy(&bits,&float_value,4);g_assert_cmpuint(bits,==,float_bits);}g_value_unset(&value);
      g_assert_true(gimp_filter_arguments_snapshot_is_null(restored,14));g_assert_false(gimp_filter_arguments_snapshot_is_null(restored,15));
      definition=gimp_filter_layer_ref_definition(GIMP_FILTER_LAYER(filter));g_assert_true(g_bytes_equal(raw,definition));g_bytes_unref(definition);
      gimp_filter_arguments_snapshot_free(restored);g_object_unref(image);image=copy;
    }
  for(guint i=0;i<16;++i)if(G_VALUE_TYPE(&values[i]))g_value_unset(&values[i]);
  g_value_unset(&nested_values[0]);g_value_unset(&nested_values[1]);gimp_filter_arguments_snapshot_free(args);
  g_bytes_unref(bytes);g_bytes_unref(raw);g_object_unref(image);g_file_delete(file,NULL,NULL);g_object_unref(file);
}

typedef struct { GObject parent; gint count, cancel_at; gboolean close_cancel, mutate, ended; gint mutation_kind; GimpLayer *layer; } SaveProgress;
typedef struct { GObjectClass parent; } SaveProgressClass;
static GimpProgress *save_progress_start (GimpProgress *progress, gboolean cancellable, const gchar *text)
{
  SaveProgress *self=(SaveProgress*)progress;
  g_assert_true(cancellable);if(self->cancel_at==0)gimp_progress_cancel(progress);return progress;
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
typedef struct { GFilterOutputStream parent; gsize limit,written; gboolean cancelled_close; } FailOutput;
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
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
