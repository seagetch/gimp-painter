/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_PROCEDURE_ARGUMENTS_HPP
#define GIMP_FILTER_PROCEDURE_ARGUMENTS_HPP
#include "painter/filter-procedure.hpp"
#include <glib-object.h>
#include <array>
#include <memory>

typedef struct _GimpProcedure GimpProcedure;
typedef struct _GimpImage GimpImage;
typedef struct _GimpValueArray GimpValueArray;

namespace GimpPainter {
/* Core-only metadata. Schema and binder borrow the procedure and its immutable
 * specs: the caller's retained procedure must outlive both. The private helper
 * already keeps its ObjectRef through execution and output recovery. The
 * image-procedure context prefix is ABI; only subsequent inputs may reorder. */
class FilterParameterSchema
{
public:
  FilterParameterSchema (GimpProcedure *procedure, FilterProcedure route, bool hidden = true);
  GParamSpec *spec (const char *name) const;
  unsigned slot (const char *name) const;
  GimpProcedure *procedure () const { return procedure_; }
  unsigned size () const { return size_; }
private:
  GimpProcedure *procedure_;
  unsigned size_;
};

/* Restricted to the types used by the four admitted routes. Bounds and exact
 * types are checked before copying. These helpers never alter the input. */
bool filter_parameter_values_equal (const GValue& first, const GValue& second);
void filter_parameter_validate_copy (GParamSpec *spec, const GValue& value);

class FilterParameterBinder
{
public:
  explicit FilterParameterBinder (const FilterParameterSchema& schema);
  void set_enum (const char *name, GType type, gint value);
  void set_int (const char *name, gint value);
  void set_double (const char *name, gdouble value);
  void set_boolean (const char *name, bool value);
  void set_string (const char *name, const char *value);
  void set_image (const char *name, GimpImage *value);
  /* Elements are borrowed: caller's image/layer ObjectRefs must outlive
   * execute, plug-in wait and output recovery, independently of this box. */
  void set_drawables (const char *name, GObject *const *value, gsize count);
  void set_double_array (const char *name, const gdouble *value, gsize count);
  void set_int32_array (const char *name, const gint32 *value, gsize count);
  void assign (const char *name, const GValue& value);
  GimpValueArray *finish () const;
private:
  struct ValuesDelete { void operator() (GimpValueArray *value) const; };
  FilterParameterSchema schema_;
  std::unique_ptr<GimpValueArray, ValuesDelete> values_;
  std::array<bool, 9> assigned_ {};
};
}
#endif
