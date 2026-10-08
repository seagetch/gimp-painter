/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_LAYER_ARGUMENTS_HPP
#define GIMP_FILTER_LAYER_ARGUMENTS_HPP
#include "../painter/gimp-painter-visibility.h"
/* Included after the core/GimpValueArray C declarations. Main-thread only. */
#include "painter/resources.hpp"
#include "painter/object-ref.hpp"
#include <memory>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Persistent object arguments are descriptors plus weak links, never GValues
 * owning images/layers. This prevents image -> layer -> argument -> image
 * cycles without losing the reference type or the recorded core ID. */
struct FilterArgumentReference
{
  explicit FilterArgumentReference (GObject *object, GType declared_type)
    : type (object ? G_OBJECT_TYPE (object) : declared_type),
      had_object (object != nullptr),
      id (GIMP_IS_ITEM (object) ? gimp_item_get_id (GIMP_ITEM (object)) :
          GIMP_IS_IMAGE (object) ? gimp_image_get_id (GIMP_IMAGE (object)) : 0),
      target (ObjectRef<GObject>::retain (object)) {}
  GType type;
  bool had_object;
  gint64 id;
  WeakRef<GObject> target;
};
/* Bulk imported scalar copies must be recoverable. XCF may retain the
 * original typed model as opaque when native materialization is unavailable. */
inline gpointer try_argument_copy (gconstpointer data, gsize size)
{
  if (size && !data) throw std::runtime_error ("Invalid imported scalar buffer");
  gpointer result = size ? g_try_malloc (size) : nullptr;
  if (size && !result) throw std::bad_alloc ();
  if (size) std::memcpy (result, data, size);
  return result;
}
inline gchar *try_argument_string (const gchar *text)
{
  if (!text) return nullptr;
  const gsize size = std::strlen (text);
  if (size == G_MAXSIZE) throw std::bad_alloc ();
  return static_cast<gchar *> (try_argument_copy (text, size + 1));
}
inline void copy_imported_argument_scalar (const GValue *source, GValue *target)
{
  const GType type = G_VALUE_TYPE (source);
  if (type == G_TYPE_STRING)
    g_value_take_string (target, try_argument_string (g_value_get_string (source)));
  else if (type == G_TYPE_STRV)
    {
      auto **input = static_cast<gchar **> (g_value_get_boxed (source));
      if (!input) return;
      gsize count = 0;
      while (input[count])
        { if (count == G_MAXSIZE / sizeof (gchar *) - 1) throw std::bad_alloc (); ++count; }
      std::unique_ptr<gchar *, decltype (&g_strfreev)> copy (g_try_new0 (gchar *, count + 1), g_strfreev);
      if (!copy) throw std::bad_alloc ();
      for (gsize i = 0; i < count; ++i) copy.get ()[i] = try_argument_string (input[i]);
      g_value_take_boxed (target, copy.release ());
    }
  else if (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY)
    {
      const auto *input = static_cast<const GimpArray *> (g_value_get_boxed (source));
      if (!input) return;
      std::unique_ptr<guint8, decltype (&g_free)> copy (
        static_cast<guint8 *> (try_argument_copy (input->data, input->length)), g_free);
      auto *array = gimp_array_new (copy.get (), input->length, TRUE);
      array->static_data = FALSE; copy.release ();
      g_value_take_boxed (target, array);
    }
  else
    /* Fixed scalars and immutable GBytes/GVariant references need no bulk copy. */
    g_value_copy (source, target);
}
class FilterArguments
{
  struct Argument
  {
    Value value;
    std::vector<FilterArgumentReference> objects;
    std::shared_ptr<const FilterArguments> nested;
    bool null_container = false;
  };
public:
  explicit FilterArguments (const GimpValueArray *args, unsigned depth = 0)
  { std::size_t remaining = 65536; initialize_values (args, depth, remaining); }
  FilterArguments (const GimpValueArray *args, unsigned depth, std::size_t& remaining)
  { initialize_values (args, depth, remaining); }
private:
  void initialize_values (const GimpValueArray *args, unsigned depth, std::size_t& remaining)
  {
    if (depth > 32) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Nested filter arguments exceed the limit");
    const auto n = gimp_value_array_length (args);
    if (n < 0 || std::size_t (n) > remaining) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Too many filter arguments");
    remaining -= n;
    arguments_.reserve (n);
    for (int i = 0; i < n; ++i)
      {
        const GValue *value = gimp_value_array_index (args, i);
        Argument arg;
        arg.value = Value (G_VALUE_TYPE (value));
        if (G_VALUE_HOLDS_OBJECT (value))
          {
            if (!remaining) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Too many object references");
            --remaining;
            arg.objects.emplace_back (G_OBJECT (g_value_get_object (value)), G_VALUE_TYPE (value));
          }
        else if (GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (value))
          {
            auto **array = static_cast<GObject **> (g_value_get_boxed (value));
            arg.null_container = array == nullptr;
            for (std::size_t j = 0; array && array[j]; ++j)
              {
                if (!remaining) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Too many object references");
                --remaining;
                arg.objects.emplace_back (array[j], G_TYPE_OBJECT);
              }
          }
        else if (GIMP_VALUE_HOLDS_VALUE_ARRAY (value))
          {
            auto *array = static_cast<const GimpValueArray *> (g_value_get_boxed (value));
            if (array) arg.nested = std::make_shared<FilterArguments> (array, depth + 1, remaining);
          }
        else
          {
            /* Unknown opaque boxed/pointer values cannot safely be retained:
             * they might own UI objects or be borrowed. The caller can keep the
             * complete raw definition with execution_args=null instead. */
            const auto type = G_VALUE_TYPE (value);
            if (G_VALUE_HOLDS_POINTER (value) || G_VALUE_HOLDS_PARAM (value) ||
                (G_VALUE_HOLDS_BOXED (value) && type != G_TYPE_BYTES && type != G_TYPE_STRV &&
                 type != GIMP_TYPE_ARRAY && type != GIMP_TYPE_INT32_ARRAY && type != GIMP_TYPE_DOUBLE_ARRAY))
              throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE,
                           "Opaque filter argument needs a serialized definition and an explicit execution converter");
            g_value_copy (value, arg.value.get ());
          }
        arguments_.push_back (std::move (arg));
      }
  }
public:
  FilterArguments (guint count, const GimpFilterArgumentSpec *specs, unsigned depth, std::size_t& remaining)
  {
    if (depth > 32 || count > remaining || (count && !specs))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter argument import exceeds structural limits");
    remaining -= count;
    arguments_.reserve (count);
    for (guint i = 0; i < count; ++i)
      {
        const auto& spec = specs[i];
        if (spec.is_null != FALSE && spec.is_null != TRUE)
          throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid argument null flag");
        Argument arg;
        arg.value = Value (spec.value_type);
        if (G_VALUE_HOLDS_OBJECT (arg.value.get ()) || GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (arg.value.get ()))
          {
            const bool single = G_VALUE_HOLDS_OBJECT (arg.value.get ());
            if (spec.n_children || (single && spec.n_references != 1) ||
                (!single && spec.is_null && spec.n_references) || spec.n_references > remaining ||
                (spec.n_references && !spec.references))
              throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid imported reference shape");
            remaining -= spec.n_references;
            arg.null_container = spec.is_null;
            for (guint j = 0; j < spec.n_references; ++j)
              {
                const auto& ref = spec.references[j];
                GObject *target = spec.targets ? spec.targets[j] : nullptr;
                if ((ref.was_set != FALSE && ref.was_set != TRUE) ||
                    (ref.expired != FALSE && ref.expired != TRUE) ||
                    (!ref.was_set && ref.expired) ||
                    !g_type_is_a (ref.object_type, G_TYPE_OBJECT) ||
                    (single && !g_type_is_a (ref.object_type, spec.value_type)) ||
                    (single && bool (spec.is_null) == bool (ref.was_set)) ||
                    (!single && !ref.was_set) ||
                    (target && (!ref.was_set || ref.expired ||
                                !G_TYPE_CHECK_INSTANCE_TYPE (target, ref.object_type))))
                  throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid imported object descriptor or target");
                arg.objects.emplace_back (target, ref.object_type);
                arg.objects.back ().type = ref.object_type;
                arg.objects.back ().id = ref.id;
                arg.objects.back ().had_object = ref.was_set;
              }
          }
        else if (GIMP_VALUE_HOLDS_VALUE_ARRAY (arg.value.get ()))
          {
            if (spec.n_references || (spec.is_null && spec.n_children))
              throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid nested argument shape");
            if (!spec.is_null)
              arg.nested = std::make_shared<FilterArguments> (spec.n_children, spec.children, depth + 1, remaining);
          }
        else
          {
            if (spec.n_children || spec.n_references || !spec.value || !G_IS_VALUE (spec.value) ||
                G_VALUE_TYPE (spec.value) != spec.value_type)
              throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Imported scalar has the wrong GValue type");
            const auto type = spec.value_type;
            if (G_VALUE_HOLDS_POINTER (spec.value) || G_VALUE_HOLDS_PARAM (spec.value) ||
                (G_VALUE_HOLDS_BOXED (spec.value) && type != G_TYPE_BYTES && type != G_TYPE_STRV &&
                 type != GIMP_TYPE_ARRAY && type != GIMP_TYPE_INT32_ARRAY && type != GIMP_TYPE_DOUBLE_ARRAY))
              throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Unsupported imported scalar type");
            copy_imported_argument_scalar (spec.value, arg.value.get ());
          }
        arguments_.push_back (std::move (arg));
        if (is_null (i) != bool (spec.is_null))
          throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Imported null flag conflicts with its value");
      }
  }
  FilterArguments (const std::shared_ptr<const FilterArguments>& source,
                   guint count, const GimpFilterArgumentPatch *patches)
  {
    if (!source || source->size () > 512 || count > 512 || (count && !patches))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter argument edit exceeds structural limits");
    /* Reserve for copied name, model/container storage, and the temporary and
     * retained values. Payload accounting precedes every potentially bulk copy. */
    gsize remaining = 1024 * 1024 - 16384 - source->size () * 256;
    for (guint i = 0; i < count; ++i)
      {
        const auto& patch = patches[i];
        if (patch.index >= source->size () || !patch.value || !G_IS_VALUE (patch.value) ||
            !source->scalar (patch.index) || G_VALUE_TYPE (patch.value) != G_VALUE_TYPE (source->at (patch.index)))
          throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Filter argument patch has the wrong index or exact type");
        for (guint j = 0; j < i; ++j)
          if (patches[j].index == patch.index)
            throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Duplicate filter argument patch");
        if (!patch_copy_admitted (patch.value, remaining))
          throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter argument patch exceeds the safe copy budget");
      }
    shared_arguments_.reserve (source->size ());
    for (std::size_t i = 0; i < source->size (); ++i)
      {
        /* Flatten an existing patch rather than retaining its whole model.
         * Original slots use aliasing ownership, so noncopyable weak links and
         * nested models retain their exact immutable identity and provenance. */
        shared_arguments_.push_back (source->shared_arguments_.empty () ?
          std::shared_ptr<const Argument> (source, &source->arguments_[i]) : source->shared_arguments_[i]);
      }
    for (guint i = 0; i < count; ++i)
      {
        auto replacement = std::make_shared<Argument> ();
        replacement->value = Value (G_VALUE_TYPE (patches[i].value));
        copy_imported_argument_scalar (patches[i].value, replacement->value.get ());
        shared_arguments_[patches[i].index] = std::move (replacement);
      }
  }
  std::size_t size () const noexcept
  { return shared_arguments_.empty () ? arguments_.size () : shared_arguments_.size (); }
  const GValue *at (std::size_t i) const { return argument_at (i).value.get (); }
  std::size_t reference_count (std::size_t argument) const
  { return argument < size () ? argument_at (argument).objects.size () : 0; }
  std::shared_ptr<const FilterArguments> nested (std::size_t argument) const
  { return argument < size () ? argument_at (argument).nested : nullptr; }
  bool is_null (std::size_t argument) const
  {
    const auto& arg = argument_at (argument);
    const auto *value = arg.value.get ();
    if (G_VALUE_HOLDS_OBJECT (value)) return !arg.objects[0].had_object;
    if (GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (value)) return arg.null_container;
    if (GIMP_VALUE_HOLDS_VALUE_ARRAY (value)) return !arg.nested;
    if (G_VALUE_HOLDS_STRING (value)) return g_value_get_string (value) == nullptr;
    if (G_VALUE_HOLDS_BOXED (value)) return g_value_get_boxed (value) == nullptr;
    if (G_VALUE_HOLDS_VARIANT (value)) return g_value_get_variant (value) == nullptr;
    return false;
  }
  bool scalar (std::size_t argument) const
  {
    const auto *value = at (argument);
    return !G_VALUE_HOLDS_OBJECT (value) && !GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (value) && !GIMP_VALUE_HOLDS_VALUE_ARRAY (value);
  }
  const FilterArgumentReference *reference (std::size_t argument, std::size_t element) const
  {
    if (argument >= size () || element >= argument_at (argument).objects.size ()) return nullptr;
    return &argument_at (argument).objects[element];
  }
  GimpValueArray *copy_values () const
  {
    auto *result = gimp_value_array_new (size ());
    try
      {
        for (std::size_t i = 0; i < size (); ++i)
          {
            const auto& arg = argument_at (i);
            Value value = arg.value;
            if (G_VALUE_HOLDS_OBJECT (value.get ()))
              {
                auto target = arg.objects[0].target.lock ();
                g_value_set_object (value.get (), target.get ());
              }
            else if (GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (value.get ()) && !arg.null_container)
              {
                std::vector<GObject *> pointers;
                std::vector<ObjectRef<GObject>> leases;
                for (const auto& ref : arg.objects)
                  {
                    auto target = ref.target.lock ();
                    if (!target && ref.had_object)
                      throw Error (GIMP_PAINTER_ERROR_CLOSED, "Saved object-array argument has an expired reference");
                    pointers.push_back (target.get ()); leases.push_back (std::move (target));
                  }
                pointers.push_back (nullptr);
                g_value_set_boxed (value.get (), pointers.data ());
              }
            else if (arg.nested)
              g_value_take_boxed (value.get (), arg.nested->copy_values ());
            gimp_value_array_append (result, value.get ());
          }
      }
    catch (...) { gimp_value_array_unref (result); throw; }
    return result;
  }
private:
  static bool patch_copy_admitted (const GValue *value, gsize& remaining)
  {
    const auto charge = [&] (gsize size) {
      if (size > remaining) return false;
      remaining -= size; return true;
    };
    const auto copies = [&] (gsize size) {
      return size <= remaining / 2 && charge (size * 2);
    };
    const auto string = [&] (const gchar *text) {
      if (!text) return true;
      const gsize limit = remaining / 2;
      gsize length = 0;
      while (length < limit && text[length]) ++length;
      return length < limit && copies (length + 1) && charge (64);
    };
    if (!charge (256)) return false;
    const GType type = G_VALUE_TYPE (value);
    if (type == G_TYPE_STRING) return string (g_value_get_string (value));
    if (type == G_TYPE_STRV)
      {
        const auto *strings = static_cast<const gchar *const *> (g_value_get_boxed (value));
        if (!strings) return true;
        if (!copies (sizeof (gchar *))) return false;
        for (guint i = 0; strings[i]; ++i)
          if (i >= 512 || !copies (sizeof (gchar *)) || !string (strings[i])) return false;
        return true;
      }
    if (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY)
      {
        const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (value));
        if (!array) return true;
        const gsize width = type == GIMP_TYPE_DOUBLE_ARRAY ? sizeof (gdouble) :
                            type == GIMP_TYPE_INT32_ARRAY ? sizeof (gint32) : 1;
        return (!array->length || array->data) && !(array->length % width) &&
               copies (array->length) && charge (2 * (sizeof (GimpArray) + 32));
      }
    if (type == G_TYPE_BYTES || type == G_TYPE_VARIANT || type == G_TYPE_GTYPE) return true;
    switch (G_TYPE_FUNDAMENTAL (type))
      {
      case G_TYPE_CHAR: case G_TYPE_UCHAR: case G_TYPE_BOOLEAN:
      case G_TYPE_INT: case G_TYPE_UINT: case G_TYPE_LONG: case G_TYPE_ULONG:
      case G_TYPE_INT64: case G_TYPE_UINT64: case G_TYPE_ENUM: case G_TYPE_FLAGS:
      case G_TYPE_FLOAT: case G_TYPE_DOUBLE: return true;
      default: return false;
      }
  }
  const Argument& argument_at (std::size_t i) const
  { return shared_arguments_.empty () ? arguments_.at (i) : *shared_arguments_.at (i); }
  std::vector<Argument> arguments_;
  std::vector<std::shared_ptr<const Argument>> shared_arguments_;
};
} // namespace GimpPainter
#endif
