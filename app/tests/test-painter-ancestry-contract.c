/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "core/core-types.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpviewable.h"

/* Only supply a child container.  The ancestry vfunc, signal and depth updates
 * are all inherited unchanged from GimpViewable.  No image or GUI is needed. */
typedef struct
{
  GimpViewable   parent_instance;
  GimpContainer *children;
} AncestryNode;

typedef GimpViewableClass AncestryNodeClass;

GType ancestry_node_get_type (void);
G_DEFINE_TYPE (AncestryNode, ancestry_node, GIMP_TYPE_VIEWABLE)

static GimpContainer *
ancestry_node_get_children (GimpViewable *viewable)
{
  return ((AncestryNode *) viewable)->children;
}

static void
ancestry_node_finalize (GObject *object)
{
  g_clear_object (&((AncestryNode *) object)->children);
  G_OBJECT_CLASS (ancestry_node_parent_class)->finalize (object);
}

static void
ancestry_node_class_init (AncestryNodeClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = ancestry_node_finalize;
  GIMP_VIEWABLE_CLASS (klass)->get_children = ancestry_node_get_children;
}

static void
ancestry_node_init (AncestryNode *node)
{
  node->children = gimp_list_new (GIMP_TYPE_VIEWABLE, FALSE);
}

static GimpViewable *
node_new (void)
{
  return g_object_new (ancestry_node_get_type (), NULL);
}

static void
node_add (GimpViewable *parent,
          GimpViewable *child)
{
  g_assert_true (gimp_container_add (gimp_viewable_get_children (parent),
                                     GIMP_OBJECT (child)));
  gimp_viewable_set_parent (child, parent);
}

typedef struct
{
  GimpViewable *viewable;
  GimpViewable *parent;
  gint          depth;
  guint         calls;
} Observation;

static void
observe_ancestry (GimpViewable *viewable,
                  Observation  *observation)
{
  g_assert_true (viewable == observation->viewable);
  /* Ordinary handlers must see the new identity and depth synchronously. */
  g_assert_true (gimp_viewable_get_parent (viewable) == observation->parent);
  g_assert_cmpint (gimp_viewable_get_depth (viewable), ==, observation->depth);
  observation->calls++;
}

static void
same_parent_is_silent (void)
{
  GimpViewable *parent = node_new ();
  GimpViewable *child = node_new ();
  Observation  observation = { child, NULL, 0, 0 };

  g_signal_connect (child, "ancestry-changed",
                    G_CALLBACK (observe_ancestry), &observation);
  gimp_viewable_set_parent (child, NULL);
  g_assert_cmpuint (observation.calls, ==, 0);

  observation.parent = parent;
  observation.depth = 1;
  gimp_viewable_set_parent (child, parent);
  g_assert_cmpuint (observation.calls, ==, 1);
  gimp_viewable_set_parent (child, parent);
  gimp_viewable_set_parent (child, parent);
  g_assert_cmpuint (observation.calls, ==, 1);

  observation.parent = NULL;
  observation.depth = 0;
  gimp_viewable_set_parent (child, NULL);
  g_assert_cmpuint (observation.calls, ==, 2);
  gimp_viewable_set_parent (child, NULL);
  g_assert_cmpuint (observation.calls, ==, 2);
  g_object_unref (child);
  g_object_unref (parent);
}

static void
parent_identity_changes_at_equal_depth (void)
{
  GimpViewable *first = node_new ();
  GimpViewable *second = node_new ();
  GimpViewable *child = node_new ();
  Observation  observation = { child, first, 1, 0 };
  GSignalQuery query = { 0 };

  g_signal_query (g_signal_lookup ("ancestry-changed", GIMP_TYPE_VIEWABLE),
                  &query);
  g_assert_cmpuint (query.signal_id, !=, 0);
  g_assert_cmpuint (query.return_type, ==, G_TYPE_NONE);
  g_assert_cmpuint (query.n_params, ==, 0);
  g_assert_true ((query.signal_flags & G_SIGNAL_RUN_FIRST) != 0);

  g_signal_connect (child, "ancestry-changed",
                    G_CALLBACK (observe_ancestry), &observation);
  gimp_viewable_set_parent (child, first);
  g_assert_cmpuint (observation.calls, ==, 1);
  observation.parent = second;
  gimp_viewable_set_parent (child, second);
  g_assert_cmpuint (observation.calls, ==, 2);
  observation.parent = first;
  gimp_viewable_set_parent (child, first);
  g_assert_cmpuint (observation.calls, ==, 3);
  observation.parent = NULL;
  observation.depth = 0;
  gimp_viewable_set_parent (child, NULL);
  g_assert_cmpuint (observation.calls, ==, 4);

  g_object_unref (child);
  g_object_unref (second);
  g_object_unref (first);
}

typedef struct
{
  Observation nodes[4];
  guint       completed;
} SubtreeObservation;

static void
observe_completed_subtree (GimpViewable      *viewable,
                           SubtreeObservation *subtree)
{
  guint i;

  g_assert_true (viewable == subtree->nodes[0].viewable);
  subtree->completed++;
  /* RUN_FIRST propagation finishes every descendant before an ordinary
   * handler on the moved ancestor runs. Do not prescribe sibling order. */
  for (i = 0; i < G_N_ELEMENTS (subtree->nodes); i++)
    {
      Observation *node = &subtree->nodes[i];

      g_assert_true (gimp_viewable_get_parent (node->viewable) == node->parent);
      g_assert_cmpint (gimp_viewable_get_depth (node->viewable), ==, node->depth);
      g_assert_cmpuint (node->calls, ==, subtree->completed);
    }
}

static void
subtree_depth_propagates (void)
{
  GimpViewable *root = node_new ();
  GimpViewable *destination = node_new ();
  GimpViewable *branch = node_new ();
  GimpViewable *child = node_new ();
  GimpViewable *sibling = node_new ();
  GimpViewable *grandchild = node_new ();
  GimpViewable *unrelated = node_new ();
  SubtreeObservation subtree = {
    { { branch, NULL, 0, 0 }, { child, branch, 1, 0 },
      { sibling, branch, 1, 0 }, { grandchild, child, 2, 0 } }, 0
  };
  Observation untouched = { unrelated, NULL, 0, 0 };
  guint i;

  node_add (root, destination);
  node_add (branch, child);
  node_add (branch, sibling);
  node_add (child, grandchild);
  for (i = 0; i < G_N_ELEMENTS (subtree.nodes); i++)
    g_signal_connect (subtree.nodes[i].viewable, "ancestry-changed",
                      G_CALLBACK (observe_ancestry), &subtree.nodes[i]);
  g_signal_connect (branch, "ancestry-changed",
                    G_CALLBACK (observe_completed_subtree), &subtree);
  g_signal_connect (unrelated, "ancestry-changed",
                    G_CALLBACK (observe_ancestry), &untouched);

  /* Move an existing two-generation subtree two levels deeper. The direct
   * parents of descendants remain unchanged, but they must still be notified. */
  subtree.nodes[0].parent = destination;
  for (i = 0; i < G_N_ELEMENTS (subtree.nodes); i++)
    subtree.nodes[i].depth += 2;
  gimp_viewable_set_parent (branch, destination);
  g_assert_cmpuint (subtree.completed, ==, 1);
  gimp_viewable_set_parent (branch, destination);
  for (i = 0; i < G_N_ELEMENTS (subtree.nodes); i++)
    g_assert_cmpuint (subtree.nodes[i].calls, ==, 1);

  subtree.nodes[0].parent = NULL;
  for (i = 0; i < G_N_ELEMENTS (subtree.nodes); i++)
    subtree.nodes[i].depth -= 2;
  gimp_viewable_set_parent (branch, NULL);
  g_assert_cmpuint (subtree.completed, ==, 2);
  g_assert_cmpuint (untouched.calls, ==, 0);
  g_assert_cmpint (gimp_viewable_get_depth (unrelated), ==, 0);
  g_assert_null (gimp_viewable_get_parent (unrelated));

  g_object_unref (unrelated);
  g_object_unref (grandchild);
  g_object_unref (sibling);
  g_object_unref (child);
  g_object_unref (branch);
  g_object_unref (destination);
  g_object_unref (root);
}

static void
ancestry_does_not_own_parent_or_child (void)
{
  GimpViewable *parent = node_new ();
  GimpViewable *child = node_new ();
  gpointer     weak_parent = parent;
  gpointer     weak_child = child;

  g_object_add_weak_pointer (G_OBJECT (parent), &weak_parent);
  g_object_add_weak_pointer (G_OBJECT (child), &weak_child);
  gimp_viewable_set_parent (child, parent);
  /* Setting ancestry alone does not add a child-container ownership ref. */
  g_assert_cmpint (gimp_container_get_n_children (
                    gimp_viewable_get_children (parent)), ==, 0);
  g_object_unref (child);
  g_assert_null (weak_child);
  g_assert_nonnull (weak_parent);
  g_object_unref (parent);
  g_assert_null (weak_parent);

  parent = node_new ();
  child = node_new ();
  weak_parent = parent;
  weak_child = child;
  g_object_add_weak_pointer (G_OBJECT (parent), &weak_parent);
  g_object_add_weak_pointer (G_OBJECT (child), &weak_child);
  node_add (parent, child);
  g_object_unref (child);
  g_assert_nonnull (weak_child);
  /* The container owns the child, while its ancestry backlink is borrowed:
   * releasing the sole parent ref must destroy both, without a ref cycle. */
  g_object_unref (parent);
  g_assert_null (weak_parent);
  g_assert_null (weak_child);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/painter-ancestry/same-parent-silent", same_parent_is_silent);
  g_test_add_func ("/painter-ancestry/equal-depth-parent-identity", parent_identity_changes_at_equal_depth);
  g_test_add_func ("/painter-ancestry/descendant-depth", subtree_depth_propagates);
  g_test_add_func ("/painter-ancestry/ownership", ancestry_does_not_own_parent_or_child);
  return g_test_run ();
}
