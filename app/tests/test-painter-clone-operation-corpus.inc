/* SPDX-License-Identifier: GPL-3.0-or-later
 * Current-core replay of ONLY capture_clone(), retained old capture lines1151-1252.
 * No file load/save, Filter branch, old runtime or graphical interaction. */
#include "core/gimpdrawable-fill.h"
typedef struct {const gchar *id;GimpLayer *layer;} CloneCorpusItem;
static CloneCorpusItem corpus_items[10];static guint corpus_count;
static void corpus_watch(GimpLayer *layer,const gchar *id)
{
 g_assert_cmpuint(corpus_count,<,10);corpus_items[corpus_count]=(CloneCorpusItem){id,layer};
 g_object_add_weak_pointer(G_OBJECT(layer),(gpointer*)&corpus_items[corpus_count].layer);++corpus_count;
}
static GimpLayer *corpus_item(const gchar *id)
{
 if(!strcmp(id,"NULL")||!strcmp(id,"-"))return NULL;
 for(guint i=0;i<corpus_count;++i)if(!strcmp(corpus_items[i].id,id))return corpus_items[i].layer;
 g_error("unknown old corpus item %s",id);return NULL;
}
static void corpus_pixels(GimpPickable *pickable,gint width,gint height,const gchar *stage,const gchar *id)
{
 gchar *name=g_strdup_printf("%s.%s.pixels",stage,id);
 gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-layer-operations/clone",name,NULL);
 gchar *expected=NULL;gsize size=0;GError *error=NULL;guchar *actual=g_malloc(width*height*4);
 g_assert_true(g_file_get_contents(path,&expected,&size,&error));g_assert_no_error(error);g_assert_cmpuint(size,==,width*height*4);
 gimp_pickable_flush(pickable);
 gegl_buffer_get(gimp_pickable_get_buffer(pickable),GEGL_RECTANGLE(0,0,width,height),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 for(gsize i=0;i<size;++i)if(actual[i]!=(guchar)expected[i])g_error("corpus %s/%s component%zu expected%u got%u",stage,id,i,(guchar)expected[i],actual[i]);
 g_free(actual);g_free(expected);g_free(path);g_free(name);
}
static void corpus_snapshot(GimpImage *image,const gchar *stage)
{
 gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/tests/clone-operation-corpus/old-model.tsv",NULL),*text=NULL;gchar **rows;
 gint64 until=g_get_monotonic_time()+100000;guint count=0;
 while(g_get_monotonic_time()<until){g_main_context_iteration(NULL,FALSE);gimp_pickable_flush(GIMP_PICKABLE(image));g_usleep(1000);}
 g_assert_true(g_file_get_contents(path,&text,NULL,NULL));rows=g_strsplit(text,"\n",-1);
 for(guint i=0;rows[i];++i){
  gchar **f=g_strsplit(rows[i],"\t",-1);if(g_strv_length(f)!=11||strcmp(f[0],stage)){g_strfreev(f);continue;}
  GimpLayer *layer=corpus_item(f[1]);++count;g_assert_cmpint(layer!=NULL,==,atoi(f[2]));
  if(layer){GimpItem *item=GIMP_ITEM(layer);gint w=atoi(f[8]),h=atoi(f[9]);
   g_assert_cmpint(gimp_item_is_attached(item),==,atoi(f[3]));
   g_assert_true(gimp_item_get_parent(item)==GIMP_ITEM(corpus_item(f[4])));
   g_assert_cmpint(gimp_item_is_attached(item)?gimp_item_get_index(item):-1,==,atoi(f[5]));
   g_assert_cmpint(gimp_item_get_offset_x(item),==,atoi(f[6]));g_assert_cmpint(gimp_item_get_offset_y(item),==,atoi(f[7]));
   g_assert_cmpint(gimp_item_get_width(item),==,w);g_assert_cmpint(gimp_item_get_height(item),==,h);
   if(strcmp(f[10],"-")){g_assert_true(GIMP_IS_CLONE_LAYER(layer));g_assert_true(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(layer))==corpus_item(f[10]));}
   corpus_pixels(GIMP_PICKABLE(layer),w,h,stage,f[1]);
  }
  g_strfreev(f);
 }
 g_assert_cmpuint(count,==,corpus_count);corpus_pixels(GIMP_PICKABLE(image),96,80,stage,"projection");
 g_test_message("old Clone operation stage %s: %u object models/raw buffers and7680 projection pixels",stage,count);
 g_strfreev(rows);g_free(text);g_free(path);
}
static void corpus_fill(GimpLayer *layer,const guchar rgba[4])
{
 GeglColor *color=gegl_color_new(NULL);gegl_color_set_pixel(color,babl_format("R'G'B'A u8"),rgba);
 gimp_drawable_fill_buffer(GIMP_DRAWABLE(layer),gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),color,NULL,0,0);
 g_object_unref(color);gimp_drawable_update(GIMP_DRAWABLE(layer),0,0,24,20);
}
static GimpLayer *corpus_rect(GimpImage *image,GimpLayer *parent,const gchar *id,gint x,gint y,const guchar rgba[4])
{
 GimpLayer *layer=gimp_layer_new(image,24,20,babl_format("R'G'B'A u8"),id,1,GIMP_LAYER_MODE_PAINTER_NORMAL);
 g_assert_true(gimp_image_add_layer(image,layer,parent,0,FALSE));gimp_item_set_offset(GIMP_ITEM(layer),x,y);
 corpus_fill(layer,rgba);corpus_watch(layer,id);return layer;
}
static GimpLayer *corpus_clone(GimpImage *image,GimpLayer *parent,GimpLayer *source,const gchar *id)
{
 GimpLayer *layer=gimp_clone_layer_new(image,source,24,20,id,1,GIMP_LAYER_MODE_PAINTER_NORMAL);
 g_assert_true(gimp_image_add_layer(image,layer,parent,0,FALSE));corpus_watch(layer,id);return layer;
}
static void corpus_undo_redo(GimpImage *image,const gchar *stage)
{
 gchar *name=g_strdup_printf("%s-undo",stage);g_assert_true(gimp_image_undo(image));corpus_snapshot(image,name);g_free(name);
 name=g_strdup_printf("%s-redo",stage);g_assert_true(gimp_image_redo(image));corpus_snapshot(image,name);g_free(name);
}
static void clone_original_operation_corpus(void)
{
 const guchar red[]={204,51,102,255},blue[]={51,77,204,255},green[]={51,204,77,255};
 GimpImage *image=gimp_image_new(gimp,96,80,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
 GimpLayer *external,*group,*source,*clone,*ec,*direct,*copy,*copy_source;corpus_count=0;
 external=corpus_rect(image,NULL,"external",60,40,blue);group=gimp_group_layer_new(image);
 gimp_layer_set_mode(group,GIMP_LAYER_MODE_PAINTER_NORMAL,FALSE);gimp_object_set_name(GIMP_OBJECT(group),"group");
 g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));corpus_watch(group,"group");
 source=corpus_rect(image,group,"source",8,8,red);clone=corpus_clone(image,group,source,"clone");gimp_item_set_offset(GIMP_ITEM(clone),36,8);
 ec=corpus_clone(image,group,external,"external-clone");gimp_item_set_offset(GIMP_ITEM(ec),36,36);
 gimp_drawable_update(GIMP_DRAWABLE(source),0,0,24,20);gimp_drawable_update(GIMP_DRAWABLE(external),0,0,24,20);
 gimp_image_undo_free(image);corpus_snapshot(image,"baseline");
 gimp_item_translate(GIMP_ITEM(source),5,7,TRUE);corpus_snapshot(image,"move-source");corpus_undo_redo(image,"move-source");
 gimp_item_translate(GIMP_ITEM(clone),-3,4,TRUE);corpus_snapshot(image,"move-clone");corpus_undo_redo(image,"move-clone");
 g_assert_true(gimp_image_reorder_item(image,GIMP_ITEM(source),GIMP_ITEM(group),0,TRUE,"Move source in stack"));corpus_snapshot(image,"reorder-source");corpus_undo_redo(image,"reorder-source");
 direct=GIMP_LAYER(gimp_item_duplicate(GIMP_ITEM(clone),GIMP_TYPE_CLONE_LAYER));corpus_watch(direct,"direct-copy");gimp_object_set_name(GIMP_OBJECT(direct),"direct-copy");
 g_assert_true(gimp_image_add_layer(image,direct,NULL,0,TRUE));corpus_snapshot(image,"duplicate-clone");corpus_undo_redo(image,"duplicate-clone");
 copy=GIMP_LAYER(gimp_item_duplicate(GIMP_ITEM(group),GIMP_TYPE_GROUP_LAYER));corpus_watch(copy,"group-copy");gimp_object_set_name(GIMP_OBJECT(copy),"group-copy");
 {GimpContainer *children=gimp_viewable_get_children(GIMP_VIEWABLE(copy));
  for(gint i=0;i<gimp_container_get_n_children(children);++i){GimpLayer *layer=GIMP_LAYER(gimp_container_get_child_by_index(children,i));const gchar *name=gimp_object_get_name(layer);
   if(!strcmp(name,"source"))corpus_watch(layer,"copy-source");else if(!strcmp(name,"clone"))corpus_watch(layer,"copy-clone");else {g_assert_cmpstr(name,==,"external-clone");corpus_watch(layer,"copy-external-clone");}
  }}
 g_assert_true(gimp_image_add_layer(image,copy,NULL,0,TRUE));copy_source=corpus_item("copy-source");
 corpus_snapshot(image,"duplicate-group");corpus_undo_redo(image,"duplicate-group");
 corpus_fill(source,green);corpus_snapshot(image,"edit-original");corpus_fill(copy_source,blue);corpus_snapshot(image,"edit-copy");corpus_fill(external,green);corpus_snapshot(image,"edit-external");
 gimp_image_remove_layer(image,source,TRUE,NULL);corpus_snapshot(image,"delete-source");corpus_undo_redo(image,"delete-source");
 g_assert_true(gimp_image_undo(image));corpus_snapshot(image,"restore-source");corpus_fill(source,blue);corpus_snapshot(image,"edit-restored");
 gimp_image_remove_layer(image,clone,TRUE,NULL);corpus_snapshot(image,"delete-clone");corpus_undo_redo(image,"delete-clone");
 gimp_image_remove_layer(image,copy,TRUE,NULL);corpus_snapshot(image,"delete-group");corpus_undo_redo(image,"delete-group");
 g_object_unref(image);for(guint i=0;i<corpus_count;++i)g_assert_null(corpus_items[i].layer);
}
