/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <glib-object.h>
#include <gio/gio.h>
#include "libgimpbase/gimpbase.h"
_Static_assert(GIMP_MYPAINT_NORMAL==0 && GIMP_MYPAINT_NORMAL_AND_ERASE==1 &&
               GIMP_MYPAINT_LOCK_ALPHA==2 && GIMP_MYPAINT_COLORIZE==3,
               "Original public library enum values");
GType mypaint_enum_public_c_check(void);
GType mypaint_enum_public_c_check(void)
{
 GType (*entry)(void)=gimp_mypaint_brush_mode_get_type;
 GType type=entry();GEnumClass *klass=g_type_class_ref(type);
 const gchar *names[]={"GIMP_MYPAINT_NORMAL","GIMP_MYPAINT_NORMAL_AND_ERASE","GIMP_MYPAINT_LOCK_ALPHA","GIMP_MYPAINT_COLORIZE"};
 const gchar *nicks[]={"normal","normal-and-erase","lock-alpha","colorize"};
 const gchar *descriptions[]={"Normal","Normal and Erase","Lock Alpha","Colorize"};
 const GimpEnumDesc *descs=gimp_enum_get_value_descriptions(type);
 g_assert_cmpstr(g_type_name(type),==,"GimpMypaintBrushMode");g_assert_cmpuint(type,==,GIMP_TYPE_MYPAINT_BRUSH_MODE);g_assert_cmpuint(klass->n_values,==,4);
 g_assert_cmpstr(gimp_type_get_translation_domain(type),==,GETTEXT_PACKAGE "-libgimp");g_assert_cmpstr(gimp_type_get_translation_context(type),==,"mypaint-brush-mode");g_assert_nonnull(descs);
 for(gint i=0;i<4;++i){const GEnumValue *value=g_enum_get_value(klass,i);GValue v=G_VALUE_INIT,copy=G_VALUE_INIT;g_assert_nonnull(value);g_assert_cmpstr(value->value_name,==,names[i]);g_assert_cmpstr(value->value_nick,==,nicks[i]);g_assert_cmpint(descs[i].value,==,i);g_assert_cmpstr(descs[i].value_desc,==,descriptions[i]);g_assert_null(descs[i].value_help);g_value_init(&v,type);g_value_set_enum(&v,i);g_value_init(&copy,type);g_value_copy(&v,&copy);g_value_unset(&v);g_assert_cmpint(g_value_get_enum(&copy),==,i);g_value_unset(&copy);}
 g_assert_null(descs[4].value_desc);g_type_class_unref(klass);return type;
}
#ifdef MYPAINT_ENUM_STANDALONE
int main(void){g_assert_cmpuint(g_type_from_name("GimpMypaintBrushMode"),==,0);for(guint i=0;i<32;++i)mypaint_enum_public_c_check();g_print("PASS: standalone C11 public library enum identity/values/nicks/metadata\n");return 0;}
#endif
