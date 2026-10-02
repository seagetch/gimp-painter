/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <cstring>
#include <string>
#include "core/gimppainterprofile.h"
static std::string converted (const char *text,const char *kind="settings",bool origin=true) {
  GError*error=nullptr;auto*out=gimp_painter_profile_transform(text,kind,origin,&error);g_assert_no_error(error);g_assert_nonnull(out);std::string result(out);g_free(out);return result;
}
static void structured_mapping () {
  const char*input="# gimp-smudge-tool stays in comments\n(tool \"gimp-smudge-tool\")\n(mypaint-brush \"gimp-mypaint-tool\")\n(future (tool \"gimp-smudge-tool\"))\n(label \"gimp-mypaint-tool\")\n";
  g_assert_true(converted(input)=="# gimp-smudge-tool stays in comments\n(tool \"gimp-painter-smudge-tool\")\n(painter-mybrush \"gimp-mypaint-tool\")\n(future (tool \"gimp-smudge-tool\"))\n(label \"gimp-mypaint-tool\")\n");
  g_assert_true(converted(input,"contextrc",false)==input);
  g_assert_true(converted("(tool-options \"GimpMypaintOptions\" (tool \"gimp-mypaint-tool\") (stroke-opacity 0.37) (texture-name \"paper \\\"rough\\\"\") (unknown (nested 3)))","preset")=="(tool-options \"GimpPainterMybrushOptions\" (tool \"gimp-painter-mypaint-tool\") (stroke-opacity 0.37) (texture-name \"paper \\\"rough\\\"\") (unknown (nested 3))\n(paint-mode painter-normal))");
  g_assert_true(converted("(GimpDeviceInfo \"stylus\" (tool \"gimp-smudge-tool\") (tool-options \"GimpSmudgeOptions\" (use-color-blending yes)))","devicerc").find("GimpPainterSmudgeOptions")!=std::string::npos);
}
static void toolrc_and_modes () {
  auto text=converted("(file-version 1)\n(GimpToolGroup \"mine\" (active-tool \"gimp-mypaint-tool\") (expanded no) (children (GimpToolInfo \"gimp-mypaint-tool\" (stock-id \"gimp-tool-mypaint\") (visible no))))","toolrc");
  g_assert_nonnull(strstr(text.c_str(),"(file-version 1001)"));g_assert_nonnull(strstr(text.c_str(),"(active-tool \"gimp-painter-mypaint-tool\")"));g_assert_nonnull(strstr(text.c_str(),"(expanded no)"));g_assert_nonnull(strstr(text.c_str(),"(icon-name \"gimp-tool-mypaint-brush\")"));
  for(auto name:{"normal","multiply","erase","replace","anti-erase","src-in","dst-in","src-out","dst-out"}) {
    auto input=std::string("(paint-mode ")+name+"-mode)";
    g_assert_true(converted(input.c_str())==std::string("(paint-mode painter-")+name+")");
  }
  g_assert_true(converted("(paint-mode hue-mode)")=="(paint-mode hsv-hue-legacy)");
  g_assert_nonnull(strstr(converted("(tool \"gimp-smudge-tool\")","contextrc").c_str(),"(paint-mode painter-normal)"));
  g_assert_cmpstr(gimp_painter_profile_action("tools-mypaint"),==,"tools-painter-mypaint");
  g_assert_cmpstr(gimp_painter_profile_action("tools-mypaint-brush"),==,"tools-mypaint-brush");
}
static void malformed () {
  for(const char*input:{"(tool \"oops)","(tool x", ")", "atom", "(tool \\\""}) {GError*error=nullptr;g_assert_null(gimp_painter_profile_transform(input,"toolrc",true,&error));g_assert_nonnull(error);g_clear_error(&error);}
  auto deep=std::string(130,'(')+"x"+std::string(130,')');GError*error=nullptr;g_assert_null(gimp_painter_profile_transform(deep.c_str(),"toolrc",true,&error));g_assert_nonnull(error);g_clear_error(&error);
}
static void put(const std::string&root,const char*name,const char*text) {
  auto path=root+"/"+name;auto*parent=g_path_get_dirname(path.c_str());g_assert_cmpint(g_mkdir_with_parents(parent,0700),==,0);g_free(parent);g_assert_true(g_file_set_contents(path.c_str(),text,-1,nullptr));
}
static std::string get(const std::string&root,const char*name) {gchar*bytes=nullptr;gsize length=0;g_assert_true(g_file_get_contents((root+"/"+name).c_str(),&bytes,&length,nullptr));std::string result(bytes,length);g_free(bytes);return result;}
static void remove_tree(const std::string&path) {
  if(g_file_test(path.c_str(),G_FILE_TEST_IS_SYMLINK)) {g_unlink(path.c_str());return;}
  GDir*d=g_dir_open(path.c_str(),0,nullptr);if(!d) {g_unlink(path.c_str());return;}
  while(const char*n=g_dir_read_name(d)) remove_tree(path+"/"+n);
  g_dir_close(d);g_rmdir(path.c_str());
}
static void origin_and_conflicts () {
  GError*error=nullptr;auto*tmp=g_dir_make_tmp("painter-profile-XXXXXX",&error);g_assert_no_error(error);std::string root(tmp);g_free(tmp);auto src=root+"/old",dst=root+"/new";
  put(src,"toolrc","# GIMP 2.8\n(file-version 1)\n(GimpToolInfo \"gimp-smudge-tool\")\n");
  g_assert_false(gimp_painter_profile_detect(src.c_str()));
  put(src,"contextrc","(label \"gimp-mypaint-tool\") (future (tool \"gimp-mypaint-tool\"))");g_assert_false(gimp_painter_profile_detect(src.c_str()));
  g_assert_true(gimp_painter_profile_preserve(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_false(g_file_test((dst+"/toolrc").c_str(),G_FILE_TEST_EXISTS));
  put(src,"toolrc","(file-version 1) (GimpToolInfo \"gimp-mypaint-tool\" (visible yes))");g_assert_true(gimp_painter_profile_detect(src.c_str()));
  const char*options="(tool \"gimp-smudge-tool\") (use-color-blending yes) (rate 37) (future (unknown \"unchanged\"))";
  put(src,"tool-options/gimp-smudge-tool",options);
  put(src,"mypaint-brushes/user.myb","{\"version\":3,\"settings\":{},\"user\":true}");
  put(src,"mypaint-brushes/a space.preview.png","fake image byte fixture");
  put(src,"tool-presets/broken.gtp","(tool-options \"oops");
  put(src,"tool-presets/nested/preset.gtp","(tool-options \"GimpMypaintOptions\" (stroke-opacity 0.3))");
  put(dst,"painter-mypaint-brushes/user.myb","edited destination");
  g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_true(get(dst,"painter-mypaint-brushes/user.myb")=="edited destination");
  g_assert_true(get(src,"tool-options/gimp-smudge-tool")==options);
  g_assert_true(get(dst,"tool-options/gimp-painter-smudge-tool").find("(future (unknown \"unchanged\"))")!=std::string::npos);
  g_assert_false(g_file_test((dst+"/tool-presets/broken.gtp").c_str(),G_FILE_TEST_EXISTS));
  auto*digest=g_compute_checksum_for_string(G_CHECKSUM_SHA256,options,-1);auto archive="painter-migration/originals/"+std::string(digest)+"/tool-options/gimp-smudge-tool";g_free(digest);
  g_assert_true(get(dst,archive.c_str())==options);
  put(dst,"tool-options/gimp-painter-smudge-tool","user edit after first run");
  g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_true(get(dst,"tool-options/gimp-painter-smudge-tool")=="user edit after first run");
  remove_tree(root);
}
static void blend_filename_options () {
  auto*tmp=g_dir_make_tmp("painter-profile-gradient-XXXXXX",nullptr);std::string root(tmp);g_free(tmp);
  auto src=root+"/old",dst=root+"/new";
  put(src,"toolrc","(file-version 1) (GimpToolInfo \"gimp-mypaint-tool\") (GimpToolInfo \"gimp-blend-tool\")");
  const char*options="(offset 17.5) (gradient-type radial) (gradient-reverse yes)";
  put(src,"tool-options/gimp-blend-tool",options);GError*error=nullptr;
  g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_nonnull(strstr(get(dst,"tool-options/gimp-gradient-tool").c_str(),options));
  g_assert_false(g_file_test((dst+"/tool-options/gimp-blend-tool").c_str(),G_FILE_TEST_EXISTS));
  put(dst,"tool-options/gimp-gradient-tool","(offset 31.25)");
  g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_true(get(dst,"tool-options/gimp-gradient-tool")=="(offset 31.25)");remove_tree(root);
}
static void device_schema_and_paths () {
  auto device=converted("(GimpDeviceInfo \"pen\" (tool \"gimp-mypaint-tool\") (brush \"shape\") (axes 2 x y) (future (tool \"gimp-mypaint-tool\")))","devicerc");
  g_assert_nonnull(strstr(device.c_str(),"(tool-options \"GimpPainterDeviceOptions\""));
  g_assert_nonnull(strstr(device.c_str(),"(tool \"gimp-painter-mypaint-tool\")"));
  g_assert_nonnull(strstr(device.c_str(),"(axes 2 x y)"));
  g_assert_nonnull(strstr(device.c_str(),"(future (tool \"gimp-mypaint-tool\"))"));
  g_assert_nonnull(strstr(device.c_str(),"(use-opacity-paint-mode no)"));
  auto*tmp=g_dir_make_tmp("painter-profile-paths-XXXXXX",nullptr);std::string root(tmp);g_free(tmp);
  put(root,"gimprc","(mypaint-brush-path \"${gimp_dir}/mypaint-brushes:/external/custom:${gimp_data_dir}/mypaint-brushes\") (mypaint-brush-path-writable \"/old/do-not-edit\") (brush-path \"/external/shapes\") (plug-in-path \"/no/executables\")");
  GError*error=nullptr;auto*paths=gimp_painter_profile_resource_paths(root.c_str(),&error);g_assert_no_error(error);
  g_assert_nonnull(strstr(paths,"(painter-mypaint-brush-path \"${gimp_dir}/painter-mypaint-brushes:/external/custom:${gimp_data_dir}/painter-mypaint-brushes\")"));
  g_assert_nonnull(strstr(paths,"(brush-path \"/external/shapes:${gimp_dir}/brushes\")"));g_assert_null(strstr(paths,"writable"));g_assert_null(strstr(paths,"plug-in"));g_free(paths);remove_tree(root);
}
static void options_only_provenance () {
  auto*tmp=g_dir_make_tmp("painter-profile-origin-XXXXXX",nullptr);std::string root(tmp);g_free(tmp);
  put(root,"toolrc","(file-version 1) (GimpToolInfo \"gimp-bucket-fill-brush-tool\" (icon-name \"gimp-tool-bucket-fill\")) (GimpToolInfo \"gimp-smudge-tool\" (icon-name \"gimp-tool-smudge\"))");
  g_assert_false(gimp_painter_profile_detect(root.c_str()));
  put(root,"toolrc","(file-version 1) (GimpToolInfo \"gimp-bucket-fill-brush-tool\" (stock-id \"gimp-tool-bucket-fill\"))");g_assert_true(gimp_painter_profile_detect(root.c_str()));
  g_unlink((root+"/toolrc").c_str());
  put(root,"tool-options/gimp-smudge-tool","(rate 37)");g_assert_false(gimp_painter_profile_detect(root.c_str()));
  put(root,"tool-options/gimp-smudge-tool","(rate 37) (use-color-blending no)");g_assert_true(gimp_painter_profile_detect(root.c_str()));
  g_unlink((root+"/tool-options/gimp-smudge-tool").c_str());
  put(root,"tool-options/gimp-mypaint-tool","(stroke-opacity .37)");g_assert_true(gimp_painter_profile_detect(root.c_str()));remove_tree(root);
}
static void symlinks_and_nested_destination () {
  auto*tmp=g_dir_make_tmp("painter-profile-links-XXXXXX",nullptr);std::string root(tmp);g_free(tmp);auto src=root+"/old",dst=root+"/new";
  put(src,"toolrc","(file-version 1) (GimpToolInfo \"gimp-mypaint-tool\")");
  GError*error=nullptr;g_assert_false(gimp_painter_profile_migrate(src.c_str(),(src+"/nested").c_str(),&error));g_assert_nonnull(error);g_clear_error(&error);
  put(src,"mypaint-brushes/ordinary.myb","original bytes");
  auto*link=g_file_new_for_path((src+"/mypaint-brushes/loop").c_str());
  if(!g_file_make_symbolic_link(link,src.c_str(),nullptr,&error)) {g_test_skip("Symbolic links unavailable");g_clear_error(&error);g_object_unref(link);remove_tree(root);return;}
  g_object_unref(link);g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);g_assert_false(g_file_test((dst+"/painter-mypaint-brushes/loop").c_str(),G_FILE_TEST_EXISTS));
  auto other=root+"/other";g_assert_cmpint(g_mkdir(other.c_str(),0700),==,0);auto dst2=root+"/new2";g_assert_cmpint(g_mkdir(dst2.c_str(),0700),==,0);
  link=g_file_new_for_path((dst2+"/painter-migration").c_str());g_assert_true(g_file_make_symbolic_link(link,other.c_str(),nullptr,&error));g_assert_no_error(error);g_object_unref(link);
  g_assert_false(gimp_painter_profile_migrate(src.c_str(),dst2.c_str(),&error));g_assert_nonnull(error);g_clear_error(&error);g_assert_false(g_file_test((other+"/originals").c_str(),G_FILE_TEST_EXISTS));remove_tree(root);
}
static void canvas_behavior_default () {
  auto value=converted("# old defaults\n","gimprc");
  g_assert_nonnull(strstr(value.c_str(),"(painter-canvas-ui yes)"));
  value=converted("(painter-canvas-ui no)","gimprc");
  g_assert_true(value=="(painter-canvas-ui no)");
  GError *error=nullptr;
  auto *unchanged=gimp_painter_profile_transform("# ambiguous origin\n","gimprc",FALSE,&error);
  g_assert_no_error(error);g_assert_cmpstr(unchanged,==,"# ambiguous origin\n");g_free(unchanged);
  auto *tmp=g_dir_make_tmp("painter-canvas-profile-XXXXXX",nullptr);std::string root(tmp);g_free(tmp);
  auto src=root+"/old",dst=root+"/new";
  put(src,"toolrc","(file-version 1) (GimpToolInfo \"gimp-mypaint-tool\")");
  g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_nonnull(strstr(get(dst,"gimprc").c_str(),"(painter-canvas-ui yes)"));
  put(dst,"gimprc","(painter-canvas-ui no)");
  g_assert_true(gimp_painter_profile_migrate(src.c_str(),dst.c_str(),&error));g_assert_no_error(error);
  g_assert_true(get(dst,"gimprc")=="(painter-canvas-ui no)");remove_tree(root);
}
int main(int argc,char**argv) {
  g_test_init(&argc,&argv,nullptr);
  g_test_add_func("/painter-profile/structured-mapping",structured_mapping);
  g_test_add_func("/painter-profile/toolrc-modes",toolrc_and_modes);
  g_test_add_func("/painter-profile/malformed",malformed);
  g_test_add_func("/painter-profile/origin-conflicts",origin_and_conflicts);
  g_test_add_func("/painter-profile/device-schema-paths",device_schema_and_paths);
  g_test_add_func("/painter-profile/symlink-nested-rejection",symlinks_and_nested_destination);
  g_test_add_func("/painter-profile/options-only-origin",options_only_provenance);
  g_test_add_func("/painter-profile/blend-filename-options",blend_filename_options);
  g_test_add_func("/painter-profile/canvas-behavior-default",canvas_behavior_default);
  return g_test_run();
}
