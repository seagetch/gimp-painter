/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <glib/gstdio.h>
#include <cstring>
#include <string>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpcurve.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpdatafactory.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/gimpdeviceinfo.h"
#include "core/gimpcontext.h"
#include "core/gimp-contexts.h"
#include "menus/shortcuts-rc.h"
#include "widgets/gimpaction.h"
#include "core/gimptoolgroup.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimptoolpreset.h"
#include "core/gimppainterprofile.h"
#include "core/gimp-user-install.h"
#include "paint/gimppaintersmudge.h"
#include "tools/tools-types.h"
#include "tools/gimp-tools.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
#include "gimpcoreapp.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.h"
static Gimp*gimp;
static gchar*convert(const char*text,const char*kind) {GError*error=nullptr;auto*out=gimp_painter_profile_transform(text,kind,TRUE,&error);g_assert_no_error(error);g_assert_nonnull(out);return out;}
static void deserialize(GimpConfig*object,const char*text) {GError*error=nullptr;g_assert_true(gimp_config_deserialize_string(object,text,-1,nullptr,&error));g_assert_no_error(error);}
static void tool_order_and_groups () {
  gimp_set_focused_once(gimp);
  const char*legacy="(file-version 1)\n(GimpToolGroup \"custom\" (visible yes) (active-tool \"gimp-mypaint-tool\") (children (GimpToolInfo \"gimp-smudge-tool\" (stock-id \"gimp-tool-smudge\") (visible no)) (GimpToolInfo \"gimp-mypaint-tool\" (stock-id \"gimp-tool-mypaint\") (visible yes))))";
  auto*text=convert(legacy,"toolrc");GError*error=nullptr;auto*scanner=gimp_scanner_new_string(text,-1,&error);
  g_assert_true(gimp_tools_deserialize(gimp,gimp->tool_item_list,scanner));g_assert_no_error(error);gimp_scanner_unref(scanner);g_free(text);
  auto*group=GIMP_TOOL_GROUP(gimp_container_get_first_child(gimp->tool_item_list));g_assert_true(GIMP_IS_TOOL_GROUP(group));g_assert_cmpstr(gimp_object_get_name(group),==,"custom");
  g_assert_cmpstr(gimp_tool_group_get_active_tool(group),==,"gimp-painter-mypaint-tool");
  g_assert_true(gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
  gimp_viewable_set_expanded(GIMP_VIEWABLE(group),FALSE);
  g_assert_false(gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
  auto*children=gimp_viewable_get_children(GIMP_VIEWABLE(group));auto*first=gimp_container_get_first_child(children);
  g_assert_cmpstr(gimp_object_get_name(first),==,"gimp-painter-smudge-tool");g_assert_false(gimp_tool_item_get_visible(GIMP_TOOL_ITEM(first)));
  auto*standard=gimp_get_tool_info(gimp,"gimp-smudge-tool");g_assert_nonnull(standard);g_assert_true(gimp_container_have(gimp->tool_item_list,GIMP_OBJECT(standard)));
  auto*buffer=g_string_new(nullptr);auto*writer=gimp_config_writer_new_from_string(buffer);g_assert_true(gimp_tools_serialize(gimp,gimp->tool_item_list,writer));g_assert_true(gimp_config_writer_finish(writer,nullptr,&error));g_assert_no_error(error);
  g_assert_nonnull(strstr(buffer->str,"(file-version 1)"));scanner=gimp_scanner_new_string(buffer->str,-1,&error);g_assert_true(gimp_tools_deserialize(gimp,gimp->tool_item_list,scanner));g_assert_no_error(error);gimp_scanner_unref(scanner);g_string_free(buffer,TRUE);
  group=GIMP_TOOL_GROUP(gimp_container_get_first_child(gimp->tool_item_list));g_assert_cmpstr(gimp_tool_group_get_active_tool(group),==,"gimp-painter-mypaint-tool");
  g_assert_false(gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
}
static void drop_group(GimpViewable*,gpointer data) {g_clear_object(static_cast<GimpToolGroup**>(data));}
static void mark_gone(gpointer data,GObject*) {*static_cast<bool*>(data)=true;}
static void expanded_last_owner () {
  auto*group=gimp_tool_group_new();bool gone=false;g_object_weak_ref(G_OBJECT(group),mark_gone,&gone);
  g_signal_connect(group,"expanded-changed",G_CALLBACK(drop_group),&group);
  gimp_viewable_set_expanded(GIMP_VIEWABLE(group),FALSE);g_assert_null(group);g_assert_true(gone);
}
static void mypaint_options_and_preset () {
  const char*legacy="(name \"Old Painter preset\") (stock-id \"gimp-tool-mypaint\") (tool-options \"GimpMypaintOptions\" (tool \"gimp-mypaint-tool\") (mypaint-brush \"absent old brush\") (foreground (color-rgb 0.2 0.3 0.4)) (stroke-opacity 0.37) (non-incremental yes) (use-gimp-texture yes) (texture-specified yes) (texture-name \"missing paper\") (future-option (curve (0 1)))) (use-pattern yes)";
  auto*text=convert(legacy,"preset");auto*preset=GIMP_TOOL_PRESET(g_object_new(GIMP_TYPE_TOOL_PRESET,"gimp",gimp,nullptr));deserialize(GIMP_CONFIG(preset),text);g_free(text);
  g_assert_true(GIMP_IS_PAINTER_MYBRUSH_OPTIONS(preset->tool_options));gdouble opacity=0;gboolean nonincremental=FALSE;gchar*texture=nullptr;
  g_object_get(preset->tool_options,"stroke-opacity",&opacity,"non-incremental",&nonincremental,"texture-name",&texture,nullptr);
  g_assert_cmpfloat_with_epsilon(opacity,.37,1e-6);g_assert_true(nonincremental);g_assert_cmpstr(texture,==,"missing paper");g_free(texture);
  g_assert_cmpstr(gimp_object_get_name(GIMP_CONTEXT(preset->tool_options)->tool_info),==,"gimp-painter-mypaint-tool");
  auto*serialized=gimp_config_serialize_to_string(GIMP_CONFIG(preset),nullptr);g_assert_nonnull(serialized);auto*copy=GIMP_TOOL_PRESET(g_object_new(GIMP_TYPE_TOOL_PRESET,"gimp",gimp,nullptr));deserialize(GIMP_CONFIG(copy),serialized);g_free(serialized);
  g_object_get(copy->tool_options,"stroke-opacity",&opacity,"non-incremental",&nonincremental,"texture-name",&texture,nullptr);g_assert_cmpfloat_with_epsilon(opacity,.37,1e-6);g_assert_true(nonincremental);g_assert_cmpstr(texture,==,"missing paper");g_free(texture);g_object_unref(copy);g_object_unref(preset);
}
static void smudge_options_and_context () {
  auto*info=gimp_get_tool_info(gimp,"gimp-painter-smudge-tool");g_assert_nonnull(info);
  auto*text=convert("(tool \"gimp-smudge-tool\") (paint-info \"gimp-smudge\") (rate 37) (use-color-blending yes) (use-texture yes) (paint-mode normal-mode) (texture-grain .4) (future-option (value 2))","options");deserialize(GIMP_CONFIG(info->tool_options),text);g_free(text);
  g_assert_true(G_TYPE_CHECK_INSTANCE_TYPE(info->tool_options,GIMP_TYPE_PAINTER_SMUDGE_OPTIONS));gdouble rate=0;gboolean blending=FALSE;g_object_get(info->tool_options,"rate",&rate,"use-color-blending",&blending,nullptr);g_assert_cmpfloat(rate,==,37);g_assert_true(blending);
  gboolean texture=FALSE;g_object_get(info->tool_options,"use-texture",&texture,nullptr);g_assert_true(texture);
  g_assert_cmpint(gimp_context_get_paint_mode(GIMP_CONTEXT(info->tool_options)),==,GIMP_LAYER_MODE_PAINTER_NORMAL);
  auto*serialized=gimp_config_serialize_to_string(GIMP_CONFIG(info->tool_options),nullptr);auto*copy=g_object_new(G_OBJECT_TYPE(info->tool_options),"gimp",gimp,nullptr);deserialize(GIMP_CONFIG(copy),serialized);g_free(serialized);g_object_get(copy,"rate",&rate,"use-color-blending",&blending,nullptr);g_assert_cmpfloat(rate,==,37);g_assert_true(blending);g_object_get(copy,"use-texture",&texture,nullptr);g_assert_true(texture);g_object_unref(copy);
  auto*context=gimp_context_new(gimp,"migrated",nullptr);text=convert("(tool \"gimp-smudge-tool\") (mypaint-brush \"absent painter brush\")","contextrc");deserialize(GIMP_CONFIG(context),text);g_free(text);
  g_assert_true(gimp_context_get_tool(context)==info);g_assert_cmpstr(context->painter_mybrush_name,==,"absent painter brush");g_object_unref(context);
}
static std::string fixture(const char*name) {
  auto*path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration","fixtures","legacy-profile","profile",name,nullptr);
  gchar*text=nullptr;g_assert_true(g_file_get_contents(path,&text,nullptr,nullptr));std::string result(text);g_free(text);g_free(path);return result;
}
static void genuine_legacy_writer () {
  gimp_set_focused_once(gimp);
  auto options=fixture("tool-options/gimp-mypaint-tool");auto*converted=convert(options.c_str(),"tool-options/gimp-mypaint-tool");
  auto*info=gimp_get_tool_info(gimp,"gimp-painter-mypaint-tool");deserialize(GIMP_CONFIG(info->tool_options),converted);g_free(converted);
  gdouble opacity=0;gboolean nonincremental=FALSE;gchar*paper=nullptr;
  g_object_get(info->tool_options,"stroke-opacity",&opacity,"non-incremental",&nonincremental,"texture-name",&paper,nullptr);
  g_assert_cmpfloat_with_epsilon(opacity,.37,1e-6);g_assert_true(nonincremental);g_assert_cmpstr(paper,==,"Missing paper: preserve reference");g_free(paper);
  auto original=fixture("toolrc");converted=convert(original.c_str(),"toolrc");GError*error=nullptr;auto*scanner=gimp_scanner_new_string(converted,-1,&error);
  g_assert_true(gimp_tools_deserialize(gimp,gimp->tool_item_list,scanner));g_assert_no_error(error);gimp_scanner_unref(scanner);g_free(converted);
  g_assert_true(GIMP_IS_TOOL_GROUP(gimp_container_get_first_child(gimp->tool_item_list)));
  g_assert_nonnull(gimp_get_tool_info(gimp,"gimp-smudge-tool"));
}
static void genuine_device_context () {
  auto original=fixture("devicerc");auto*converted=convert(original.c_str(),"devicerc");
  g_assert_nonnull(strstr(converted,"(tool-options \"GimpPainterDeviceOptions\""));
  auto*list=GIMP_CONTAINER(g_object_new(GIMP_TYPE_LIST,"child-type",GIMP_TYPE_DEVICE_INFO,"append",TRUE,"unique-names",TRUE,nullptr));
  GError*error=nullptr;g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(list),converted,-1,gimp,&error));g_assert_no_error(error);g_free(converted);
  g_assert_cmpint(gimp_container_get_n_children(list),==,2);
  auto*device=GIMP_DEVICE_INFO(gimp_container_get_child_by_name(list,"Core Pointer"));g_assert_nonnull(device);
  auto*preset=GIMP_TOOL_PRESET(device);g_assert_nonnull(preset->tool_options);g_assert_cmpuint(G_OBJECT_TYPE(preset->tool_options),==,GIMP_TYPE_PAINTER_DEVICE_OPTIONS);
  g_assert_cmpstr(gimp_object_get_name(gimp_context_get_tool(GIMP_CONTEXT(preset->tool_options))),==,"gimp-painter-mypaint-tool");
  g_assert_true(preset->use_fg_bg);g_assert_true(preset->use_pattern);g_assert_true(preset->use_gradient);g_assert_false(preset->use_opacity_paint_mode);
  g_assert_false(preset->use_painter_mybrush); // The old device mask did not store MyPaint selection.
  GimpCurve*curve=nullptr;g_object_get(device,"pressure-curve",&curve,nullptr);g_assert_nonnull(curve);gdouble x=0,y=0;gimp_curve_get_point(curve,0,&x,&y);g_assert_cmpfloat(x,==,0);g_assert_cmpfloat(y,==,0);g_object_unref(curve);
  auto*info=gimp_get_tool_info(gimp,"gimp-painter-mypaint-tool");g_object_set(info->tool_options,"stroke-opacity",.61,nullptr);
  gimp_device_info_restore_tool(device);gdouble opacity=0;g_object_get(info->tool_options,"stroke-opacity",&opacity,nullptr);g_assert_cmpfloat_with_epsilon(opacity,.61,1e-6);
  auto*serialized=gimp_config_serialize_to_string(GIMP_CONFIG(list),nullptr);auto*copy=GIMP_CONTAINER(g_object_new(GIMP_TYPE_LIST,"child-type",GIMP_TYPE_DEVICE_INFO,"append",TRUE,nullptr));
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(copy),serialized,-1,gimp,&error));g_assert_no_error(error);g_assert_cmpint(gimp_container_get_n_children(copy),==,2);g_free(serialized);
  gimp_context_set_tool_preset(gimp_get_user_context(gimp),nullptr);g_object_unref(copy);g_object_unref(list);
}
static void independent_defaults_and_mask () {
  auto*config=GIMP_CORE_CONFIG(g_object_new(GIMP_TYPE_RC,"gimp",gimp,nullptr));gchar*standard=nullptr;g_object_get(config,"default-mypaint-brush",&standard,nullptr);
  auto*converted=convert("(default-mypaint-brush \"old custom Painter\") (global-mypaint-brush no)","gimprc");deserialize(GIMP_CONFIG(config),converted);g_free(converted);
  g_assert_cmpstr(config->default_mypaint_brush,==,standard);g_assert_cmpstr(config->default_painter_mypaint_brush,==,"old custom Painter");g_assert_false(config->global_painter_mypaint_brush);g_free(standard);
  auto*text=gimp_config_serialize_to_string(GIMP_CONFIG(config),nullptr);auto*copy=GIMP_CORE_CONFIG(g_object_new(GIMP_TYPE_RC,"gimp",gimp,nullptr));deserialize(GIMP_CONFIG(copy),text);g_assert_cmpstr(copy->default_painter_mypaint_brush,==,config->default_painter_mypaint_brush);g_assert_false(copy->global_painter_mypaint_brush);g_free(text);g_object_unref(copy);g_object_unref(config);
}
static void old_native_brush_defaults () {
  auto*brush=GIMP_BRUSH(gimp_brush_generated_new("profile-native-defaults",GIMP_BRUSH_GENERATED_CIRCLE,7,2,.42,1,0));
  gimp_brush_set_spacing(brush,37);gimp_data_make_internal(GIMP_DATA(brush),"profile-native-defaults");
  auto*container=gimp_data_factory_get_container(gimp->brush_factory);gimp_container_add(container,GIMP_OBJECT(brush));
  auto load=[&](const char*extra) {
    const std::string old=std::string("(tool-options \"GimpPaintOptions\" (tool \"gimp-paintbrush-tool\") (brush \"profile-native-defaults\") ")+extra+")";
    auto*text=convert(old.c_str(),"preset");auto*preset=GIMP_TOOL_PRESET(g_object_new(GIMP_TYPE_TOOL_PRESET,"gimp",gimp,nullptr));deserialize(GIMP_CONFIG(preset),text);g_free(text);return preset;
  };
  auto*preset=load("");gdouble spacing=0,hardness=0;g_object_get(preset->tool_options,"brush-spacing",&spacing,"brush-hardness",&hardness,nullptr);
  g_assert_cmpfloat_with_epsilon(spacing,.37,1e-6);g_assert_cmpfloat_with_epsilon(hardness,1.0,1e-6);
  auto*text=gimp_config_serialize_to_string(GIMP_CONFIG(preset),nullptr);g_assert_null(strstr(text,"(painter-legacy-native-spacing yes)"));g_assert_null(strstr(text,"(painter-legacy-native-hardness yes)"));
  auto*copy=GIMP_TOOL_PRESET(g_object_new(GIMP_TYPE_TOOL_PRESET,"gimp",gimp,nullptr));deserialize(GIMP_CONFIG(copy),text);g_free(text);g_object_get(copy->tool_options,"brush-spacing",&spacing,"brush-hardness",&hardness,nullptr);g_assert_cmpfloat_with_epsilon(spacing,.37,1e-6);g_assert_cmpfloat_with_epsilon(hardness,1.0,1e-6);g_object_unref(copy);g_object_unref(preset);
  preset=load("(brush-spacing .73) (brush-hardness .91)");g_object_get(preset->tool_options,"brush-spacing",&spacing,"brush-hardness",&hardness,nullptr);g_assert_cmpfloat_with_epsilon(spacing,.73,1e-6);g_assert_cmpfloat_with_epsilon(hardness,.91,1e-6);g_object_unref(preset);
  gimp_container_remove(container,GIMP_OBJECT(brush));g_object_unref(brush);
}
static void remove_test_tree(const std::string&path) {
  auto*d=g_dir_open(path.c_str(),0,nullptr);if(!d){g_unlink(path.c_str());return;}
  while(const char*n=g_dir_read_name(d))remove_test_tree(path+"/"+n);
  g_dir_close(d);g_rmdir(path.c_str());
}
static void installer_startup_and_retry () {
  auto*tmp=g_dir_make_tmp("painter-first-run-XXXXXX",nullptr);std::string root(tmp);g_free(tmp);
  auto old=root+"/2.8",now=root+"/3.0";g_assert_cmpint(g_mkdir_with_parents((old+"/tool-options").c_str(),0700),==,0);
  for(const char*name:{"toolrc","contextrc","devicerc","menurc","gimprc","tool-options/gimp-mypaint-tool","tool-options/gimp-smudge-tool"}) {
    auto content=fixture(name);g_assert_true(g_file_set_contents((old+"/"+name).c_str(),content.c_str(),content.size(),nullptr));
  }
  const char*settings="(save-tool-options yes) (default-mypaint-brush \"legacy independent default\") (global-mypaint-brush no) (mypaint-brush-path \"${gimp_dir}/mypaint-brushes:/custom/legacy-brushes\")";
  g_assert_true(g_file_set_contents((old+"/gimprc").c_str(),settings,-1,nullptr));
  // Deliberately authored distinct values exercise the old filename mapping;
  // the genuine writer fixture only emitted its selected gradient resource.
  g_assert_true(g_file_set_contents((old+"/tool-options/gimp-blend-tool").c_str(),
    "(offset 17.5) (gradient-type radial) (gradient-reverse yes)",-1,nullptr));
  gchar*previous=g_strdup(g_getenv("GIMP3_DIRECTORY"));g_setenv("GIMP3_DIRECTORY",now.c_str(),TRUE);
  auto*install=gimp_user_install_new(G_OBJECT(gimp),FALSE);g_assert_true(gimp_user_install_run(install,1));
  auto load=[&](const char*name){gchar*bytes=nullptr;g_assert_true(g_file_get_contents((now+"/"+name).c_str(),&bytes,nullptr,nullptr));std::string result(bytes);g_free(bytes);return result;};
  auto shortcuts=load("shortcutsrc");
  g_assert_nonnull(strstr(shortcuts.c_str(),"(action \"tools-painter-mypaint\" \"<Primary><Shift>p\")"));
  g_assert_nonnull(strstr(shortcuts.c_str(),"(action \"tools-painter-smudge\" \"<Primary><Shift>s\")"));
  g_assert_nonnull(strstr(shortcuts.c_str(),"(action \"view-reset\" \"<Primary>0\")"));
  auto options=load("tool-options/gimp-painter-mypaint-tool");g_assert_nonnull(strstr(options.c_str(),"(stroke-opacity 0.370000)"));
  auto config=load("gimprc");g_assert_nonnull(strstr(config.c_str(),"(default-painter-mypaint-brush \"legacy independent default\")"));g_assert_nonnull(strstr(config.c_str(),"(global-painter-mypaint-brush no)"));g_assert_nonnull(strstr(config.c_str(),"painter-mypaint-brushes:/custom/legacy-brushes"));
  g_assert_null(strstr(config.c_str(),"(default-mypaint-brush \"legacy independent default\")"));
  // Reload the exact installed files through the same registered runtime entry
  // points used at startup, rather than merely inspecting transformed strings.
  GError*error=nullptr;
  gimp_tools_reset(gimp,gimp->tool_item_list,TRUE);
  g_assert_true(GIMP_IS_TOOL_GROUP(gimp_container_get_first_child(gimp->tool_item_list)));
  g_assert_true(gimp_contexts_load(gimp,&error));g_assert_no_error(error);
  auto*my_info=gimp_get_tool_info(gimp,"gimp-painter-mypaint-tool");
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==my_info);
  g_assert_true(gimp_tool_options_deserialize(my_info->tool_options,&error));g_assert_no_error(error);
  gdouble restored_opacity=0;g_object_get(my_info->tool_options,"stroke-opacity",&restored_opacity,nullptr);g_assert_cmpfloat_with_epsilon(restored_opacity,.37,1e-6);
  g_assert_cmpint(gimp_context_get_paint_mode(GIMP_CONTEXT(my_info->tool_options)),==,GIMP_LAYER_MODE_PAINTER_NORMAL);
  auto*gradient_info=gimp_get_tool_info(gimp,"gimp-gradient-tool");g_assert_nonnull(gradient_info);
  g_assert_true(gimp_tool_options_deserialize(gradient_info->tool_options,&error));g_assert_no_error(error);
  gdouble gradient_offset=0;gint gradient_type=0;gboolean gradient_reverse=FALSE;
  g_object_get(gradient_info->tool_options,"offset",&gradient_offset,"gradient-type",&gradient_type,"gradient-reverse",&gradient_reverse,nullptr);
  g_assert_cmpfloat_with_epsilon(gradient_offset,17.5,1e-6);g_assert_cmpint(gradient_type,==,GIMP_GRADIENT_RADIAL);g_assert_true(gradient_reverse);
  g_assert_false(g_file_test((now+"/tool-options/gimp-blend-tool").c_str(),G_FILE_TEST_EXISTS));
  auto*device_file=g_file_new_for_path((now+"/devicerc").c_str());
  auto*devices=GIMP_CONTAINER(g_object_new(GIMP_TYPE_LIST,"child-type",GIMP_TYPE_DEVICE_INFO,"append",TRUE,"unique-names",TRUE,nullptr));
  g_assert_true(gimp_config_deserialize_file(GIMP_CONFIG(devices),device_file,gimp,&error));g_assert_no_error(error);g_object_unref(device_file);
  auto*old_device=GIMP_DEVICE_INFO(gimp_container_get_child_by_name(devices,"Core Pointer"));g_assert_nonnull(old_device);
  gimp_device_info_restore_tool(old_device);g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==my_info);
  gimp_context_set_tool_preset(gimp_get_user_context(gimp),nullptr);g_object_unref(devices);
  auto*shortcut_file=g_file_new_for_path((now+"/shortcutsrc").c_str());g_assert_true(shortcuts_rc_parse(GTK_APPLICATION(gimp->app),shortcut_file,&error));g_assert_no_error(error);g_object_unref(shortcut_file);
  for(const char*name:{"tools-painter-mypaint","tools-painter-smudge"}) {
    auto*action=g_action_map_lookup_action(G_ACTION_MAP(gimp->app),name);g_assert_nonnull(action);g_assert_true(GIMP_IS_ACTION(action));
    const gchar**accels=gimp_action_get_accels(GIMP_ACTION(action));g_assert_nonnull(accels);g_assert_nonnull(accels[0]);
    guint key=0,expected_key=0;GdkModifierType modifiers,expected_modifiers;gtk_accelerator_parse(accels[0],&key,&modifiers);
    gtk_accelerator_parse(std::strstr(name,"mypaint")?"<Primary><Shift>p":"<Primary><Shift>s",&expected_key,&expected_modifiers);
    g_assert_cmpuint(gdk_keyval_to_lower(key),==,gdk_keyval_to_lower(expected_key));
    g_assert_cmpuint(modifiers,==,expected_modifiers);
    g_action_activate(action,nullptr);
    g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==gimp_get_tool_info(gimp,std::strstr(name,"mypaint")?"gimp-painter-mypaint-tool":"gimp-painter-smudge-tool"));
  }

  const char*edited="(default-brush \"Destination edit\")\n";g_assert_true(g_file_set_contents((now+"/gimprc").c_str(),edited,-1,nullptr));
  const char*edited_options="(stroke-opacity .82)\n";g_assert_true(g_file_set_contents((now+"/tool-options/gimp-painter-mypaint-tool").c_str(),edited_options,-1,nullptr));
  g_assert_true(gimp_user_install_run(install,1));g_assert_true(load("gimprc")==edited);g_assert_true(load("tool-options/gimp-painter-mypaint-tool")==edited_options);gimp_user_install_free(install);
  if(previous)g_setenv("GIMP3_DIRECTORY",previous,TRUE);else g_unsetenv("GIMP3_DIRECTORY");g_free(previous);remove_test_tree(root);
}
int main(int argc,char**argv) {
  g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/painter-profile-native/tool-order-roundtrip",tool_order_and_groups);
  g_test_add_func("/painter-profile-native/mypaint-preset-roundtrip",mypaint_options_and_preset);
  g_test_add_func("/painter-profile-native/smudge-context-roundtrip",smudge_options_and_context);
  g_test_add_func("/painter-profile-native/genuine-old-writer",genuine_legacy_writer);
  g_test_add_func("/painter-profile-native/genuine-device-context",genuine_device_context);
  g_test_add_func("/painter-profile-native/independent-defaults",independent_defaults_and_mask);
  g_test_add_func("/painter-profile-native/first-run-installer-retry",installer_startup_and_retry);
  g_test_add_func("/painter-profile-native/native-brush-defaults",old_native_brush_defaults);
  g_test_add_func("/painter-profile-native/expanded-last-owner",expanded_last_owner);
  g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
