/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmypaint_brusheditor.c
 * Copyright 1998 Jay Cox <jaycox@earthlink.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-utils.hpp"

extern "C" {
#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/mypaintbrush-brushsettings.h"
#include "core/gimpmypaintbrush.h"

#include "gimpmypaintbrusheditor.h"
#include "gimpdocked.h"
#include "gimpspinscale.h"
#include "gimpview.h"
#include "gimpviewrenderer.h"
#include "gimppropwidgets.h"
#include "gimpspinscale.h"
#include "gimpwidgets-constructors.h"

#include "gimp-intl.h"
#include "paint/gimpmypaintoptions.h"

extern "C++" {
#include "base/scopeguard.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/delegators.hpp"
}

#define MYPAINT_BRUSH_VIEW_SIZE 32

using namespace GLib;

/*  local function prototypes  */

static void   gimp_mypaint_brush_editor_docked_iface_init (GimpDockedInterface *face);

static void   gimp_mypaint_brush_editor_constructed    (GObject            *object);

static void   gimp_mypaint_brush_editor_set_data       (GimpDataEditor     *editor,
                                                GimpData           *data);

static void   gimp_mypaint_brush_editor_set_context    (GimpDocked         *docked,
                                                GimpContext        *context);

static void   gimp_mypaint_brush_editor_update_mypaint_brush   (GtkAdjustment      *adjustment,
                                                GimpMypaintBrushEditor    *editor);
static void   gimp_mypaint_brush_editor_update_shape   (GtkWidget          *widget,
                                                GimpMypaintBrushEditor    *editor);
static void   gimp_mypaint_brush_editor_notify_mypaint_brush   (GimpMypaintBrush *mypaint_brush,
                                                GParamSpec         *pspec,
                                                GimpMypaintBrushEditor    *editor);


G_DEFINE_TYPE_WITH_CODE (GimpMypaintBrushEditor, gimp_mypaint_brush_editor,
                         GIMP_TYPE_DATA_EDITOR,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_DOCKED,
                                                gimp_mypaint_brush_editor_docked_iface_init))

#define parent_class gimp_mypaint_brush_editor_parent_class

static GimpDockedInterface *parent_docked_iface = NULL;


static void
gimp_mypaint_brush_editor_class_init (GimpMypaintBrushEditorClass *klass)
{
  GObjectClass        *object_class = G_OBJECT_CLASS (klass);
  GimpDataEditorClass *editor_class = GIMP_DATA_EDITOR_CLASS (klass);

  object_class->constructed = gimp_mypaint_brush_editor_constructed;

  editor_class->set_data    = gimp_mypaint_brush_editor_set_data;
  editor_class->title       = _("Mypaint Brush Editor");
}

static void
gimp_mypaint_brush_editor_docked_iface_init (GimpDockedInterface *iface)
{
  parent_docked_iface = 
    reinterpret_cast<GimpDockedInterface*>(g_type_interface_peek_parent (iface));

  if (! parent_docked_iface)
    parent_docked_iface = 
      reinterpret_cast<GimpDockedInterface*>(g_type_default_interface_peek (GIMP_TYPE_DOCKED));

  iface->set_context = gimp_mypaint_brush_editor_set_context;
}

static void
gimp_mypaint_brush_editor_init (GimpMypaintBrushEditor *editor)
{

}

static void
gimp_mypaint_brush_editor_constructed (GObject *object)
{
  GimpDataEditor *data_editor = GIMP_DATA_EDITOR (object);
  GimpMypaintBrushEditor* editor = GIMP_MYPAINT_BRUSH_EDITOR (object);

  if (G_OBJECT_CLASS (parent_class)->constructed)
    G_OBJECT_CLASS (parent_class)->constructed (object);

  GtkWidget      *frame;
  GtkWidget      *hbox;
  GtkWidget      *label;
  GtkWidget      *box;
  GtkWidget      *scale;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (editor), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  data_editor->view = gimp_view_new_full_by_types (NULL,
                                                   GIMP_TYPE_VIEW,
                                                   GIMP_TYPE_MYPAINT_BRUSH,
                                                   MYPAINT_BRUSH_VIEW_SIZE,
                                                   MYPAINT_BRUSH_VIEW_SIZE, 0,
                                                   FALSE, FALSE, TRUE);
  gtk_widget_set_size_request (data_editor->view, -1, MYPAINT_BRUSH_VIEW_SIZE);
  gimp_view_set_expand (GIMP_VIEW (data_editor->view), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), data_editor->view);
  gtk_widget_show (data_editor->view);

  GList* setting_groups;
  setting_groups = mypaint_brush_get_setting_group_list();
  GimpContext* context;
  g_object_get(object, "context", &context, NULL);
  
  GtkWidget* brushsetting_vbox = gtk_vbox_new(FALSE, 0);

  editor->options_box = gtk_notebook_new();
  gtk_box_pack_start (GTK_BOX (editor), editor->options_box, FALSE, FALSE, 0);
  gtk_widget_show (editor->options_box);

  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(editor->options_box), FALSE);
  gtk_notebook_insert_page(GTK_NOTEBOOK(editor->options_box), brushsetting_vbox, label, 0);
//  gtk_notebook_insert_page(GTK_NOTEBOOK(editor->options_box), brushinputs, label, 1);
//  gtk_notebook_insert_page(GTK_NOTEBOOK(editor->options_box), brushicon, label, 2);
    

  for (GList* iter = setting_groups; iter; iter = iter->next) {
    MypaintBrushSettingGroup* group =
      (MypaintBrushSettingGroup*)iter->data;
//    self.visible_settings = self.visible_settings + group['settings']

    CString bold_title(g_strdup_printf("<b>%s</b>", group->display_name));
    GtkWidget* group_expander = gtk_expander_new(bold_title.ptr());
    gtk_expander_set_use_markup(GTK_EXPANDER(group_expander), TRUE);

    GtkWidget* table = gtk_table_new(3, g_list_length(group->setting_list), FALSE);

    if (strcmp(group->name, "basic") == 0)
	gtk_expander_set_expanded (GTK_EXPANDER(group_expander), TRUE);

    int count = 0;
    for (GList* iter2 = group->setting_list; iter2; iter2 = iter2->next, count ++) {
      MyPaintBrushSettings* s = (MyPaintBrushSettings*)iter2->data;
//      gtk_misc_set_alignment(GTK_MISC(l), 0, 0.5);

      GtkWidget* h = gimp_prop_spin_scale_new(G_OBJECT(context),s->internal_name,
                               s->displayed_name,
                               0.1, 1.0, 2);
      gtk_widget_set_tooltip_text(h, s->tooltip);

      GtkWidget* button = gimp_stock_button_new (GIMP_STOCK_RESET, NULL);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_image_set_from_stock (GTK_IMAGE (gtk_bin_get_child (GTK_BIN (button))),
                                GIMP_STOCK_RESET, GTK_ICON_SIZE_MENU);
      gtk_widget_show (button);
      gtk_widget_set_tooltip_text(button, _("Reset to default value"));
      
      //g_signal_connect_...(button, "clicked,...);
      GtkWidget* button2;
      if (s->constant) {
        button2 = gtk_label_new("");
	gtk_widget_set_tooltip_text(button2, _("No additional configuration"));
	gtk_misc_set_alignment(GTK_MISC(button2), 0.5, 0.5);
      } else {
        button2 = gtk_button_new_with_label("...");
	gtk_widget_set_tooltip_text(button2, _("Add input value mapping"));
        //g_signal_connect...(button2, "clicked", self.details_clicked_cb, adj, s)
      }
      gtk_table_attach(GTK_TABLE(table), h, 0, 1, count, count+1, 
                       GtkAttachOptions(GTK_EXPAND | GTK_FILL), 
                       GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
      gtk_table_attach(GTK_TABLE(table), button, 1, 2, count, count+1, 
                       GtkAttachOptions(GTK_EXPAND | GTK_FILL), 
                       GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
      gtk_table_attach(GTK_TABLE(table), button2, 2, 3, count, count+1, 
                       GtkAttachOptions(GTK_EXPAND | GTK_FILL), 
                       GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);

      gtk_container_add(GTK_CONTAINER(group_expander), table);
      gtk_box_pack_start(GTK_BOX(brushsetting_vbox) ,group_expander,FALSE,TRUE,0);
    }
  }

#if 0
  /*  mypaint_brush angle scale  */
  editor->angle_data =
    GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 180.0, 0.1, 1.0, 0.0));
  scale = gimp_spin_scale_new (editor->angle_data, _("Angle"), 1);
  gtk_box_pack_start (GTK_BOX (editor->options_box), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);

  gimp_help_set_help_data (scale, _("Percentage of width of mypaint_brush"), NULL);

  g_signal_connect (editor->angle_data, "value-changed",
                    G_CALLBACK (gimp_mypaint_brush_editor_update_mypaint_brush),
                    editor);
#endif
  
  gimp_docked_set_show_button_bar (GIMP_DOCKED (object), FALSE);
}

static void
gimp_mypaint_brush_editor_set_data (GimpDataEditor *editor,
                            GimpData       *data)
{
  GimpMypaintBrushEditor         *mypaint_brush_editor = GIMP_MYPAINT_BRUSH_EDITOR (editor);

  GimpContext* context;
  g_object_get(G_OBJECT(editor), "context", &context, NULL);

  GimpMypaintBrush* brush;
  brush = gimp_mypaint_options_get_current_brush (GIMP_MYPAINT_OPTIONS(context));

  GIMP_DATA_EDITOR_CLASS (parent_class)->set_data (editor, GIMP_DATA(brush));

  gimp_view_set_viewable (GIMP_VIEW (editor->view), GIMP_VIEWABLE (brush));

  gtk_widget_set_sensitive (mypaint_brush_editor->options_box,
                            editor->data_editable);
}

static void
gimp_mypaint_brush_editor_set_context (GimpDocked  *docked,
                               GimpContext *context)
{
  GimpDataEditor *data_editor = GIMP_DATA_EDITOR (docked);

  parent_docked_iface->set_context (docked, context);

  gimp_view_renderer_set_context (GIMP_VIEW (data_editor->view)->renderer,
                                  context);
}


/*  public functions  */

GtkWidget *
gimp_mypaint_brush_editor_new (GimpContext     *context,
                       GimpMenuFactory *menu_factory)
{
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (GIMP_IS_MYPAINT_OPTIONS(context), NULL);

  GimpMypaintOptions* options = NULL;
  GIMP_MYPAINT_OPTIONS(context);

  gpointer result = g_object_new (GIMP_TYPE_MYPAINT_BRUSH_EDITOR,
                       "menu-factory",    menu_factory,
                       "menu-identifier", "<MypaintBrushEditor>",
                       "ui-path",         "/mypaint_brush-editor-popup",
                       "data-factory",    context->gimp->mypaint_brush_factory,
                       "context",         context,
                       "data",            gimp_mypaint_options_get_current_brush (options),
                       NULL);
  return GTK_WIDGET(result);
}


/*  private functions  */

static void
gimp_mypaint_brush_editor_update_mypaint_brush (GtkAdjustment   *adjustment,
                                GimpMypaintBrushEditor *editor)
{
#if 0
  GimpMypaintBrushGenerated *mypaint_brush;
  gdouble             radius;
  gint                spikes;
  gdouble             hardness;
  gdouble             ratio;
  gdouble             angle;
  gdouble             spacing;

  if (! GIMP_IS_MYPAINT_BRUSH_GENERATED (GIMP_DATA_EDITOR (editor)->data))
    return;

  mypaint_brush = GIMP_MYPAINT_BRUSH_GENERATED (GIMP_DATA_EDITOR (editor)->data);

  radius   = gtk_adjustment_get_value (editor->radius_data);
  spikes   = ROUND (gtk_adjustment_get_value (editor->spikes_data));
  hardness = gtk_adjustment_get_value (editor->hardness_data);
  ratio    = gtk_adjustment_get_value (editor->aspect_ratio_data);
  angle    = gtk_adjustment_get_value (editor->angle_data);
  spacing  = gtk_adjustment_get_value (editor->spacing_data);

  if (radius   != gimp_mypaint_brush_generated_get_radius       (mypaint_brush) ||
      spikes   != gimp_mypaint_brush_generated_get_spikes       (mypaint_brush) ||
      hardness != gimp_mypaint_brush_generated_get_hardness     (mypaint_brush) ||
      ratio    != gimp_mypaint_brush_generated_get_aspect_ratio (mypaint_brush) ||
      angle    != gimp_mypaint_brush_generated_get_angle        (mypaint_brush) ||
      spacing  != gimp_mypaint_brush_get_spacing                (GIMP_MYPAINT_BRUSH (mypaint_brush)))
    {
      g_signal_handlers_block_by_func (mypaint_brush,
                                       gimp_mypaint_brush_editor_notify_mypaint_brush,
                                       editor);

      gimp_data_freeze (GIMP_DATA (mypaint_brush));
      g_object_freeze_notify (G_OBJECT (mypaint_brush));

      gimp_mypaint_brush_generated_set_radius       (mypaint_brush, radius);
      gimp_mypaint_brush_generated_set_spikes       (mypaint_brush, spikes);
      gimp_mypaint_brush_generated_set_hardness     (mypaint_brush, hardness);
      gimp_mypaint_brush_generated_set_aspect_ratio (mypaint_brush, ratio);
      gimp_mypaint_brush_generated_set_angle        (mypaint_brush, angle);
      gimp_mypaint_brush_set_spacing                (GIMP_MYPAINT_BRUSH (mypaint_brush), spacing);

      g_object_thaw_notify (G_OBJECT (mypaint_brush));
      gimp_data_thaw (GIMP_DATA (mypaint_brush));

      g_signal_handlers_unblock_by_func (mypaint_brush,
                                         gimp_mypaint_brush_editor_notify_mypaint_brush,
                                         editor);
    }
#endif
}

static void
gimp_mypaint_brush_editor_update_shape (GtkWidget       *widget,
                                GimpMypaintBrushEditor *editor)
{
#if 0
  GimpMypaintBrushGenerated *mypaint_brush;

  if (! GIMP_IS_MYPAINT_BRUSH_GENERATED (GIMP_DATA_EDITOR (editor)->data))
    return;

  mypaint_brush = GIMP_MYPAINT_BRUSH_GENERATED (GIMP_DATA_EDITOR (editor)->data);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      GimpMypaintBrushGeneratedShape shape;

      shape = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                  "gimp-item-data"));

      if (gimp_mypaint_brush_generated_get_shape (mypaint_brush) != shape)
        gimp_mypaint_brush_generated_set_shape (mypaint_brush, shape);
    }
#endif
}

static void
gimp_mypaint_brush_editor_notify_mypaint_brush (GimpMypaintBrush   *mypaint_brush,
                                GParamSpec           *pspec,
                                GimpMypaintBrushEditor      *editor)
{
#if 0
  GtkAdjustment *adj   = NULL;
  gdouble        value = 0.0;

  if (! strcmp (pspec->name, "shape"))
    {
      g_signal_handlers_block_by_func (editor->shape_group,
                                       gimp_mypaint_brush_editor_update_shape,
                                       editor);

      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (editor->shape_group),
                                       mypaint_brush->shape);

      g_signal_handlers_unblock_by_func (editor->shape_group,
                                         gimp_mypaint_brush_editor_update_shape,
                                         editor);

      adj   = editor->radius_data;
      value = mypaint_brush->radius;
    }
  else if (! strcmp (pspec->name, "radius"))
    {
      adj   = editor->radius_data;
      value = mypaint_brush->radius;
    }
  else if (! strcmp (pspec->name, "spikes"))
    {
      adj   = editor->spikes_data;
      value = mypaint_brush->spikes;
    }
  else if (! strcmp (pspec->name, "hardness"))
    {
      adj   = editor->hardness_data;
      value = mypaint_brush->hardness;
    }
  else if (! strcmp (pspec->name, "angle"))
    {
      adj   = editor->angle_data;
      value = mypaint_brush->angle;
    }
  else if (! strcmp (pspec->name, "aspect-ratio"))
    {
      adj   = editor->aspect_ratio_data;
      value = mypaint_brush->aspect_ratio;
    }
  else if (! strcmp (pspec->name, "spacing"))
    {
      adj   = editor->spacing_data;
      value = GIMP_MYPAINT_BRUSH (mypaint_brush)->spacing;
    }

  if (adj)
    {
      g_signal_handlers_block_by_func (adj,
                                       gimp_mypaint_brush_editor_update_mypaint_brush,
                                       editor);

      gtk_adjustment_set_value (adj, value);

      g_signal_handlers_unblock_by_func (adj,
                                         gimp_mypaint_brush_editor_update_mypaint_brush,
                                         editor);
    }
#endif
}
};
