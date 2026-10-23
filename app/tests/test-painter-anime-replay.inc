/* SPDX-License-Identifier: GPL-3.0-or-later
 * Replay only the retained anime fixture's current-core edit/opacity history.
 * No old runtime, watercolor/filter fixture or GUI input is invoked. */
#include "core/gimpclonelayer.h"
#include "core/gimpdrawable-fill.h"
static void anime_replay_projection(GimpImage *image,const gchar *state)
{
 gchar *name=g_strdup_printf("anime-%s.projection.rgba",state);
 gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-composite/anime",name,NULL);
 gchar *expected=NULL;gsize size=0;guchar actual[96*80*4];GError *error=NULL;
 GimpLayer *clone=mode_named_layer(image,"mask"),*source=mode_named_layer(image,"paint source");
 gint64 until=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
 /* Match the bounded original capture's main-context observation interval. */
 while(g_get_monotonic_time()<until){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}
 g_assert_true(GIMP_IS_CLONE_LAYER(clone));
 g_assert_true(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(clone))==source);
 g_assert_cmpint(gimp_layer_get_mode(clone),==,GIMP_LAYER_MODE_PAINTER_DST_IN);
 g_assert_cmpstr(gimp_object_get_name(gimp_item_get_parent(GIMP_ITEM(clone))),==,"New group");
 g_assert_true(g_file_get_contents(path,&expected,&size,&error));g_assert_no_error(error);
 g_assert_cmpuint(size,==,sizeof actual);gimp_pickable_flush(GIMP_PICKABLE(image));
 gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),GEGL_RECTANGLE(0,0,96,80),1,
                 babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 for(guint p=0;p<sizeof actual;++p)if(actual[p]!=(guchar)expected[p])
  g_error("anime replay %s component%u expected%u got%u",state,p,(guchar)expected[p],actual[p]);
 g_test_message("anime replay %s:7680 exact RGBA pixels and Clone source/type/hierarchy",state);
 g_free(expected);g_free(path);g_free(name);
}
static void anime_current_edit_replay(void)
{
 GimpPlugInProcedure *proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-load"));
 gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-composite/anime/anime-initial.xcf",NULL);
 GFile *file=g_file_new_for_path(path);GError *error=NULL;GimpPDBStatusType status;
 GimpImage *image=file_open_image(gimp,gimp_get_user_context(gimp),NULL,file,0,0,FALSE,proc,GIMP_RUN_NONINTERACTIVE,&status,NULL,&error);
 GimpLayer *source,*shadow;GeglColor *color;const guchar rgba[]={51,153,204,255};
 g_assert_no_error(error);g_assert_cmpint(status,==,GIMP_PDB_SUCCESS);g_assert_nonnull(image);
 source=mode_named_layer(image,"paint source");shadow=mode_named_layer(image,"shadow");
 g_assert_cmpint(gimp_item_get_width(GIMP_ITEM(source)),==,48);
 g_assert_cmpint(gimp_item_get_height(GIMP_ITEM(source)),==,40);
 g_assert_cmpint(gimp_layer_get_mode(shadow),==,GIMP_LAYER_MODE_PAINTER_MULTIPLY);
 g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(shadow),1,1e-7);
 anime_replay_projection(image,"initial-reopened");
 color=gegl_color_new(NULL);gegl_color_set_pixel(color,babl_format("R'G'B'A u8"),rgba);
 gimp_drawable_fill_buffer(GIMP_DRAWABLE(source),gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),color,NULL,0,0);
 g_object_unref(color);gimp_drawable_update(GIMP_DRAWABLE(source),0,0,48,40);
 anime_replay_projection(image,"recolored");
 gimp_image_undo_free(image);gimp_layer_set_opacity(shadow,.6,TRUE);
 g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(shadow),.6,1e-7);
 anime_replay_projection(image,"opacity");
 g_assert_true(gimp_image_undo(image));g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(shadow),1,1e-7);
 anime_replay_projection(image,"undo");
 g_assert_true(gimp_image_redo(image));g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(shadow),.6,1e-7);
 anime_replay_projection(image,"redo");
 g_object_unref(image);g_object_unref(file);g_free(path);
}
