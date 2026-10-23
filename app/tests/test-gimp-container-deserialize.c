/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <string.h>
#include <gio/gio.h>
#include <gegl.h>

#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "core/gimpobject.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"

/* A real GimpConfig child keeps these tests independent of resource factories,
 * GUI startup and profile migration. Counters also detect leaked failed reads. */
typedef struct
{
  GimpObject parent_instance;
  gint       value;
} ContainerTestChild;

typedef GimpObjectClass ContainerTestChildClass;

static guint created;
static guint finalized;

GType container_test_child_get_type (void);
G_DEFINE_TYPE_WITH_CODE (ContainerTestChild, container_test_child,
                         GIMP_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_CONFIG, NULL))

static void
child_set_property (GObject      *object,
                    guint         property_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
  if (property_id == 1)
    ((ContainerTestChild *) object)->value = g_value_get_int (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
child_get_property (GObject    *object,
                    guint       property_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
  if (property_id == 1)
    g_value_set_int (value, ((ContainerTestChild *) object)->value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
child_finalize (GObject *object)
{
  finalized++;
  G_OBJECT_CLASS (container_test_child_parent_class)->finalize (object);
}

static void
container_test_child_class_init (ContainerTestChildClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = child_set_property;
  object_class->get_property = child_get_property;
  object_class->finalize = child_finalize;
  GIMP_CONFIG_PROP_INT (object_class, 1, "value", NULL, NULL,
                        G_MININT, G_MAXINT, 0, GIMP_PARAM_STATIC_STRINGS);
}

static void
container_test_child_init (ContainerTestChild *child)
{
  child->value = 0;
  created++;
}

static GimpContainer *
new_list (gboolean unique)
{
  GimpContainer *list = gimp_list_new (container_test_child_get_type (), unique);

  g_object_set (list, "append", TRUE, NULL);
  g_assert_cmpint (gimp_container_get_unique_names (list), ==, unique);
  return list;
}

static GimpObject *
add_child (GimpContainer *list,
           const gchar   *name,
           gint           value)
{
  GimpObject *child = g_object_new (container_test_child_get_type (),
                                   "name", name, "value", value, NULL);

  g_assert_true (gimp_container_add (list, child));
  g_object_unref (child);
  return child; /* borrowed from the strong container */
}

static void
assert_child (GimpContainer *list,
              gint           index,
              const gchar   *name,
              gint           value)
{
  GimpObject *child = gimp_container_get_child_by_index (list, index);
  gint        actual;

  g_assert_nonnull (child);
  g_assert_cmpstr (gimp_object_get_name (child), ==, name);
  g_object_get (child, "value", &actual, NULL);
  g_assert_cmpint (actual, ==, value);
}

static void
read_text (GimpContainer *list,
           const gchar   *text,
           gboolean       success)
{
  GError   *error = NULL;
  gboolean  result;

  result = gimp_config_deserialize_string (GIMP_CONFIG (list), text, -1,
                                            NULL, &error);
  g_assert_cmpint (result, ==, success);
  if (success)
    g_assert_no_error (error);
  else
    g_assert_nonnull (error);
  g_clear_error (&error);
}

static void
same_name_roundtrip (gconstpointer data)
{
  gboolean       unique = GPOINTER_TO_INT (data);
  GimpContainer *source = new_list (FALSE);
  GimpContainer *target = new_list (unique);
  GimpObject    *existing = add_child (target, "same", 7);
  GimpObject    *first = add_child (source, "same", 11);
  GimpObject    *second = add_child (source, "same", 29);
  gchar         *text;
  guint          before = created;

  g_assert_true (first != second);
  text = gimp_config_serialize_to_string (GIMP_CONFIG (source), NULL);
  g_assert_nonnull (text);
  read_text (target, text, TRUE);
  g_assert_true (gimp_container_get_child_by_index (target, 0) == existing);
  if (unique)
    {
      g_assert_cmpuint (created, ==, before);
      g_assert_cmpint (gimp_container_get_n_children (target), ==, 1);
      assert_child (target, 0, "same", 29);
    }
  else
    {
      GimpObject *copy1 = gimp_container_get_child_by_index (target, 1);
      GimpObject *copy2 = gimp_container_get_child_by_index (target, 2);

      g_assert_cmpuint (created, ==, before + 2);
      g_assert_cmpint (gimp_container_get_n_children (target), ==, 3);
      g_assert_true (copy1 != copy2 && copy1 != existing && copy2 != existing);
      g_assert_true (copy1 != first && copy2 != second);
      assert_child (target, 0, "same", 7);
      assert_child (target, 1, "same", 11);
      assert_child (target, 2, "same", 29);
    }
  /* Releasing the serialization source must not invalidate the read result. */
  g_object_unref (source);
  assert_child (target, unique ? 0 : 2, "same", 29);
  g_object_unref (target);
  g_free (text);
  g_assert_cmpuint (created, ==, finalized);
}

static void
empty_names (gconstpointer data)
{
  gboolean       unique = GPOINTER_TO_INT (data);
  GimpContainer *source = new_list (FALSE);
  GimpContainer *target = new_list (unique);
  gchar         *text;

  add_child (source, "", 13);
  add_child (source, "", 31);
  text = gimp_config_serialize_to_string (GIMP_CONFIG (source), NULL);
  g_assert_nonnull (text);
  read_text (target, text, TRUE);
  g_assert_cmpint (gimp_container_get_n_children (target), ==, unique ? 1 : 2);
  /* The string parser returns NULL for a quoted empty string. The container
   * normalizes that parser result back to an empty (non-NULL) child name. */
  assert_child (target, 0, "", unique ? 31 : 13);
  if (! unique)
    {
      assert_child (target, 1, "", 31);
      g_assert_true (gimp_container_get_child_by_index (target, 0) !=
                     gimp_container_get_child_by_index (target, 1));
    }
  g_free (text);
  g_object_unref (source);
  g_object_unref (target);
  g_assert_cmpuint (created, ==, finalized);
}

static void
null_name_rejected (gconstpointer data)
{
  GimpContainer *source = new_list (FALSE);
  GimpContainer *target = new_list (GPOINTER_TO_INT (data));
  gchar         *text;
  guint          before;

  add_child (source, "good", 13);
  add_child (source, NULL, 31);
  text = gimp_config_serialize_to_string (GIMP_CONFIG (source), NULL);
  g_assert_nonnull (text);
  g_assert_nonnull (strstr (text, "NULL"));
  before = created;
  /* Both original and native readers require a quoted name. A bare NULL
   * emitted by the writer is rejected before constructing that child. */
  read_text (target, text, FALSE);
  g_assert_cmpuint (created, ==, before + 1);
  g_assert_cmpint (gimp_container_get_n_children (target), ==, 1);
  assert_child (target, 0, "good", 13);
  g_free (text);
  g_object_unref (source);
  g_object_unref (target);
  g_assert_cmpuint (created, ==, finalized);
}

static void
failed_child_read (gconstpointer data)
{
  gboolean       unique = GPOINTER_TO_INT (data);
  GimpContainer *list = new_list (unique);
  GimpObject    *existing = add_child (list, "same", 41);
  guint          before_created = created;
  guint          before_finalized = finalized;

  read_text (list, "(ContainerTestChild \"same\" (value \"invalid\"))", FALSE);
  g_assert_cmpint (gimp_container_get_n_children (list), ==, 1);
  g_assert_true (gimp_container_get_child_by_index (list, 0) == existing);
  assert_child (list, 0, "same", 41);
  g_assert_cmpuint (created, ==, before_created + (unique ? 0 : 1));
  g_assert_cmpuint (finalized, ==, before_finalized + (unique ? 0 : 1));
  /* Failure is not transactional: a preceding complete child remains added. */
  read_text (list, "(ContainerTestChild \"good\" (value 17))"
                   "(ContainerTestChild \"bad\" (value \"invalid\"))", FALSE);
  g_assert_cmpint (gimp_container_get_n_children (list), ==, 2);
  assert_child (list, 1, "good", 17);
  g_assert_null (gimp_container_get_child_by_name (list, "bad"));
  g_object_unref (list);
  g_assert_cmpuint (created, ==, finalized);
}

static void
strong_child_lifetime (void)
{
  GimpContainer *list = new_list (FALSE);
  GimpObject    *child;
  gpointer       weak;

  read_text (list, "(ContainerTestChild \"owned\" (value 23))", TRUE);
  child = gimp_container_get_child_by_index (list, 0);
  weak = child;
  g_object_add_weak_pointer (G_OBJECT (child), &weak);
  g_object_ref (child);
  g_assert_true (gimp_container_remove (list, child));
  g_object_unref (list);
  g_assert_true (weak == child);
  g_assert_cmpstr (gimp_object_get_name (child), ==, "owned");
  g_object_unref (child);
  g_assert_null (weak);
  g_assert_cmpuint (created, ==, finalized);
}

static void
weak_child_lifetime (void)
{
  GimpContainer *list = gimp_list_new_weak (container_test_child_get_type (), FALSE);
  GimpObject    *child;
  gpointer       weak;

  read_text (list, "(ContainerTestChild \"unowned\" (value 37))", TRUE);
  child = gimp_container_get_child_by_index (list, 0);
  g_assert_nonnull (child);
  weak = child;
  g_object_add_weak_pointer (G_OBJECT (child), &weak);
  /* A weak container leaves the deserializer's initial ref to the caller. */
  g_object_unref (child);
  g_assert_null (weak);
  g_assert_cmpint (gimp_container_get_n_children (list), ==, 0);
  g_object_unref (list);
  g_assert_cmpuint (created, ==, finalized);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_data_func ("/container-deserialize/nonunique-roundtrip", GINT_TO_POINTER (FALSE), same_name_roundtrip);
  g_test_add_data_func ("/container-deserialize/unique-reuse", GINT_TO_POINTER (TRUE), same_name_roundtrip);
  g_test_add_data_func ("/container-deserialize/nonunique-empty", GINT_TO_POINTER (FALSE), empty_names);
  g_test_add_data_func ("/container-deserialize/unique-empty", GINT_TO_POINTER (TRUE), empty_names);
  g_test_add_data_func ("/container-deserialize/nonunique-failure", GINT_TO_POINTER (FALSE), failed_child_read);
  g_test_add_data_func ("/container-deserialize/unique-failure", GINT_TO_POINTER (TRUE), failed_child_read);
  g_test_add_data_func ("/container-deserialize/nonunique-null-rejected", GINT_TO_POINTER (FALSE), null_name_rejected);
  g_test_add_data_func ("/container-deserialize/unique-null-rejected", GINT_TO_POINTER (TRUE), null_name_rejected);
  g_test_add_func ("/container-deserialize/strong-lifetime", strong_child_lifetime);
  g_test_add_func ("/container-deserialize/weak-lifetime", weak_child_lifetime);
  return g_test_run ();
}
