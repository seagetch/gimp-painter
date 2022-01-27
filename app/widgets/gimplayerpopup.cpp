/* GIMP-painter
 * GimpCellRendererPopup
 * Copyright (C) 2016  seagetch
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
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-impl.hpp"
#include "base/selectcase-utils.hpp"
#include <functional>

extern "C" {
#include <config.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include <gdk/gdkpixbuf.h>

#include "widgets-types.h"
#include "core/gimpdashpattern.h"
#include "core/gimpmarshal.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"

#include "pdb/gimppdb-query.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/gimppluginprocedure.h"

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "libgimpbase/gimpbase.h"

#include "gimp-intl.h"
#include "gimpwidgets-constructors.h"
#include "gimphelp-ids.h"
#include "gimpspinscale.h"
#include "gimpcolorpanel.h"

};

#include "base/glib-cxx-def-utils.hpp"

#include "gimpcellrendererpopup.h"
#include "popupper.h"
#include "gimplayerpopup.h"

#include "core/gimpfilterlayer.h"

using namespace GLib;

//////////////////////////////////////////////////////////////////////////////////////////////
class PopupWindowDecorator {
  enum {
    LABEL,
    VALUE,
    PIXBUF,
    NUM_ARGS
  };
  IObject<GimpLayer> layer;
  IObject<GtkWidget> mode_select;
  IObject<GtkWidget> filter_select;
  IObject<GtkWidget> filter_edit;
  CString            proc_name;
  Array              proc_args;

  GtkWidget* create_label_tree_view(const gchar* title, GtkTreeModel* model) {
    return with (gtk_tree_view_new_with_model(model), [&title] (auto tree_view) {
#if 0
      tree_view [gtk_tree_view_append_column] (
        gtk_tree_view_column_new_with_attributes (
            " ", gtk_cell_renderer_pixbuf_new(),
            "pixbuf", PIXBUF,
            NULL));
#endif
      tree_view [gtk_tree_view_append_column] (
        gtk_tree_view_column_new_with_attributes (
            title, gtk_cell_renderer_text_new(),
            "text", LABEL,
            NULL));
    });
  };

  void add_proc(IObject<GtkTreeStore> store, GtkTreeIter* iter, GtkTreeIter* parent, GimpPlugInProcedure* proc, const gchar* label = NULL) {
    const gchar* desc = label;
    const gchar* stock_id = NULL;
    GdkPixbuf* pixbuf = NULL;
    if (proc) {
      desc = ref(proc) [gimp_plug_in_procedure_get_label] ();
      stock_id = ref(proc) [gimp_plug_in_procedure_get_stock_id] ();
      pixbuf = ref(proc) [gimp_plug_in_procedure_get_pixbuf] ();
      gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
    }
    store [gtk_tree_store_append] (iter, parent);
    gtk_tree_store_set (GTK_TREE_STORE(store.ptr()), iter, VALUE, proc? GIMP_PROCEDURE(proc)->original_name: "", LABEL, _(desc), PIXBUF, pixbuf, -1);
  };

  bool is_allowed_proc_path(gchar* menu_path) {
    static const gchar* white_list_prefix[] = {
        "<Image>/Filters/",
        "<Image>/Colors/",
        NULL
    };
    static const gchar* black_list_prefix[] = {
        "<Image>/Filters/Animation",
        "<Image>/Filters/Render",
        "<Image>/Filters/Language",
        "<Image>/Filters/Web",
        "<Image>/Filters/Alpha to Logo",
        "<Image>/Filters/Combine",
        "<Image>/Filters/Decor",
        NULL
    };
    for (const gchar** i = white_list_prefix; *i; i ++) {
      if (strncmp(menu_path, *i, strlen(*i)) == 0) {
        bool result = true;
        for (const gchar** j = black_list_prefix; *j; j ++) {
          if (strncmp(menu_path, *j, strlen(*j)) == 0) {
            result = false;
            break;
          }
        }
        if (result)
          return true;
      }
    }
    return false;
  }

  GtkWidget* create_filter_list(IObject<GimpLayer> layer) {
    auto             store    = ref(gtk_tree_store_new(NUM_ARGS, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF));
    auto             pdb      = ref( layer [gimp_item_get_image] ()->gimp->pdb );
    gint             num_procs;
    gchar**          procs;
    gboolean query = pdb [gimp_pdb_query] (".*", ".*", ".*", ".*", ".*", ".*", ".*", &num_procs, &procs, NULL);

    IHashTable<gchar*, GtkTreeIter*> tree_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    for (int i = 0; i < num_procs; i ++) {
      gchar*         proc_name        = procs[i];
      GimpProcedure* proc             = pdb [gimp_pdb_lookup_procedure] (proc_name);

      g_free(proc_name);

      if (!proc || !GIMP_IS_PLUG_IN_PROCEDURE(proc)) continue;
      bool use_image = false, use_drawable = false;
      for (int j = 0; j < proc->num_args; j ++) {
        GParamSpec*  pspec         = proc->args[j];
        if (GIMP_IS_PARAM_SPEC_IMAGE_ID(pspec))
          use_image = true;
        if (GIMP_IS_PARAM_SPEC_DRAWABLE_ID(pspec))
          use_drawable = true;
      }
      if (!use_image || !use_drawable) continue;

      GimpPlugInProcedure* plugin_proc = GIMP_PLUG_IN_PROCEDURE(proc);
      auto menu_paths = ref<gchar*>(plugin_proc->menu_paths);
      for (auto path: menu_paths) {
        if (path && is_allowed_proc_path(path)) {

          // Registers procedure for filter selection list.
          StringList path_list    = g_strsplit(path, "/", -1);
          String           current_path = g_string_new("");
          GtkTreeIter*     parent       = NULL;

          // Traverses appropriate GtkTreeIter folder for menu_path
          for (int p = 0; path_list[p]; p ++) {
            if (p == 0) continue;
            current_path += '/';
            current_path += path_list[p];
            if (tree_map[current_path])
              parent = tree_map[current_path];
            else {
              GtkTreeIter* cur_iter = g_new0(GtkTreeIter, 1);
              add_proc(store, cur_iter, parent, NULL, path_list[p]);
              tree_map.insert(g_strdup(current_path), cur_iter);
              parent = cur_iter;
            }
          }

          // Registers filter
          GtkTreeIter proc_iter;
          add_proc(store, &proc_iter, parent, plugin_proc);
          break;
        }
      }

    }
    g_free(procs);

    filter_select = create_label_tree_view(_("Effects"), GTK_TREE_MODEL(store.ptr()));
    return filter_select;
  }

  void update_filter_edit(const gchar* proc_name) {
    auto           pdb  = ref( layer [gimp_item_get_image] ()->gimp->pdb );
    GimpProcedure* proc = pdb [gimp_pdb_lookup_procedure] (proc_name);

    if (!GIMP_IS_PLUG_IN_PROCEDURE(proc)) return;

    auto f            = FilterLayerInterface::cast(layer.ptr());
    auto plug_in_proc = ref(GIMP_PLUG_IN_PROCEDURE(proc));

    if (strcmp(f->get_procedure(), proc_name) == 0) {
      proc_args = f->get_procedure_args();
      g_print("%s->get_procedure_args() = \n", proc_name);
      if (proc_args) {
        auto i_proc_args = ref<GValue>(proc_args);
        for (auto gvalue : i_proc_args) {
          CopyValue v = gvalue;
          g_print("  type=%s\n", g_type_name(v.type()));
        }
      } else {
        g_print("  proc_args = NULL\n");
      }
    } else
      proc_args = NULL;
    if (!proc_args) {
      GValueArray* temp_args = plug_in_proc [gimp_procedure_get_arguments] ();
      proc_args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), temp_args->n_values);
      g_array_set_clear_func (proc_args, (GDestroyNotify) g_value_unset);
      g_array_append_vals(proc_args, temp_args->values, temp_args->n_values);
      g_value_array_free(temp_args);
    }

    for (auto widget : ref<GtkWidget*>(filter_edit [gtk_container_get_children] ())) {
      ref(widget) [gtk_widget_destroy] ();
    };

    with (filter_edit, [&](auto vbox) {
      GtkRequisition req = { 300, -1 };

      vbox.pack_start(false, true, 4) (
        gtk_label_new(""), [&](auto it) {
          it [gtk_widget_show] ();
          it [gtk_label_set_line_wrap] (TRUE);
          it [gtk_widget_set_size_request] (req.width, req.height);
          const gchar* label = plug_in_proc [gimp_plug_in_procedure_get_label] ();
          CString markup = g_markup_printf_escaped ("<span font_weight=\"bold\" size=\"larger\">%s</span>", label);
          it [gtk_label_set_markup] (markup.ptr());
        }
      ).pack_start(false, true, 0) (
        gtk_label_new(plug_in_proc [gimp_plug_in_procedure_get_blurb]()), [&](auto it) {
          it [gtk_widget_show] ();
          it [gtk_label_set_line_wrap] (TRUE);
          it [gtk_widget_set_size_request] (req.width, req.height);
        }
      ).pack_start(false, true, 3) (
        gtk_hseparator_new(), [&](auto it) {
          it [gtk_widget_show] ();
        }
      );

      IHashTable<gchar*, GimpFrame*> frames = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

      for (int j = 0; j < proc->num_args; j ++) {
        GParamSpec*  pspec         = proc->args[j];
        CString arg_name      = g_strdup(g_param_spec_get_name(pspec));
        CString arg_blurb     = g_strdup(pspec->_blurb);
        GType        arg_type      = G_PARAM_SPEC_TYPE(pspec);
        const gchar* arg_type_name = G_PARAM_SPEC_TYPE_NAME(pspec);

        g_print("param=%s [%s]\n   %s\n", arg_name.ptr(), arg_type_name, arg_blurb.ptr());

        static const gchar* GROUP_PATTERN  = "^<([^>]*)>";

        static const gchar* ENUM_PATTERN   = _("{ *(([^,]+ *,)* *[^,]+) *}");
        static const gchar* RANGE_PATTERN  = _("((-?\\d+(\\.\\d*)?)\\s*<=)|(<=\\s*(-?\\d+(\\.\\d*)?))");
        static const gchar* RANGE_PATTERN2 = _("\\[\\s*(-?\\d+(\\.\\d*)?)\\s*,\\s*(-?\\d+(\\.\\d*)?)\\s*\\]");
        static const gchar* RANGE_PATTERN3 = _("\\(\\s*(-?\\d+(\\.\\d*)?)\\s*(-|(to)|(\\.\\.))\\s*((-?\\d+(\\.\\d*)?)|(->))\\s*\\)");
        static const gchar* BOOL_PATTERN   = _("(\\(TRUE\\s*[/,]\\s*FALSE\\))|(\\(bool(ean)?\\))");
        static const gchar* BOOL_PATTERN2  = _("TRUE:([^,]+),?\\s*FALSE:([^,]+)");
        static const gchar* DEGREE_PATTERN = _("([Aa]ngle)|(\\((in )?degrees?\\))");
        static const gchar* PERCENTILE_PATTERN = _("(in %)|([Pp]ercent)");

        if ((GIMP_IS_PARAM_SPEC_IMAGE_ID(pspec) && strcmp(arg_name,"image") == 0) ||
            (GIMP_IS_PARAM_SPEC_DRAWABLE_ID(pspec) && strcmp(arg_name, "drawable") == 0) ||
//            GIMP_IS_PARAM_SPEC_ITEM_ID(pspec) ||
            strcmp(arg_name, "run-mode") == 0)
          continue;

        Definer<GtkWidget> box = vbox;

        auto group_pattern = Regex(GROUP_PATTERN);
        group_pattern.match(arg_blurb, [&](auto matched) {
          CString group = matched[1];
          if(!group) return;

          auto frame = ref(frames[group]);
          if (!frame) {
            frame = ref(gimp_frame_new(_(group)));

            vbox.pack_start(false, true, 0) (
              frame.ptr(), [] (auto fr) {
                fr [gtk_widget_show] ();
                fr.add(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), [](auto it) {
                  it [gtk_widget_show] ();
                });
              }
            );

            frames.insert(g_strdup(group), frame.ptr());
          }

          auto children = ref<GtkWidget*>( frame [gtk_container_get_children] () );
          box = children[0];
          arg_blurb = group_pattern.replace(arg_blurb, "");
        });

        // Double value
        if (G_IS_PARAM_SPEC_DOUBLE(pspec)) {
          GParamSpecDouble* dpspec         = G_PARAM_SPEC_DOUBLE(pspec);
          double            spec_min_value = dpspec->minimum;
          double            spec_max_value = dpspec->maximum;
          double            spec_default   = dpspec->default_value;
          double            max_value      = spec_min_value;
          double            min_value      = spec_max_value;
          auto i_proc_args = ref<GValue>(proc_args);
          double            cur_value      = g_value_get_double(&i_proc_args[j]);
          gchar*            desc           = NULL;

          regex_case(arg_blurb).
            when(RANGE_PATTERN, [&](auto matched){
              if (matched.count() > 4) {
                max_value = g_ascii_strtod(matched[5], NULL);
                g_print("max: %le\n", max_value);
              } else if (matched.count() > 1) {
                min_value = g_ascii_strtod(matched[2], NULL);
                g_print("min: %le\n", min_value);
              }
              desc = Regex(RANGE_PATTERN).replace(arg_blurb, "");
            }).
            when(RANGE_PATTERN2, [&](auto matched){
              min_value = g_ascii_strtod(matched[1], NULL);
              max_value = g_ascii_strtod(matched[3], NULL);
              g_print("min: %le , max: %le\n", min_value, max_value);
              desc = Regex(RANGE_PATTERN2).replace(arg_blurb, "");
            }).
            when(RANGE_PATTERN3, [&](auto matched){
              min_value = g_ascii_strtod(matched[1], NULL);
              if (!matched[9] || strlen(matched[9]) == 0)
                max_value = g_ascii_strtod(matched[7], NULL);
              g_print("min: %le , max: %le\n", min_value, max_value);
              desc = Regex(RANGE_PATTERN3).replace(arg_blurb, "");
            }).
            when(DEGREE_PATTERN, [&](auto matched) {
              min_value = 0;
              max_value = 360;
              g_print("matched=%s, degree\n", matched[0]);
              desc = Regex(RANGE_PATTERN3).replace(arg_blurb, "");
            }).
            when(PERCENTILE_PATTERN, [&](auto matched) {
              min_value = 0;
              max_value = 100;
              g_print("matched=%s, percentile\n", matched[0]);
            });
          bool is_infinity_range = false;
          if (max_value == spec_min_value) {
            is_infinity_range = true;
            max_value         = spec_max_value;
          }
          if (min_value == spec_max_value) {
            is_infinity_range = true;
            min_value         = spec_min_value;
          }
          if (!desc) desc = g_strdup(arg_blurb.ptr());

          g_print("range: %le - %le\n", min_value, max_value);

          if (is_infinity_range) {
            box.pack_start(false, true, 2) (
              gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), [&](auto hbox) {
                hbox [gtk_widget_show] ();
                hbox.pack_start(false, false, 0) (
                  gtk_label_new (_(desc)), [&](auto it) {
                    it [gtk_widget_show] ();
                    it [gtk_widget_set_tooltip_text] (_(desc));
                    it [gtk_widget_set_size_request] (200, -1);
                  }
                ).pack_end(true, true, 0) (
                  gtk_spin_button_new (
                    with (gtk_adjustment_new (cur_value, min_value, max_value, 1.0, 10.0, 0.0),[&](auto it) {

                      it.connect("value-changed", delegator(std::function<void(GtkWidget*)>([this,j](GtkWidget* o) {
                        auto i_proc_args = ref<GValue>(proc_args);
                        g_value_set_double(&i_proc_args[j], ref(o)["value"]);
                      })));

                    }), 1, 1),
                  [&desc](auto it) {
                    it [gtk_widget_show] ();
                  }
                );
              }
            );

          } else {
            box.pack_start(false, true, 2) (
              gimp_spin_scale_new (
                with (gtk_adjustment_new (cur_value, min_value, max_value, 1.0, 10.0, 0.0),[&](auto it) {

                  it.connect("value-changed", delegator(std::function<void(GtkWidget*)>([this,j](GtkWidget* o) {
                    auto i_proc_args = ref<GValue>(proc_args);
                    g_value_set_double(&i_proc_args[j], ref(o)["value"]);
                  })));

                }),
                _(desc), 1),
              [&desc](auto it) {
                it [gtk_widget_show] ();
                it [gtk_widget_set_tooltip_text] (_(desc));
              });
          }
          g_free(desc);

        // Int32 value
        } else if (GIMP_IS_PARAM_SPEC_INT32(pspec) || GIMP_IS_PARAM_SPEC_INT8(pspec)) {

          gchar*       desc           = NULL;
          bool         constructed    = false;
          bool         check          = false;
          int          spec_min_value = 0;
          int          spec_max_value = 0;
          int          spec_default   = 0;
          bool         is_int32       = GIMP_IS_PARAM_SPEC_INT32(pspec);
          if (is_int32) {
            auto ipspec    = G_PARAM_SPEC_INT(pspec);
            spec_min_value = ipspec->minimum;
            spec_max_value = ipspec->maximum;
            spec_default   = ipspec->default_value;
          } else {
            auto ipspec    = G_PARAM_SPEC_UINT(pspec);
            spec_min_value = ipspec->minimum;
            spec_max_value = ipspec->maximum;
            spec_default   = ipspec->default_value;
          }
          int          max_value      = spec_min_value;
          int          min_value      = spec_max_value;
          auto i_proc_args = ref<GValue>(proc_args);
          gint         cur_value      = g_value_get_int(&i_proc_args[j]);
          /*
          if (is_int32)
            cur_value = ref(proc_args->values[j]);
          else
            cur_value = ref(proc_args->values[j]);
          */
          regex_case(arg_blurb).

            when(ENUM_PATTERN, [&] (auto matched) {
              g_print("Matched_FULL=%s, matched = %d, count=%d\n", matched[0], (bool)matched, matched.count());
              if (!matched[0])
                return;

              StringList list =
                  g_regex_split_simple ("\\s*,\\s*", matched[1], GRegexCompileFlags(0), GRegexMatchFlags(0));

              int count = 0;
              for (gchar** w = list.ptr(); *w; w++, count ++);
              gchar** str_list = g_new0(gchar*, count + 1);
              GtkWidget* combo = NULL;

              for (int i = 0; i < count; i ++) {
                g_print("(%d): %s\n", i, list.ptr()[i]);
                gint32 value =0;
                gchar* label = NULL;
                regex_case(list.ptr()[i]).
                  when ("(.*) *\\(([0-9]+)\\)", [&](auto submatched) {
                    value = (gint32)g_ascii_strtoll(submatched[2], NULL, 10);
                    label = submatched[1];
                  }).
                  when ("(TRUE)|(FALSE)", [&] (auto submatched) {
                    check = true;
                  });
                if (!check) {
                  if (!combo) {
                    combo = gimp_int_combo_box_new(_(label), value, NULL);
                  } else {
                  gimp_int_combo_box_append(GIMP_INT_COMBO_BOX(combo),
                      GIMP_INT_STORE_VALUE, value,
                      GIMP_INT_STORE_LABEL, _(label),
                      -1);
                  }
                }
                g_free(label);
              }

              CString desc2     = Regex(ENUM_PATTERN).replace(arg_blurb, "");

              if (check) {
                box.pack_start(false, true, 2) (
                  gtk_check_button_new_with_label (desc2), [&](auto it) {
                    it [gtk_widget_show] ();
                    it [gtk_toggle_button_set_active] (cur_value);

                    it.connect("toggled", delegator(std::function<void(GtkWidget*)>([this,j](GtkWidget* o) {
                      auto i_proc_args = ref<GValue>(proc_args);
                      g_value_set_int(&i_proc_args[j], ref(o) [gtk_toggle_button_get_active] ());
                    })));

                  }
                );
              } else {
                box.pack_start(false, true, 2) (
                  gtk_label_new(desc2), [] (auto it) {
                    it [gtk_label_set_line_wrap] (TRUE);
                    it [gtk_misc_set_alignment] (0, 0);
                    it [gtk_widget_show] ();
                  }
                )(
                  combo, [&](auto it) {
                    it [gtk_widget_show] ();
                    it [gimp_int_combo_box_set_active] (cur_value);

                    it.connect("changed", delegator(std::function<void(GtkWidget*)>([this,j](GtkWidget* o) {
                      gint32 value;
                      ref(o) [gimp_int_combo_box_get_active] (&value);
                      auto i_proc_args = ref<GValue>(proc_args);
                      g_value_set_int(&i_proc_args[j], value);
                    })));

                  }
                );
              }
              constructed = true;

            }).
            when(BOOL_PATTERN, [&](auto matched){
              CString desc2 = Regex(BOOL_PATTERN).replace(arg_blurb, "");
              box.pack_start(false, true, 2) (
                gtk_check_button_new_with_label (desc2), [](auto it) {
                  it [gtk_widget_show] ();
                }
              );
              constructed = true;
            }).
            when(BOOL_PATTERN2, [&](auto matched){
              box.pack_start(false, true, 2) (
                gtk_label_new(arg_name), [] (auto it) {
                  it [gtk_label_set_line_wrap] (TRUE);
                  it [gtk_widget_show] ();
                }
              )(
                gimp_int_combo_box_new(matched[1], TRUE, matched[2], FALSE, NULL),
                [](auto it) {
                  it [gtk_widget_show] ();
                }
              );
              constructed = true;
            }).
            when(RANGE_PATTERN, [&](auto matched){
              if (matched.count() > 4) {
                max_value = (gint32)g_ascii_strtod(matched[5], NULL);
                g_print("max: %d\n", max_value);
              } else if (matched.count() > 1) {
                min_value = (gint32)g_ascii_strtod(matched[2], NULL);
                g_print("min: %d\n", min_value);
              }
              desc = Regex(RANGE_PATTERN).replace(arg_blurb, "");
            }).
            when(RANGE_PATTERN2, [&](auto matched){
              min_value = (gint32)g_ascii_strtod(matched[1], NULL);
              max_value = (gint32)g_ascii_strtod(matched[3], NULL);
              g_print("min: %d , max: %d\n", min_value, max_value);
              desc = Regex(RANGE_PATTERN2).replace(arg_blurb, "");
            }).
            when(RANGE_PATTERN3, [&](auto matched){
              min_value = (gint32)g_ascii_strtod(matched[1], NULL);
              if (!matched[9] || strlen(matched[9]) == 0)
                max_value = (gint32)g_ascii_strtod(matched[7], NULL);
              g_print("min: %d , max: %d\n", min_value, max_value);
              desc = Regex(RANGE_PATTERN3).replace(arg_blurb, "");
            }).
            when(PERCENTILE_PATTERN, [&](auto matched) {
              min_value = 0;
              max_value = 100;
              g_print("matched=%s, percentile\n", matched[0]);
            });

          if (!constructed) {
            bool is_infinity_range = false;
            if (max_value == spec_min_value) {
              is_infinity_range = true;
              max_value         = spec_max_value;
            }
            if (min_value == spec_max_value) {
              is_infinity_range = true;
              min_value         = spec_min_value;
            }

            if (!desc)
              desc = g_strdup(arg_blurb.ptr());

            g_print("range: %d - %d\n", min_value, max_value);

            if (is_infinity_range) {
              box.pack_start(false, true, 2) (
                gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), [&](auto hbox) {
                  hbox [gtk_widget_show] ();
                  hbox.pack_start(false, false, 0) (
                    gtk_label_new (_(desc)), [&](auto it) {
                      it [gtk_widget_show] ();
                      it [gtk_widget_set_tooltip_text] (_(desc));
                      it [gtk_widget_set_size_request] (200, -1);
                    }
                  ).pack_end(true, true, 0) (
                    gtk_spin_button_new (
                      with (gtk_adjustment_new (cur_value, min_value, max_value, 1.0, 10.0, 0.0),[&](auto it) {

                        it.connect("value-changed", delegator(std::function<void(GtkWidget*)>([this,j](GtkWidget* o) {
                          auto i_proc_args = ref<GValue>(proc_args);
                          g_value_set_int(&i_proc_args[j], ref(o)["value"]);
                        })));

                      }), 1, 0),
                    [&desc](auto it) {
                      it [gtk_widget_show] ();
                    }
                  );
                }
              );

            } else {
              box.pack_start (false, true, 2)(
                gimp_spin_scale_new (
                  with (gtk_adjustment_new (cur_value, min_value, max_value, 1.0, 10.0, 0.0),[&](auto it) {

                    it.connect("value-changed", delegator(std::function<void(GtkWidget*)>([this,j](GtkWidget* o) {
                      gdouble value = (gdouble)ref(o)["value"];
                      auto i_proc_args = ref<GValue>(proc_args);
                      g_value_set_int(&i_proc_args[j], (gint32)value);
                    })));

                  }), _(desc), 1),
                [&desc](auto it) {
                  it [gtk_widget_set_tooltip_text] (_(desc));
                  it [gtk_widget_show] ();
                }
              );
            }

            g_free(desc);
          }

        } else if (GIMP_IS_PARAM_SPEC_RGB(pspec)) {
          const GValue*  value = g_param_spec_get_default_value(pspec);
          GimpRGB* rgb = NULL;
          GimpRGB color;
          gimp_value_get_rgb(value, rgb);
          if (!rgb) {
            gimp_rgba_set (&color, 1.0, 1.0, 1.0, 1.0);
            rgb = &color;
          }
          box.pack_start (false, true, 0) (
            gtk_label_new(_(arg_blurb)), [&req](auto label) {
              label [gtk_widget_show] ();
              label [gtk_label_set_line_wrap] (TRUE);
              label [gtk_widget_set_size_request] (req.width, req.height);
            }
          )(
            gimp_color_panel_new (_(arg_blurb), rgb, GIMP_COLOR_AREA_SMALL_CHECKS, 128, 24), [](auto label) {
              label [gtk_widget_show] ();
            }
          );

        } else if (G_IS_PARAM_SPEC_STRING(pspec)) {
          box.pack_start (false, true, 0) (
            gtk_label_new(_(arg_blurb)), [&req](auto label) {
              label [gtk_widget_show] ();
              label [gtk_label_set_line_wrap] (TRUE);
              label [gtk_widget_set_size_request] (req.width, req.height);
            }
          )(
            gtk_entry_new(), [](auto label) {
              label [gtk_widget_show] ();
            }
          );

        } else if (GIMP_IS_PARAM_SPEC_INT8_ARRAY(pspec)) {
          box.pack_start (false, true, 0) (
            gtk_label_new(_(arg_blurb)), [&req](auto label) {
              label [gtk_widget_show] ();
              label [gtk_label_set_line_wrap] (TRUE);
              label [gtk_widget_set_size_request] (req.width, req.height);
            }
          )(
            gtk_label_new(_(arg_type_name)), [&req](auto label) {
              label [gtk_widget_show] ();
              label [gtk_label_set_line_wrap] (TRUE);
              label [gtk_widget_set_size_request] (req.width, req.height);
            }
          );

        } else{
          box.pack_start (false, true, 0) (
            gtk_label_new(_(arg_blurb)), [&req](auto label) {
              label [gtk_widget_show] ();
              label [gtk_label_set_line_wrap] (TRUE);
              label [gtk_widget_set_size_request] (req.width, req.height);
            }
          )(
            gtk_label_new(_(arg_type_name)), [&req](auto label) {
              label [gtk_widget_show] ();
              label [gtk_label_set_line_wrap] (TRUE);
              label [gtk_widget_set_size_request] (req.width, req.height);
            }
          );
        }

      }
    });
  }

  GtkWidget* create_filter_edit(IObject<GimpLayer> layer) {
    filter_edit = with (gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), [&](auto vbox) {
      vbox [gtk_widget_show] ();
    });
    return filter_edit;
  }

  GtkWidget* create_paint_mode_list() {
    static GEnumClass* klass = G_ENUM_CLASS(g_type_class_ref(GIMP_TYPE_LAYER_MODE_EFFECTS));
    auto               store = ref( gtk_tree_store_new(NUM_ARGS, G_TYPE_STRING, GIMP_TYPE_LAYER_MODE_EFFECTS, GDK_TYPE_PIXBUF) );

    auto group = [&](GtkTreeIter* parent, const gchar* label, auto f) {
      GtkTreeIter iter;
      store [gtk_tree_store_append] (&iter, parent);
      gtk_tree_store_set (GTK_TREE_STORE(store.ptr()), &iter, VALUE, -1, LABEL, label, PIXBUF, NULL, -1);
      f(&iter);
    };

    auto mode  = [&](GtkTreeIter* parent, GimpLayerModeEffects value) {
      GtkTreeIter  iter;
      GEnumValue*  v    = g_enum_get_value (klass, (gint) value);
      const gchar* desc = _(gimp_enum_value_get_desc (klass, v));
      g_print("PopupWindow::add_mode: desc=%s, value=%d\n", desc, (gint)value);
      store [gtk_tree_store_append] (&iter, parent);
      gtk_tree_store_set (GTK_TREE_STORE(store.ptr()), &iter, VALUE, value, LABEL, desc, PIXBUF, NULL, -1);
    };

    group(NULL, _("Normal"), [&](auto parent) {
      mode( parent, GIMP_NORMAL_MODE);
      mode( parent, GIMP_DISSOLVE_MODE);
    });
    group(NULL, _("Lighten"), [&](auto parent) {
      mode( parent, GIMP_LIGHTEN_ONLY_MODE);
      mode( parent, GIMP_SCREEN_MODE);
      mode( parent, GIMP_DODGE_MODE);
      mode( parent, GIMP_ADDITION_MODE);
    });
    group(NULL, _("Darken"), [&](auto parent) {
      mode( parent, GIMP_DARKEN_ONLY_MODE);
      mode( parent, GIMP_MULTIPLY_MODE);
      mode( parent, GIMP_BURN_MODE);
    });
    group(NULL, _("Emphasis"), [&](auto parent) {
      mode( parent, GIMP_OVERLAY_MODE);
      mode( parent, GIMP_SOFTLIGHT_MODE);
      mode( parent, GIMP_HARDLIGHT_MODE);
    });
    group(NULL, _("Inversion"), [&](auto parent) {
      mode( parent, GIMP_DIFFERENCE_MODE);
      mode( parent, GIMP_SUBTRACT_MODE);
      mode( parent, GIMP_GRAIN_EXTRACT_MODE);
      mode( parent, GIMP_GRAIN_MERGE_MODE);
      mode( parent, GIMP_DIVIDE_MODE);
    });
    group(NULL, _("Color"), [&](auto parent) {
      mode( parent, GIMP_HUE_MODE);
      mode( parent, GIMP_SATURATION_MODE);
      mode( parent, GIMP_COLOR_MODE);
      mode( parent, GIMP_VALUE_MODE);
    });
    group(NULL, _("Render"), [&](auto parent) {
      mode( parent, GIMP_ERASE_MODE);
      mode( parent, GIMP_REPLACE_MODE);
      mode( parent, GIMP_ANTI_ERASE_MODE);
    });
    group(NULL, _("Masking"), [&](auto parent) {
      mode( parent, GIMP_SRC_IN_MODE);
      mode( parent, GIMP_DST_IN_MODE);
      mode( parent, GIMP_SRC_OUT_MODE);
      mode( parent, GIMP_DST_OUT_MODE);
    });

    mode_select = create_label_tree_view(_("Mode"), GTK_TREE_MODEL(store.ptr()));
    return mode_select;
  };

public:

  PopupWindowDecorator() : layer(NULL), mode_select(NULL), filter_select(NULL), filter_edit(NULL)
  {
    g_print("PopupWindowDecorator::PopupWindowDecorator\n");
  }

  ~PopupWindowDecorator()
  {
    g_print("PopupWindowDecorator::~PopupWindowDecorator\n");
  }


  void create_view(GtkWidget* widget, GtkWidget** result, gpointer data) {
    PangoAttribute        *attr;

    g_print("PopupWindowDecorator::create_view\n");
    layer = GIMP_LAYER(data);
    g_print("layer=%p\n", layer);
    *result = with (gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), [&](auto vbox) {
      vbox.pack_start (true, false, 0) (
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), [&](auto hbox) {
          hbox [gtk_widget_show] ();

          /*  Opacity scale  */

          g_print("opacity adjustment\n");
          hbox.pack_start(true, true, 0) (
            gimp_spin_scale_new (
              with (gtk_adjustment_new (100.0, 0.0, 100.0, 1.0, 10.0, 0.0),[&](auto it) {
                it.set("value", this->layer [gimp_layer_get_opacity] () * 100.0);

                it.connect("value-changed", delegator(std::function<void(GtkAdjustment*)>([this](GtkAdjustment* o) {
//                  g_print("Update opacity: %d\n", (gint)ref(o)["value"]);
                  this->layer [gimp_layer_set_opacity] ((gdouble)ref(o)["value"] / 100.0, TRUE);
                  auto image = ref( this->layer [gimp_item_get_image] () );
                  image [gimp_image_flush] ();
                })));

              }),
              _("Opacity"), 1),
            [](auto it) {
              it [gimp_help_set_help_data] (NULL, GIMP_HELP_LAYER_DIALOG_OPACITY_SCALE);
            }
          ).pack_start (false, false, 0) (
            /*  Lock alpha toggle  */
            gtk_toggle_button_new(), [&] (auto it) {
              it [gtk_widget_show] ();
              it.connect("toggled", delegator(std::function<void(GtkWidget*)>([this](GtkWidget* o) {
                this->layer [gimp_layer_set_lock_alpha] ( ref(o) [gtk_toggle_button_get_active] (), TRUE );
              })));
              it [gimp_help_set_help_data] ( _("Lock alpha channel"), GIMP_HELP_LAYER_DIALOG_LOCK_ALPHA_BUTTON);

              auto icon_size = GTK_ICON_SIZE_BUTTON;
              it.add (gtk_image_new_from_stock (GIMP_STOCK_TRANSPARENCY, icon_size), [&](auto image) {
                image [gtk_widget_show] ();
              });
            }
          );
        }
      ).pack_start (true, true, 0) (

        // Main contents
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), [&](auto hbox) {
          hbox [gtk_widget_show] ();

          //  Paint mode menu
//          g_print("create paint_mode_menu\n");
          hbox.pack_start (true, true, 0) (gtk_scrolled_window_new(NULL, NULL), [&](auto it) {
            it [gtk_widget_show] ();
            it.add (this->create_paint_mode_list(), [this](auto it) {
              // Set attributes.
              it [gimp_help_set_help_data] (NULL, GIMP_HELP_LAYER_DIALOG_PAINT_MODE_MENU);
              it [gtk_tree_view_expand_all] ();
              it [gtk_widget_show] ();

              auto model     = ref(it [gtk_tree_view_get_model] ());
              auto selection = ref(it [gtk_tree_view_get_selection] ());

              // "changed" handler
              selection.connect("changed", delegator(std::function<void(GtkWidget*)>([this](auto o) {
                GtkTreeModel* model;
                GtkTreeIter iter;
                GimpLayerModeEffects effect;
                auto selection = ref(o);
                selection [gtk_tree_selection_get_selected] (&model, &iter);
                gtk_tree_model_get (model, &iter, VALUE, &effect, -1);
                if (effect > -1)
                  layer [gimp_layer_set_mode] (effect, TRUE);
                auto image = ref( layer [gimp_item_get_image] () );
                image [gimp_image_flush] ();
              })));

              // initial cursor placement.
              auto callback = guard(delegator(std::function<gboolean(GtkTreeModel*, GtkTreePath*, GtkTreeIter*)>(
                  [this, &it, /*&model,*/ &selection](auto model, auto path, auto iter)->gboolean
                  {
                    GimpLayerModeEffects effect;
                    gtk_tree_model_get (model, iter, VALUE, &effect, -1);
                    if (effect == layer [gimp_layer_get_mode] ()) {
                      selection [gtk_tree_selection_select_iter] (iter);
                      it [gtk_tree_view_scroll_to_cell] (path, NULL, TRUE, 0.5, 0.0);
                      return TRUE;
                    }
                    return FALSE;
                  })));
              model [gtk_tree_model_foreach] (GLib::strip_ref<decltype(*callback)>::type::callback, (gpointer)callback.ptr());

            });
            GtkRequisition req;
            vbox[gtk_widget_get_requisition](&req);

            req.width  = MAX(req.width, 150);
            req.height = MAX(req.height, 200);
            it [gtk_widget_set_size_request] (req.width, req.height);
          });

          if (FilterLayerInterface::is_instance(layer)) {
            auto filter_layer = FilterLayerInterface::cast(layer);

            // Filter configuration pane
            hbox.pack_start (true, true, 0) (
              gtk_box_new (GTK_ORIENTATION_VERTICAL, 0), [&](auto vbox2) {

                vbox2 [gtk_widget_show] ();
                vbox2.pack_start (true, true, 0) (
                  gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0), [&](auto hbox2) {
                    hbox2 [gtk_widget_show] ();

                    hbox2.pack_start (true, true, 0)(
                      // Filter selection list (boxed by scrolled-window)
                      gtk_scrolled_window_new(NULL, NULL), [&](auto it) {
                        it [gtk_widget_show] ();
                        GtkRequisition req;
                        vbox [gtk_widget_get_requisition] (&req);
                        req.width  = MAX(req.width, 200);
                        req.height = MAX(req.height, 300);
                        it [gtk_widget_set_size_request] (req.width, req.height);

                        it.add (this->create_filter_list(layer), [this](auto it) {
                          it [gtk_widget_show] ();
                          it [gtk_tree_view_expand_all] ();
                          auto selection = ref( it [gtk_tree_view_get_selection] () );

                          // "changed" handler
                          selection.connect("changed", delegator(std::function<void(GtkWidget*)>([this](auto widget) {
                            GtkTreeModel* model     = NULL;
                            GtkTreeIter   iter;
                            gchar*        proc_name = NULL;

                            auto selection = ref(widget);
                            selection [gtk_tree_selection_get_selected] (&model, &iter);
                            gtk_tree_model_get (model, &iter, VALUE, &proc_name, -1);
                            if (proc_name && strlen(proc_name) > 0) {
                              g_print("CHANGED: proc_name=%s\n", proc_name);
                              this->update_filter_edit(proc_name);
                            }
                            this->proc_name = proc_name;
                          })));

                        });
                      }
                    )(
                      // Filter option pane (boxed by scrolled-window)
                      gtk_scrolled_window_new(NULL, NULL), [&](auto it) {
                        it [gtk_widget_show] ();
                        GtkRequisition req;
                        vbox [gtk_widget_get_requisition] (&req);
                        req.width  = MAX(req.width, 300);
                        req.height = MAX(req.height, 300);
                        it [gtk_widget_set_size_request] (req.width, req.height);
                        it [gtk_scrolled_window_set_policy] (GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

                        it.add_with_viewport (this->create_filter_edit(layer), [&](auto it) {
                          GtkRequisition req = { 200, -1 };
                          it [gtk_widget_show] ();
                          it [gtk_widget_set_size_request] (req.width, req.height);
                        });
                      }
                    );
                  }
                ).pack_start (false, true, 0) (
                    gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0), [&](auto hbox2) {
                      hbox2 [gtk_widget_show] ();

                      hbox2.pack_start (true, true, 0)(
                        gtk_check_button_new_with_label ("Live update"), [](auto it) {
                          it [gtk_widget_show] ();
                        }
                      ).pack_start (false, true, 0)(
                        gtk_button_new_from_stock (GTK_STOCK_APPLY), [this](auto it) {
                          it [gtk_widget_show] ();
                          it.connect("clicked", delegator(std::function<void(GtkWidget*)>([this](auto o) {
//                            g_print("Apply filter settings...\n");
                            auto filter_layer = FilterLayerInterface::cast(this->layer);
                            filter_layer->set_procedure(this->proc_name, this->proc_args);
                          })));
                        }
                      );
                    }
                );
              }
            );

            const gchar* proc_name = filter_layer->get_procedure();

            auto model     = ref(filter_select [gtk_tree_view_get_model] ());
            auto selection = ref(filter_select [gtk_tree_view_get_selection] ());
            {
              auto callback = guard(delegator(std::function<gboolean(GtkTreeModel*, GtkTreePath*, GtkTreeIter*)>(
                  [&](auto model, auto path, auto iter)->gboolean
                  {
                    gchar* iter_proc_name;
                    gtk_tree_model_get (model, iter, VALUE, &iter_proc_name, -1);
                    CString name_holder = iter_proc_name;
                    if (iter_proc_name && strcmp(proc_name, iter_proc_name) == 0) {
                      selection [gtk_tree_selection_select_iter] (iter);
                      filter_select [gtk_tree_view_scroll_to_cell] (path, NULL, TRUE, 0.5, 0.0);
                      return TRUE;
                    }
                    return FALSE;
                  })));
              gtk_tree_model_foreach (model, GLib::strip_ref<decltype(*callback)>::type::callback, (gpointer)callback.ptr());
            }
            if (strlen(proc_name)) {
              // FIXME: select filter_list
              this->update_filter_edit(proc_name);
            }

          }

        }
      );
      vbox [gtk_widget_show_all] ();

    });
  };
};
//////////////////////////////////////////////////////////////////////////////////////////////
LayerPopupWindow::LayerPopupWindow() {

};
LayerPopupWindow::~LayerPopupWindow() {

}
void LayerPopupWindow::create_view(GtkWidget* widget, GtkWidget** result, gpointer data) {
    g_print("PopupWindow::create_view\n");

    auto decor = new PopupWindowDecorator();
    decor->create_view(widget, result, data);
    decorator(*result, decor);
}