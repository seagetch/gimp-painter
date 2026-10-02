/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_CLONE_LAYER_H__
#define __GIMP_CLONE_LAYER_H__

#include "gimplayer.h"

G_BEGIN_DECLS

#define GIMP_TYPE_CLONE_LAYER (gimp_clone_layer_get_type ())
#define GIMP_CLONE_LAYER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CLONE_LAYER, GimpCloneLayer))
#define GIMP_IS_CLONE_LAYER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CLONE_LAYER))
#define GIMP_CLONE_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CLONE_LAYER, GimpCloneLayerClass))

struct _GimpCloneLayer
{
  GimpLayer parent_instance;
  /* Non-owning C construction status; all implementation state is in BindingStore. */
  gboolean binding_failed;
};
struct _GimpCloneLayerClass { GimpLayerClass parent_class; };

typedef enum
{
  GIMP_CLONE_SOURCE_NONE,
  GIMP_CLONE_SOURCE_PENDING,
  GIMP_CLONE_SOURCE_LIVE,
  GIMP_CLONE_SOURCE_EXPIRED
} GimpCloneSourceState;

GimpCloneSourceState gimp_clone_layer_get_source_state (GimpCloneLayer *);
GType       gimp_clone_layer_get_type           (void) G_GNUC_CONST;
GimpLayer * gimp_clone_layer_new                (GimpImage *, GimpLayer *, gint, gint,
                                                 const gchar *, gdouble, GimpLayerMode);
void        gimp_clone_layer_set_source         (GimpCloneLayer *, GimpLayer *);
GimpLayer * gimp_clone_layer_get_source          (GimpCloneLayer *);
void        gimp_clone_layer_set_source_by_name (GimpCloneLayer *, const gchar *);
/* Copy of pending legacy name, or live source name, or last known source name.
 * Never resolves a name or dereferences an expired source; caller frees. */
gchar *     gimp_clone_layer_dup_source_name    (GimpCloneLayer *);
gboolean    gimp_clone_layer_set_source_full    (GimpCloneLayer *, GimpLayer *, GError **);
gboolean    gimp_clone_layer_set_source_name_full (GimpCloneLayer *, const gchar *, GError **);

G_END_DECLS
#endif
