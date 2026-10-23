#include "config.h"
#include <glib.h>
#include <girepository.h>
#include "libgimp/gimp.h"
#include "plug-ins/script-fu/libscriptfu/script-fu-enums.h"
#include "plug-ins/script-fu/libscriptfu/tinyscheme/scheme-private.h"
typedef struct { const gchar *name; gint value; } NamedConstant;
static const NamedConstant script_constants[] =
{
  /* Historical gimp-painter short names denote byte-compatible operators,
   * not the distinct modern ERASE/REPLACE or opt-in float operators. */
  { "ERASE",         GIMP_LAYER_MODE_PAINTER_ERASE      },
  { "REPLACE",       GIMP_LAYER_MODE_PAINTER_REPLACE    },
  { "ANTI-ERASE",    GIMP_LAYER_MODE_PAINTER_ANTI_ERASE },
  { "SRC-IN",        GIMP_LAYER_MODE_PAINTER_SRC_IN     },
  { "DST-IN",        GIMP_LAYER_MODE_PAINTER_DST_IN     },
  { "SRC-OUT",       GIMP_LAYER_MODE_PAINTER_SRC_OUT    },
  { "DST-OUT",       GIMP_LAYER_MODE_PAINTER_DST_OUT    },

  /* Useful values from libgimpbase/gimplimits.h */
  { "MIN-IMAGE-SIZE", GIMP_MIN_IMAGE_SIZE },
  { "MAX-IMAGE-SIZE", GIMP_MAX_IMAGE_SIZE },
  /* Note conversion to int, loss of precision: 0.005=>0 */
  { "MIN-RESOLUTION", (gint) GIMP_MIN_RESOLUTION },
  { "MAX-RESOLUTION", GIMP_MAX_RESOLUTION },

  /* Useful misc stuff */
  { "TRUE",           TRUE  },
  { "FALSE",          FALSE },

  /* Builtin units */
  { "UNIT-PIXEL",     GIMP_UNIT_PIXEL },
  { "UNIT-INCH",      GIMP_UNIT_INCH  },
  { "UNIT-MM",        GIMP_UNIT_MM    },
  { "UNIT-POINT",     GIMP_UNIT_POINT },
  { "UNIT-PICA",      GIMP_UNIT_PICA  },

  /* Script-Fu types */

  /* Arg types. */
  { "SF-IMAGE",       SF_IMAGE      },
  { "SF-DRAWABLE",    SF_DRAWABLE   },
  { "SF-LAYER",       SF_LAYER      },
  { "SF-CHANNEL",     SF_CHANNEL    },
  { "SF-VECTORS",     SF_VECTORS    },
  { "SF-COLOR",       SF_COLOR      },
  { "SF-TOGGLE",      SF_TOGGLE     },
  { "SF-STRING",      SF_STRING     },
  { "SF-FILENAME",    SF_FILENAME   },
  { "SF-DIRNAME",     SF_DIRNAME    },
  { "SF-ADJUSTMENT",  SF_ADJUSTMENT },
  { "SF-FONT",        SF_FONT       },
  { "SF-PATTERN",     SF_PATTERN    },
  { "SF-BRUSH",       SF_BRUSH      },
  { "SF-GRADIENT",    SF_GRADIENT   },
  { "SF-OPTION",      SF_OPTION     },
  { "SF-PALETTE",     SF_PALETTE    },
  { "SF-TEXT",        SF_TEXT       },
  { "SF-ENUM",        SF_ENUM       },
  { "SF-DISPLAY",     SF_DISPLAY    },

  /* PDB procedure drawable_arity, i.e. sensitivity.
   * Used with script-fu-register-filter.
   *
   * This declares the arity of the algorithm,
   * and not the signature of the PDB procedure.
   * Since v3, PDB procedures that are image procedures,
   * take a container of drawables.
   * This only describes how many drawables the container *should* hold.
   *
   * For calls invoked by a user, this describes
   * how many drawables the user is expected to select,
   * which disables/enables the menu item for the procedure.
   *
   * Procedures are also called from other procedures.
   * A call from another procedure may in fact
   * pass more drawables than declared for drawable_arity.
   * That is a programming error on behalf of the caller.
   * A well-written callee that is passed more drawables than declared
   * should return an error instead of processing any of the drawables.
   *
   * Similarly for fewer than declared.
   */

  /* Requires two drawables. Often an operation between them, yielding a new drawable */
  { "SF-TWO-OR-MORE-DRAWABLE",   SF_TWO_OR_MORE_DRAWABLE  },
  /* Often processed independently, sequentially, with side effects on the drawables. */
  { "SF-ONE-OR-MORE-DRAWABLE",   SF_ONE_OR_MORE_DRAWABLE  },
  /* Requires exactly one drawable. */
  { "SF-ONE-DRAWABLE",     SF_ONE_DRAWABLE    },

  /* For SF-ADJUSTMENT */
  { "SF-SLIDER",      SF_SLIDER     },
  { "SF-SPINNER",     SF_SPINNER    },

  { NULL, 0 }
};
static void
ts_init_enum (scheme     *sc,
              GIEnumInfo *info,
              const char *namespace)
{
  int n_values;

  n_values = g_enum_info_get_n_values ((GIEnumInfo *) info);
  for (int j = 0; j < n_values; j++)
    {
      GIValueInfo *value_info;
      const char  *c_identifier;
      char        *scheme_name;
      int         int_value;
      pointer     symbol;

      value_info = g_enum_info_get_value (info, j);

      /* Get name & value. Normally, we would use the actual name of the
       * GIBaseInfo here, but that would break bw-compatibility */
      c_identifier = g_base_info_get_attribute ((GIBaseInfo *) value_info, "c:identifier");
      if (c_identifier == NULL)
        {
          g_warning ("Problem in the GIR file: enum value without \"c:identifier\"!");
          g_base_info_unref ((GIBaseInfo *) value_info);
          continue;
        }

      /* Scheme-ify the name */
      if (g_strcmp0 (namespace, "Gimp") == 0)
        {
          /* Skip the GIMP prefix for GIMP enums */
          if (g_str_has_prefix (c_identifier, "GIMP_"))
            c_identifier += strlen ("GIMP_");
        }
      else
        {
          /* Other namespaces: skip non-prefixed symbols, to prevent clashes */
          if (g_ascii_strncasecmp (c_identifier, namespace, strlen (namespace)) != 0)
            {
              g_base_info_unref ((GIBaseInfo *) value_info);
              continue;
            }
        }
      scheme_name = g_strdelimit (g_strdup (c_identifier), "_", '-');
      int_value = g_value_info_get_value (value_info);

      /* Register the symbol */
      symbol = sc->vptr->mk_symbol (sc, scheme_name);
      sc->vptr->scheme_define (sc, sc->global_env, symbol,
                               sc->vptr->mk_integer (sc, int_value));
      sc->vptr->setimmutable (symbol);

      g_free (scheme_name);
      g_base_info_unref ((GIBaseInfo *) value_info);
    }
}

static void
ts_init_enums (scheme       *sc,
               GIRepository *repo,
               const char   *namespace)
{
  int i, n_infos;

  n_infos = g_irepository_get_n_infos (repo, namespace);
  for (i = 0; i < n_infos; i++)
    {
      GIBaseInfo *info;

      info = g_irepository_get_info (repo, namespace, i);
      if (GI_IS_ENUM_INFO (info))
        {
          ts_init_enum (sc, (GIEnumInfo *) info, namespace);
        }

      g_base_info_unref (info);
    }
}

static int failures;
static void check(scheme *sc,const char *expr) {
 sc->retcode=0; scheme_load_string(sc,expr);
 if(sc->retcode){fprintf(stderr,"FAILED %s\n",expr); failures++;}
}
int main(void){
 scheme *sc=scheme_init_new();g_assert_nonnull(sc);
 scheme_set_output_port_file(sc,stdout);
 GIRepository *repo=g_irepository_get_default();GError *error=NULL;
 g_assert_nonnull(g_irepository_require(repo,"Gimp","3.0",0,&error));
 ts_init_enums(sc,repo,"Gimp");
check(sc,"(if (defined? 'ERASE) (error \"alias collision\") #t)");
check(sc,"(if (defined? 'REPLACE) (error \"alias collision\") #t)");
check(sc,"(if (defined? 'ANTI-ERASE) (error \"alias collision\") #t)");
check(sc,"(if (defined? 'SRC-IN) (error \"alias collision\") #t)");
check(sc,"(if (defined? 'DST-IN) (error \"alias collision\") #t)");
check(sc,"(if (defined? 'SRC-OUT) (error \"alias collision\") #t)");
check(sc,"(if (defined? 'DST-OUT) (error \"alias collision\") #t)");
  for (int i = 0; script_constants[i].name != NULL; ++i)
    {
      pointer symbol;

      symbol = sc->vptr->mk_symbol (sc, script_constants[i].name);
      sc->vptr->scheme_define (sc, sc->global_env, symbol,
                               sc->vptr->mk_integer (sc,
                                                     script_constants[i].value));
      sc->vptr->setimmutable (symbol);
    }

check(sc,"(if (= ERASE 64) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= REPLACE 65) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= ANTI-ERASE 66) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= SRC-IN 67) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= DST-IN 68) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= SRC-OUT 69) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= DST-OUT 70) #t (error \"wrong legacy alias\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-ERASE 64) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-REPLACE 65) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-ANTI-ERASE 66) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-SRC-IN 67) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-DST-IN 68) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-SRC-OUT 69) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-PAINTER-DST-OUT 70) #t (error \"wrong GI identity\"))");
check(sc,"(if (= LAYER-MODE-ERASE 58) #t (error \"modern mode changed\"))");
check(sc,"(if (= LAYER-MODE-REPLACE 62) #t (error \"modern mode changed\"))");

 printf("aliases=7 enum-identities=7 no-GI-alias-collisions=7 modern-identities=2 failures=%d\n",failures);
 scheme_deinit(sc);g_free(sc);return failures?1:0;
}
