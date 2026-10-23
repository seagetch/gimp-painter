/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gimp-painter-error.h"
#include "gimp-painter-error.h"
#include "gimp-painter-binding.h"
#include "gimp-painter-binding.h"
#include "test-c-api.h"

static int callback_count;
static int callback_value;

int painter_test_c_callback (int value)
{
  ++callback_count;
  callback_value = value;
  return value * 2;
}

static void c_cpp_c_value (void)
{
  GError *error = NULL;
  int result = -1;

  callback_count = 0;
  callback_value = -1;
  g_assert_true (painter_test_cpp_roundtrip (7, &result, &error));
  g_assert_no_error (error);
  g_assert_cmpint (result, ==, 15);
  g_assert_cmpint (callback_count, ==, 1);
  g_assert_cmpint (callback_value, ==, 7);

  g_assert_true (painter_test_cpp_roundtrip (0, &result, &error));
  g_assert_no_error (error);
  g_assert_cmpint (result, ==, 1);
  g_assert_cmpint (callback_count, ==, 2);
  g_assert_cmpint (callback_value, ==, 0);
}

static void cpp_error_to_c (void)
{
  GError *error = NULL;
  int result = -1;

  callback_count = 0;
  callback_value = -1;
  g_assert_false (painter_test_cpp_roundtrip (-1, &result, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_EXCEPTION);
  g_assert_cmpstr (error->message, ==, "injected C++ failure");
  g_assert_cmpint (result, ==, 0);
  g_assert_cmpint (callback_count, ==, 0);
  g_assert_cmpint (callback_value, ==, -1);
  g_clear_error (&error);
  g_assert_null (error);
}

static void cpp_error_without_gerror (void)
{
  int result = -1;

  callback_count = 0;
  callback_value = -1;
  g_assert_false (painter_test_cpp_roundtrip (-1, &result, NULL));
  g_assert_cmpint (result, ==, 0);
  g_assert_cmpint (callback_count, ==, 0);
  g_assert_cmpint (callback_value, ==, -1);
}

static void cpp_exception_policy (gboolean use_void_boundary)
{
  const char *messages[] = {
    "injected typed failure", "injected standard failure", NULL,
    "Unknown C++ exception"
  };
  unsigned int continued = 0;

  for (int kind = PAINTER_TEST_EXCEPTION_TYPED;
       kind <= PAINTER_TEST_EXCEPTION_UNKNOWN; ++kind)
    {
      GError *error = NULL;
      int destroyed = -1;
      int code = kind == PAINTER_TEST_EXCEPTION_TYPED ?
                 GIMP_PAINTER_ERROR_CLOSED : GIMP_PAINTER_ERROR_EXCEPTION;

      g_assert_false (painter_test_cpp_exception (kind, use_void_boundary,
                                                &destroyed, &error));
      ++continued;
      g_assert_cmpint (destroyed, ==, 1);
      g_assert_error (error, GIMP_PAINTER_ERROR, code);
      if (messages[kind])
        g_assert_cmpstr (error->message, ==, messages[kind]);
      else
        g_assert_true (error->message && error->message[0]);
      g_clear_error (&error);
      g_assert_null (error);

      destroyed = -1;
      g_assert_false (painter_test_cpp_exception (kind, use_void_boundary,
                                                &destroyed, NULL));
      ++continued;
      g_assert_cmpint (destroyed, ==, 1);
    }
  g_assert_cmpuint (continued, ==, 8);
}

static void cpp_result_boundary_policy (void) { cpp_exception_policy (FALSE); }
static void cpp_void_boundary_policy (void) { cpp_exception_policy (TRUE); }

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/painter/interop/c-cpp-c-value", c_cpp_c_value);
  g_test_add_func ("/painter/interop/cpp-error-to-c", cpp_error_to_c);
  g_test_add_func ("/painter/interop/cpp-error-without-gerror", cpp_error_without_gerror);
  g_test_add_func ("/painter/interop/result-boundary-policy", cpp_result_boundary_policy);
  g_test_add_func ("/painter/interop/void-boundary-policy", cpp_void_boundary_policy);
  painter_test_register ();
  return g_test_run ();
}
