#ifndef __MYPAINTBRUSH_BRUSHSETTINGS_H__
#define __MYPAINTBRUSH_BRUSHSETTINGS_H__

#include <glib.h>

struct _MyPaintBrushInputSettings {
  gchar   *name;
  gint    index;
  gfloat  hard_minimum;
  gfloat  soft_minimum;
  gfloat  normal;
  gfloat  soft_maximum;
  gfloat  hard_maximum;
  gchar  *displayed_name;
  gchar  *tooltip;  
};
typedef struct _MyPaintBrushInputSettings MyPaintBrushInputSettings;

enum _MypaintBrushSettingGroupID {
  BRUSH_SETTING_GROUP_BASIC,
  BRUSH_SETTING_GROUP_OPACITY,
  BRUSH_SETTING_GROUP_DABS,
  BRUSH_SETTING_GROUP_SMUDGE,
  BRUSH_SETTING_GROUP_SPEED,
  BRUSH_SETTING_GROUP_TRACKING,
  BRUSH_SETTING_GROUP_STROKE,
  BRUSH_SETTING_GROUP_COLOR,
  BRUSH_SETTING_GROUP_TEXTURE,
  BRUSH_SETTING_GROUP_CUSTOM
};
typedef enum _MypaintBrushSettingGroupID MypaintBrushSettingGroupID;

struct _MypaintBrushSettingGroup {
  gchar                          *name;
  MypaintBrushSettingGroupID      index;
  gchar                          *display_name;
  GList                          *setting_list;
};
typedef struct _MypaintBrushSettingGroup MypaintBrushSettingGroup;

struct _MyPaintBrushSettings {
  gchar    *internal_name;
  gint      index;
  gchar    *displayed_name;
  gboolean  constant;
  gfloat    minimum;
  gfloat    default_value;
  gfloat    maximum;
  gchar    *tooltip;
};
typedef struct _MyPaintBrushSettings MyPaintBrushSettings;

struct _MyPaintBrushSwitchSettings {
  gchar    *internal_name;
  gint      index;
  gchar    *displayed_name;
  gboolean  default_value;
};
typedef struct _MyPaintBrushSwitchSettings MyPaintBrushSwitchSettings;

struct _MyPaintBrushTextSettings {
  gchar    *internal_name;
  gint      index;
  gchar    *displayed_name;
  gchar    *default_value;
};
typedef struct _MyPaintBrushTextSettings MyPaintBrushTextSettings;

struct _MyPaintBrushSettingMigrate {
  gchar *old_name;
  gchar *new_name;
  gfloat (*transform)(gfloat y);
};
typedef struct _MyPaintBrushSettingMigrate MyPaintBrushSettingMigrate;

struct _MyPaintBrushSettingEntry {
  GType  type;
  union {
    MyPaintBrushSettings       *f;
    MyPaintBrushSwitchSettings *b;
    MyPaintBrushTextSettings   *t;
    gpointer                    ptr;
  };
};
typedef struct _MyPaintBrushSettingEntry MyPaintBrushSettingEntry;

GList *mypaint_brush_get_brush_settings (void);
GList *mypaint_brush_get_brush_switch_settings (void);
GList *mypaint_brush_get_brush_text_settings (void);
GList *mypaint_brush_get_input_settings (void);
GHashTable *mypaint_brush_get_brush_settings_dict (void);
GHashTable *mypaint_brush_get_brush_switch_settings_dict (void);
GHashTable *mypaint_brush_get_brush_text_settings_dict (void);
GHashTable *mypaint_brush_get_input_settings_dict (void);
GHashTable *mypaint_brush_get_setting_migrate_dict (void);
GHashTable *mypaint_brush_get_setting_group_dict (void);
GList *mypaint_brush_get_setting_group_list (void);

gchar* mypaint_brush_internal_name_to_signal_name (const gchar* name);
gchar* mypaint_brush_signal_name_to_internal_name (const gchar* signal);

GType mypaint_brush_get_prop_type (const gchar* name);
gchar* mypaint_brush_settings_index_to_internal_name(int index);
#endif
