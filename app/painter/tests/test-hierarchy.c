/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-hierarchy.h"
#include "gimp-painter-binding.h"

typedef struct _PainterHierarchyBaseClass { GObjectClass parent; } PainterHierarchyBaseClass;
typedef struct _PainterHierarchyChildClass { PainterHierarchyBaseClass parent; } PainterHierarchyChildClass;
struct _PainterHierarchyBase { GObject parent; };
struct _PainterHierarchyChild { PainterHierarchyBase parent; };
static void readable_init (PainterReadableInterface *iface);
static PainterPropertyTrace property_trace;
void painter_hierarchy_reset_property_trace (void)
{ property_trace = (PainterPropertyTrace) {0, 0, 0, 0}; }
PainterPropertyTrace painter_hierarchy_property_trace (void)
{ return property_trace; }
G_DEFINE_INTERFACE (PainterReadable,painter_readable,G_TYPE_OBJECT)
G_DEFINE_TYPE (PainterHierarchyBase,painter_hierarchy_base,G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_CODE (PainterHierarchyChild,painter_hierarchy_child,painter_hierarchy_base_get_type (),
                        G_IMPLEMENT_INTERFACE (painter_readable_get_type (),readable_init))
static void painter_readable_default_init (PainterReadableInterface *iface) { (void) iface; }
static void readable_init (PainterReadableInterface *iface) { iface->read = painter_hierarchy_read; }
gint painter_readable_read (PainterReadable *self, GError **error)
{
  PainterReadableInterface *iface = G_TYPE_INSTANCE_GET_INTERFACE (self,painter_readable_get_type (),PainterReadableInterface);
  return iface->read (self,error);
}
static void base_constructed (GObject *owner)
{
  G_OBJECT_CLASS (painter_hierarchy_base_parent_class)->constructed (owner);
  painter_hierarchy_activate_binding (owner);
}
static void base_dispose (GObject *owner)
{
  g_assert_true (gimp_painter_binding_close (owner,NULL));
  painter_hierarchy_parent_dispose (owner);
  G_OBJECT_CLASS (painter_hierarchy_base_parent_class)->dispose (owner);
}
static void child_dispose (GObject *owner)
{
  g_assert_true (gimp_painter_binding_close (owner,NULL));
  G_OBJECT_CLASS (painter_hierarchy_child_parent_class)->dispose (owner);
}
static void base_finalize (GObject *owner)
{
  painter_hierarchy_finalize_phase (1);
  G_OBJECT_CLASS (painter_hierarchy_base_parent_class)->finalize (owner);
  painter_hierarchy_finalize_phase (-1);
}
static void child_finalize (GObject *owner)
{
  painter_hierarchy_finalize_phase (2);
  G_OBJECT_CLASS (painter_hierarchy_child_parent_class)->finalize (owner);
  painter_hierarchy_finalize_phase (-2);
}
static void base_set (GObject *owner, guint id, const GValue *value, GParamSpec *spec)
{
  ++property_trace.base_set;
  if (id == 1) painter_hierarchy_set (owner,FALSE,g_value_get_int (value));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (owner,id,spec);
}
static void base_get (GObject *owner, guint id, GValue *value, GParamSpec *spec)
{
  ++property_trace.base_get;
  if (id == 1) g_value_set_int (value,painter_hierarchy_get (owner,FALSE));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (owner,id,spec);
}
static void child_set (GObject *owner, guint id, const GValue *value, GParamSpec *spec)
{
  ++property_trace.child_set;
  if (id == 2) painter_hierarchy_set (owner,TRUE,g_value_get_int (value));
  else G_OBJECT_CLASS (painter_hierarchy_child_parent_class)->set_property (owner,id,value,spec);
}
static void child_get (GObject *owner, guint id, GValue *value, GParamSpec *spec)
{
  ++property_trace.child_get;
  if (id == 2) g_value_set_int (value,painter_hierarchy_get (owner,TRUE));
  else G_OBJECT_CLASS (painter_hierarchy_child_parent_class)->get_property (owner,id,value,spec);
}
static void painter_hierarchy_base_class_init (PainterHierarchyBaseClass *klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);
  object->constructed = base_constructed; object->dispose = base_dispose;
  object->finalize = base_finalize;
  object->set_property = base_set; object->get_property = base_get;
  g_object_class_install_property (object,1,g_param_spec_int ("base-value","Base value","Base value",0,99,7,G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}
static void painter_hierarchy_child_class_init (PainterHierarchyChildClass *klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);
  object->dispose = child_dispose; object->set_property = child_set; object->get_property = child_get;
  object->finalize = child_finalize;
  g_object_class_install_property (object,2,g_param_spec_int ("child-value","Child value","Child value",0,99,9,G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}
static void painter_hierarchy_base_init (PainterHierarchyBase *self) { painter_hierarchy_init_binding (G_OBJECT (self),FALSE); }
static void painter_hierarchy_child_init (PainterHierarchyChild *self) { painter_hierarchy_init_binding (G_OBJECT (self),TRUE); }
