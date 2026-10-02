/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_LAYER_ARGUMENTS_HPP
#define GIMP_FILTER_LAYER_ARGUMENTS_HPP
/* Included after the core/GimpValueArray C declarations. Main-thread only. */
#include "painter/resources.hpp"
#include "painter/object-ref.hpp"
#include <memory>
#include <vector>

namespace GimpPainter {
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
class FilterArguments
{
  struct Argument
  {
    Value value;
    std::vector<FilterArgumentReference> objects;
    std::shared_ptr<const FilterArguments> nested;
  };
public:
  explicit FilterArguments (const GimpValueArray *args, unsigned depth = 0)
  {
    if (depth > 32) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Nested filter arguments exceed the limit");
    const auto n = gimp_value_array_length (args);
    if (n > 65536) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Too many filter arguments");
    arguments_.reserve (n);
    for (int i = 0; i < n; ++i)
      {
        const GValue *value = gimp_value_array_index (args, i);
        Argument arg;
        arg.value = Value (G_VALUE_TYPE (value));
        if (G_VALUE_HOLDS_OBJECT (value))
          arg.objects.emplace_back (G_OBJECT (g_value_get_object (value)), G_VALUE_TYPE (value));
        else if (GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (value))
          {
            auto **array = static_cast<GObject **> (g_value_get_boxed (value));
            for (std::size_t j = 0; array && array[j]; ++j)
              {
                if (j >= 65536) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Too many object references");
                arg.objects.emplace_back (array[j], G_TYPE_OBJECT);
              }
          }
        else if (GIMP_VALUE_HOLDS_VALUE_ARRAY (value))
          {
            auto *array = static_cast<const GimpValueArray *> (g_value_get_boxed (value));
            if (array) arg.nested = std::make_shared<FilterArguments> (array, depth + 1);
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
  std::size_t size () const noexcept { return arguments_.size (); }
  const GValue *at (std::size_t i) const { return arguments_.at (i).value.get (); }
  const FilterArgumentReference *reference (std::size_t argument, std::size_t element) const
  {
    if (argument >= arguments_.size () || element >= arguments_[argument].objects.size ()) return nullptr;
    return &arguments_[argument].objects[element];
  }
  GimpValueArray *copy_values () const
  {
    auto *result = gimp_value_array_new (arguments_.size ());
    try
      {
        for (const auto& arg : arguments_)
          {
            Value value = arg.value;
            if (G_VALUE_HOLDS_OBJECT (value.get ()))
              {
                auto target = arg.objects[0].target.lock ();
                g_value_set_object (value.get (), target.get ());
              }
            else if (GIMP_VALUE_HOLDS_CORE_OBJECT_ARRAY (value.get ()))
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
  std::vector<Argument> arguments_;
};
} // namespace GimpPainter
#endif
