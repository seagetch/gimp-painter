/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * pdb-cxx-utils
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
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
#ifndef APP_PDB_PDB_CXX_UTILS_HPP_
#define APP_PDB_PDB_CXX_UTILS_HPP_

extern "C" {
#include "config.h"

#include <string.h>
#include <gegl.h>

#include "core/core-types.h"
#include "core/core-enums.h"

#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "core/gimp.h"
#include "core/gimpparamspecs.h"
#include "core/gimpimage.h"
#include "core/gimpprogress.h"
#include "core/gimpdrawable.h"
#include "core/gimpitem.h"
#include "core/gimpcontext.h"
#include "plug-in/gimptemporaryprocedure.h"
#include "plug-in/gimppluginprocedure.h"
}

#include "base/scopeguard.hpp"
#include "base/glib-cxx-utils.hpp"

////////////////////////////////////////////////////////////////////////
// Argument Configurators

class ItemArgConfigurator {
public:
  GimpDrawable*  drawable;
  GimpImage*     image;
  Gimp*          gimp;
  GimpPDB*       pdb;
  GimpContext*   context;
  GimpObject*    display;

  bool configure_args(GimpProcedure* procedure, GValueArray* args, GimpItem* item) {
    drawable = GIMP_DRAWABLE(item);
    image    = gimp_item_get_image(GIMP_ITEM(drawable));
    gimp     = image->gimp;
    pdb      = gimp->pdb;
    context  = gimp_get_user_context(gimp);
    display  = GIMP_OBJECT(gimp_context_get_display(context));

    for (int i = 0; i < procedure->num_args; i ++) {
      // Overwrite context related arguments.
      GParamSpec* pspec = procedure->args[i];
      if (strcmp(g_param_spec_get_name(pspec),"run-mode") == 0) {
        g_value_set_int(&args->values[i], GIMP_RUN_NONINTERACTIVE);

      } else if (GIMP_IS_PARAM_SPEC_IMAGE_ID(pspec)) {
        if (image)
          gimp_value_set_image (&args->values[i], image);

      } else if (GIMP_IS_PARAM_SPEC_DRAWABLE_ID(pspec)) { // FIXME.
        if (drawable)
          gimp_value_set_drawable (&args->values[i], drawable);

      } else if (GIMP_IS_PARAM_SPEC_ITEM_ID(pspec)) {
        if (item)
          gimp_value_set_item (&args->values[i], item);

      }

    }
    return true;
  }

};

////////////////////////////////////////////////////////////////////////
// Executors

class PDBAsyncExecutor {
public:
  void execute(GimpProcedure* procedure, Gimp* gimp, GimpContext* context,
               GimpProgress* progress, GValueArray* args, GimpObject* display)
  {
    gimp_procedure_execute_async (procedure, gimp, context,
                                  progress, args, display, NULL);
  }
};


class PDBSyncExecutor {
public:
  GValueArray* result;
  PDBSyncExecutor() : result(NULL) { };
  ~PDBSyncExecutor() {
    if (result)
      g_value_array_free(result);
  }

  void execute(GimpProcedure* procedure, Gimp* gimp, GimpContext* context,
               GimpProgress* progress, GValueArray* args, GimpObject* display)
  {
    result = gimp_procedure_execute (procedure, gimp, context,
                                     progress, args, NULL);
  }
};


template<class ArgConfigurator, class PDBExecutor>
class ProcedureRunnerImpl {
protected:
  // Attributes of procedure.
  GimpProcedure* procedure;
  GimpProgress*  progress;
  GValueArray*   args;
  bool           ignore_undos;

  // Running status of procedure.
  bool           running;
  bool           preserved;
  GMutex         mutex;

  ArgConfigurator arg_conf;
  PDBExecutor     executor;

  // Running context of procedure.

public:
  ProcedureRunnerImpl(GimpProcedure* proc, GimpProgress* prog, bool _ignore_undos = true) :
    procedure(proc), progress(prog), args(NULL), ignore_undos(_ignore_undos), running(false), preserved(false)
  {
    g_mutex_init(&mutex);
  };

  ~ProcedureRunnerImpl() {
    if (running)
      gimp_progress_cancel(progress);
    if (args) {
      g_value_array_free(args);
      args = NULL;
    }
  }

  bool is_running() { return running; };
  ArgConfigurator* get_arg_configurator() { return &arg_conf; }
  PDBExecutor*     get_executor()         { return &executor; }

  bool setup_args(GLib::IHashTable<const gchar*, GValue*>* arg_tables) {
    if (!args)
      args = gimp_procedure_get_arguments (procedure);

    if (!arg_tables)
      return true;
    for (int i = 0; i < procedure->num_args; i ++) {
      GParamSpec* pspec = procedure->args[i];
      GValue* value = (arg_tables)? (*arg_tables)[g_param_spec_get_name(pspec)] : NULL;
      if (value) {
        g_value_copy(value, &args->values[i]);
      } else {
        GValue v_str = G_VALUE_INIT;
        g_value_init(&v_str, G_TYPE_STRING);
        g_value_transform(g_param_spec_get_default_value(pspec), &v_str);
        g_print("FilterLayer::setup_args: %s: %s = %s\n", g_param_spec_get_name(pspec), g_type_name(G_PARAM_SPEC_VALUE_TYPE(pspec)), g_value_get_string(&v_str) );
        g_value_copy(g_param_spec_get_default_value(pspec), &args->values[i]);
      }
    }
    return true;
  }

  bool preserve() {
    GLib::synchronized locker(&mutex);
    bool result;
    if (running || preserved)
      result = false;
    else {
      preserved = true;
      result = true;
    }
    return result;
  }

  bool cancel() {
    GLib::synchronized locker(&mutex);
    preserved = false;
  }

  template<typename... ContextArgs>
  bool run(ContextArgs... cargs) {
    if (running || !procedure) {
      cancel();
      return false;
    }

    // Gathering contexts
    GError*        error   = NULL;
    gint           n_args  = 0;

    if (!args)
      setup_args(NULL);

    if (!arg_conf.configure_args(procedure, args, cargs...)) {
      cancel();
      return false;
    }

    if (GIMP_IS_PLUG_IN_PROCEDURE(procedure) ) {
      GimpPlugInProcedure* plug_in_proc = GIMP_PLUG_IN_PROCEDURE(procedure);
      bool prev_ignore_undos = plug_in_proc->ignore_undos;
      if (ignore_undos)
        plug_in_proc->ignore_undos = TRUE;
      executor.execute(procedure, arg_conf.gimp, arg_conf.context, progress, args, arg_conf.display);
      if (ignore_undos)
        plug_in_proc->ignore_undos = prev_ignore_undos;
    } else
      executor.execute(procedure, arg_conf.gimp, arg_conf.context, progress, args, arg_conf.display);
    //gimp_image_flush (image);

    {
      GLib::synchronized locker(&mutex);
      running = true;
      if (preserved)
        preserved = false;
    }

    return true;
  }


  void stop() {
    if (running)
      gimp_progress_cancel(progress);
  }

  void on_end() {
    running = false;
  }

  const char* get_procedure_name() {
    return (procedure)? procedure->original_name : "";
  }

  GValue get_arg(int index) {
    if (!procedure || index >= procedure->num_args)
      return G_VALUE_INIT;
    setup_args(NULL);
    GValue b;
    g_value_init (&b, G_TYPE_STRING);
    g_value_transform (&args->values[index], &b);
    g_print("get_arg(%d):%s\n", index, g_value_get_string(&b));
    return args->values[index];
  }

  GValueArray* get_args() {
    setup_args(NULL);
    return g_value_array_copy(args);
  }

  void set_arg(int index, GValue value) {
    if (!procedure || index >= procedure->num_args)
      return;
    setup_args(NULL);
    g_value_transform(&value, &args->values[index]);
  }

};

using ProcedureRunner = ProcedureRunnerImpl<ItemArgConfigurator, PDBAsyncExecutor>;


#endif /* APP_PDB_PDB_CXX_UTILS_HPP_ */
