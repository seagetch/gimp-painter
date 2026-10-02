/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gimp-painter-error.h"
#include "gimp-painter-error.h"
#include "gimp-painter-binding.h"
#include "gimp-painter-binding.h"
#include "test-c-api.h"

int painter_test_c_callback (int value)
{
  return value * 2;
}

int main (int argc, char **argv)
{
  GError *error = NULL;
  int result = -1;
  g_test_init (&argc, &argv, NULL);
  g_assert_true (painter_test_cpp_roundtrip (7, &result, &error));
  g_assert_no_error (error);
  g_assert_cmpint (result, ==, 15);
  g_assert_false (painter_test_cpp_roundtrip (-1, &result, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_EXCEPTION);
  g_assert_cmpint (result, ==, 0);
  g_clear_error (&error);
  painter_test_register ();
  return g_test_run ();
}
