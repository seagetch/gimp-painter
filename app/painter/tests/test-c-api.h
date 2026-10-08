/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_TEST_C_API_H
#define PAINTER_TEST_C_API_H
#include <glib-object.h>
G_BEGIN_DECLS
void painter_test_register (void);
int painter_test_c_callback (int value);
gboolean painter_test_cpp_roundtrip (int value, int *result, GError **error);
typedef enum
{
  PAINTER_TEST_EXCEPTION_TYPED,
  PAINTER_TEST_EXCEPTION_STANDARD,
  PAINTER_TEST_EXCEPTION_ALLOCATION,
  PAINTER_TEST_EXCEPTION_UNKNOWN
} PainterTestExceptionKind;
gboolean painter_test_cpp_exception (PainterTestExceptionKind kind,
                                     gboolean                 use_void_boundary,
                                     int                     *destroyed,
                                     GError                 **error);
G_END_DECLS
#endif
