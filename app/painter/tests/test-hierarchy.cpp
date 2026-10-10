/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-hierarchy.h"
#include "test-registry.hpp"
#include "binding-store.hpp"
#include <array>
#include <type_traits>
static_assert (std::is_same<decltype (PainterReadableInterface::read),
                           decltype (&painter_hierarchy_read)>::value,
               "C++ trampoline must match the native interface slot exactly");
namespace GimpPainter {
template<> struct TypeTraits<PainterHierarchyBase> { static GType type () { return painter_hierarchy_base_get_type (); } };
template<> struct TypeTraits<PainterHierarchyChild> { static GType type () { return painter_hierarchy_child_get_type (); } };
template<> struct TypeTraits<PainterReadable> { static GType type () { return painter_readable_get_type (); } };
}
using namespace GimpPainter;
namespace {
struct Stats {
  int constructed=0,destroyed=0,parent_calls=0;
  bool reenter=false,trace_shutdown=false;
  int closed_count=0,finalize_count=0;
  int interface_entered=0,interface_unwound=0;
  std::array<int,2> closed {{0,0}}, close_by_tag {{0,0}}, destroy_by_tag {{0,0}};
  std::array<int,4> finalize_phases {{0,0,0,0}};
};
Stats stats;
struct Impl {
  explicit Impl (int tag):tag(tag) { ++stats.constructed; }
  ~Impl () {
    if (stats.trace_shutdown) {
      g_assert_cmpint (stats.finalize_count, ==, stats.constructed);
      g_assert_cmpint (stats.closed_count, ==, stats.constructed);
    }
    ++stats.destroyed; ++stats.destroy_by_tag[tag-1];
  }
  void close () noexcept {
    g_assert_cmpint (stats.closed_count,<,2);
    stats.closed[stats.closed_count++]=tag; ++stats.close_by_tag[tag-1];
  }
  int tag,value=-1;
  int read_failure=0; // Fixture-only failure injection, independent of properties.
};
struct BaseSlot:SlotSpec<PainterHierarchyBase,Impl>{};
struct ChildSlot:SlotSpec<PainterHierarchyChild,Impl>{};
struct BaseLookalikeSlot:SlotSpec<PainterHierarchyBase,Impl>{};
auto child () -> ObjectRef<PainterHierarchyChild> {
  return ObjectRef<PainterHierarchyChild>::adopt (static_cast<PainterHierarchyChild*> (g_object_new (painter_hierarchy_child_get_type (),"base-value",17,"child-value",31,nullptr)));
}
void hierarchy_properties_interface ()
{
  stats={};
  const GType interface_type = painter_readable_get_type ();
  const GType base_type = painter_hierarchy_base_get_type ();
  const GType child_type = painter_hierarchy_child_get_type ();
  g_assert_true (G_TYPE_IS_INTERFACE (interface_type));
  g_assert_false (G_TYPE_IS_INSTANTIATABLE (interface_type));
  g_assert_true (G_TYPE_IS_OBJECT (base_type));
  g_assert_true (G_TYPE_IS_OBJECT (child_type));
  g_assert_false (G_TYPE_IS_INTERFACE (child_type));
  g_assert_cmpuint (g_type_parent (child_type), ==, base_type);
  guint count = 0;
  GType *types = g_type_interface_prerequisites (interface_type, &count);
  g_assert_cmpuint (count, ==, 1);
  g_assert_cmpuint (types[0], ==, G_TYPE_OBJECT);
  g_free (types);
  types = g_type_interfaces (base_type, &count);
  g_assert_cmpuint (count, ==, 0);
  g_free (types);
  types = g_type_interfaces (child_type, &count);
  g_assert_cmpuint (count, ==, 1);
  g_assert_cmpuint (types[0], ==, interface_type);
  g_free (types);
  auto *default_iface = static_cast<PainterReadableInterface *> (g_type_default_interface_ref (interface_type));
  g_assert_cmpuint (default_iface->parent.g_type, ==, interface_type);
  g_assert_null (default_iface->read);
  {
    auto defaults=ObjectRef<PainterHierarchyChild>::adopt (static_cast<PainterHierarchyChild*> (g_object_new (painter_hierarchy_child_get_type (),nullptr)));
    g_assert_cmpint (painter_hierarchy_get (G_OBJECT(defaults.get()),FALSE),==,7);
    g_assert_cmpint (painter_hierarchy_get (G_OBJECT(defaults.get()),TRUE),==,9);
  }
  stats={};
  auto owner=child (); auto base=ObjectRef<PainterHierarchyBase>::retain (reinterpret_cast<PainterHierarchyBase*>(owner.get ()));
  auto readable=ObjectRef<PainterReadable>::retain (reinterpret_cast<PainterReadable*>(owner.get ()));
  auto *iface = G_TYPE_INSTANCE_GET_INTERFACE (owner.get (), painter_readable_get_type (), PainterReadableInterface);
  g_assert_true (iface != default_iface);
  g_assert_cmpuint (iface->parent.g_type, ==, interface_type);
  g_assert_cmpuint (iface->parent.g_instance_type, ==, child_type);
  g_assert_true (g_type_interface_peek (G_OBJECT_GET_CLASS (owner.get ()), interface_type) == iface);
  g_assert_null (g_type_interface_peek_parent (iface));
  auto *base_class = G_OBJECT_CLASS (g_type_class_peek (base_type));
  auto *child_class = G_OBJECT_GET_CLASS (owner.get ());
  g_assert_true (base_class != child_class);
  g_assert_true (base_class->constructed == child_class->constructed);
  g_assert_true (base_class->get_property != child_class->get_property);
  g_assert_null (default_iface->read);
  g_assert_true (iface->read == &painter_hierarchy_read);
  g_assert_cmpint (stats.constructed,==,2);
  auto& store=BindingStore::require (G_OBJECT (owner.get ()));
  g_assert_cmpint (store.read<BaseSlot> ([] (const Impl& s){return s.value;}),==,17);
  g_assert_cmpint (store.read<ChildSlot> ([] (const Impl& s){return s.value;}),==,31);
  GError *error=nullptr; g_assert_cmpint (painter_readable_read (readable.get (),&error),==,48);g_assert_no_error (error);
  g_object_set (owner.get (),"base-value",3,"child-value",11,nullptr);
  gint inherited=0,own=0;g_object_get (owner.get (),"base-value",&inherited,"child-value",&own,nullptr);
  g_assert_cmpint (inherited,==,3);g_assert_cmpint (own,==,11);
  owner.reset ();base.reset ();g_assert_cmpint (stats.destroyed,==,0);readable.reset ();
  g_assert_cmpint (stats.destroyed,==,2);g_assert_true ((stats.closed==std::array<int,2>({{2,1}})));
  g_assert_null (default_iface->read);
  g_type_default_interface_unref (default_iface);
}
void interface_exception_boundary ()
{
  stats={};auto owner=child ();GError* error=nullptr;
  g_object_set (owner.get (),"child-value",13,nullptr);
  g_assert_cmpint (painter_readable_read (reinterpret_cast<PainterReadable*>(owner.get ()),&error),==,-1);
  g_assert_error (error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_EXCEPTION);g_clear_error (&error);
  g_object_set (owner.get (),"child-value",14,nullptr);
  g_assert_cmpint (painter_readable_read (reinterpret_cast<PainterReadable*>(owner.get ()),&error),==,31);g_assert_no_error (error);
}

void interface_exception_policy ()
{
  stats = {};
  auto owner = child ();
  auto *readable = reinterpret_cast<PainterReadable *> (owner.get ());
  auto& store = BindingStore::require (G_OBJECT (owner.get ()));
  const char *messages[] = {nullptr, "typed interface failure", "standard interface failure",
                            nullptr, "Unknown C++ exception"};
  for (int kind = 1; kind <= 4; ++kind)
    {
      store.with<ChildSlot> ([&] (Impl& impl) { impl.read_failure = kind; });
      for (bool output_error : {true, false})
        {
          GError *error = nullptr;
          const auto entered = stats.interface_entered;
          const auto unwound = stats.interface_unwound;
          // This exported C function dispatches the actual native interface slot.
          g_assert_cmpint (painter_readable_read (readable, output_error ? &error : nullptr), ==, -1);
          g_assert_cmpint (stats.interface_entered, ==, entered + 1);
          g_assert_cmpint (stats.interface_unwound, ==, unwound + 1);
          if (output_error)
            {
              const int code = kind == 1 ? GIMP_PAINTER_ERROR_CLOSED : GIMP_PAINTER_ERROR_EXCEPTION;
              g_assert_error (error, GIMP_PAINTER_ERROR, code);
              if (messages[kind]) g_assert_cmpstr (error->message, ==, messages[kind]);
              else g_assert_true (error->message && error->message[0]);
              g_clear_error (&error);
            }
          g_assert_null (error);
          g_assert_cmpuint (G_OBJECT (owner.get ())->ref_count, ==, 1);
          store.read<ChildSlot> ([] (const Impl& impl) { g_assert_cmpint (impl.value, ==, 31); });
        }
      store.with<ChildSlot> ([] (Impl& impl) { impl.read_failure = 0; });
      GError *error = nullptr;
      g_assert_cmpint (painter_readable_read (readable, &error), ==, 48);
      g_assert_no_error (error);
    }
  g_assert_cmpint (stats.interface_entered, ==, 8);
  g_assert_cmpint (stats.interface_unwound, ==, 8);
}

void slot_identity ()
{
  stats = {};
  auto owner = child ();
  auto *base = reinterpret_cast<PainterHierarchyBase *> (owner.get ());
  auto& through_child = BindingStore::require (G_OBJECT (owner.get ()));
  auto& through_base = BindingStore::require (G_OBJECT (base));
  g_assert_true (&through_child == &through_base);
  auto *readable = reinterpret_cast<PainterReadable *> (owner.get ());
  g_assert_true (&through_child == &BindingStore::require (G_OBJECT (readable)));
  using BaseAlias = BaseSlot;
  through_base.with<BaseAlias> ([&] (Impl& parent) {
    through_child.with<ChildSlot> ([&] (Impl& child) {
      // Both implementations have exactly the same C++ type. Slot identity,
      // not an owner cast or Impl type, must select the distinct instances.
      g_assert_true (&parent != &child);
      g_assert_cmpint (parent.tag, ==, 1);
      g_assert_cmpint (child.tag, ==, 2);
      g_assert_cmpint (parent.value, ==, 17);
      g_assert_cmpint (child.value, ==, 31);
      parent.value = 23;
      g_assert_cmpint (child.value, ==, 31);
      child.value = 47;
      g_assert_cmpint (parent.value, ==, 23);
    });
  });
  bool rejected = false, entered = false;
  try { through_base.read<BaseLookalikeSlot> ([&] (const Impl&) { entered = true; }); }
  catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_MISSING_SLOT; }
  g_assert_true (rejected);
  g_assert_false (entered);
  g_assert_cmpint (stats.constructed, ==, 2);
  g_assert_cmpint (painter_hierarchy_get (G_OBJECT (base), FALSE), ==, 23);
  g_assert_cmpint (painter_hierarchy_get (G_OBJECT (base), TRUE), ==, 47);
  through_base.close ();
  g_assert_cmpint (through_child.read<BaseSlot> ([] (const Impl& i) { return i.value; }), ==, 23);
  g_assert_cmpint (through_child.read<ChildSlot> ([] (const Impl& i) { return i.value; }), ==, 47);
  owner.reset ();
  g_assert_cmpint (stats.destroyed, ==, 2);
  g_assert_true ((stats.closed == std::array<int,2> ({{2,1}})));
}

void base_missing_derived ()
{
  stats = {};
  auto owner = ObjectRef<PainterHierarchyBase>::adopt (
    static_cast<PainterHierarchyBase *> (g_object_new (painter_hierarchy_base_get_type (), nullptr)));
  auto& store = BindingStore::require (G_OBJECT (owner.get ()));
  const auto generation = store.generation ();
  bool entered = false;
  for (int attempt = 0; attempt < 3; ++attempt) {
    bool read_rejected = false, write_rejected = false;
    try { store.read<ChildSlot> ([&] (const Impl&) { entered = true; }); }
    catch (const Error& error) { read_rejected = error.code () == GIMP_PAINTER_ERROR_MISSING_SLOT; }
    try { store.with<ChildSlot> ([&] (Impl&) { entered = true; }); }
    catch (const Error& error) { write_rejected = error.code () == GIMP_PAINTER_ERROR_MISSING_SLOT; }
    g_assert_true (read_rejected);
    g_assert_true (write_rejected);
    g_assert_false (entered);
    g_assert_cmpint (stats.constructed, ==, 1);
    g_assert_cmpint (stats.destroyed, ==, 0);
    g_assert_true (store.accepts (generation));
    g_assert_cmpint (store.read<BaseSlot> ([] (const Impl& i) { return i.tag; }), ==, 1);
    g_assert_cmpint (store.read<BaseSlot> ([] (const Impl& i) { return i.value; }), ==, 7);
  }
  owner.reset ();
  g_assert_cmpint (stats.destroyed, ==, 1);
  g_assert_cmpint (stats.closed_count, ==, 1);
  g_assert_cmpint (stats.closed[0], ==, 1);
}
void parent_dispose_reentry ()
{
  stats={};auto owner=child ();stats.reenter=true;stats.trace_shutdown=true;
  auto& store=BindingStore::require (G_OBJECT (owner.get ()));
  const auto generation=store.generation ();
  g_object_run_dispose (G_OBJECT (owner.get ()));
  g_assert_cmpint (stats.parent_calls,==,2); // Outer dispose and one nested dispose.
  g_object_run_dispose (G_OBJECT (owner.get ()));
  g_assert_cmpint (stats.parent_calls,==,3);
  g_assert_cmpuint (store.generation (),==,generation+1);
  g_assert_true ((stats.closed==std::array<int,2>({{2,1}})));
  g_assert_true ((stats.close_by_tag==std::array<int,2>({{1,1}})));
  g_assert_cmpint (stats.destroyed,==,0);g_assert_cmpint (stats.finalize_count,==,0);
  owner.reset ();
  g_assert_cmpint (stats.parent_calls,==,4); // Final native unref still chains dispose.
  g_assert_cmpint (stats.destroyed,==,2);
  g_assert_true ((stats.destroy_by_tag==std::array<int,2>({{1,1}})));
  g_assert_true ((stats.finalize_phases==std::array<int,4>({{2,1,-1,-2}})));
}

void hierarchy_shutdown_paths ()
{
  for (bool derived : {false,true}) for (bool explicit_close : {false,true}) {
    stats={};
    auto owner=ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (
      derived ? painter_hierarchy_child_get_type () : painter_hierarchy_base_get_type (),nullptr)));
    stats.trace_shutdown=true;
    auto& store=BindingStore::require (owner.get ());
    const auto generation=store.generation ();
    if (explicit_close) { store.close ();store.close (); }
    g_object_run_dispose (owner.get ());g_object_run_dispose (owner.get ());
    g_assert_cmpint (stats.parent_calls,==,2);
    g_assert_cmpuint (store.generation (),==,generation+1);
    g_assert_cmpint (stats.closed_count,==,derived?2:1);
    g_assert_true ((stats.close_by_tag==std::array<int,2>({{1,derived?1:0}})));
    g_assert_cmpint (stats.destroyed,==,0);g_assert_cmpint (stats.finalize_count,==,0);
    owner.reset ();
    g_assert_cmpint (stats.parent_calls,==,3);
    g_assert_cmpint (stats.destroyed,==,derived?2:1);
    g_assert_true ((stats.destroy_by_tag==std::array<int,2>({{1,derived?1:0}})));
    if (derived) g_assert_true ((stats.finalize_phases==std::array<int,4>({{2,1,-1,-2}})));
    else g_assert_true ((stats.finalize_phases==std::array<int,4>({{1,-1,0,0}})));
  }
}

void property_trace (guint base_set, guint base_get, guint child_set, guint child_get)
{
  const auto actual = painter_hierarchy_property_trace ();
  g_assert_cmpuint (actual.base_set, ==, base_set);
  g_assert_cmpuint (actual.base_get, ==, base_get);
  g_assert_cmpuint (actual.child_set, ==, child_set);
  g_assert_cmpuint (actual.child_get, ==, child_get);
}

void native_property_owner_dispatch ()
{
  stats = {};
  auto owner = child ();
  auto *klass = G_OBJECT_GET_CLASS (owner.get ());
  auto *base_spec = g_object_class_find_property (klass, "base-value");
  auto *child_spec = g_object_class_find_property (klass, "child-value");
  g_assert_cmpuint (base_spec->owner_type, ==, painter_hierarchy_base_get_type ());
  g_assert_cmpuint (child_spec->owner_type, ==, painter_hierarchy_child_get_type ());
  painter_hierarchy_reset_property_trace ();
  gint value = 0;
  g_object_set (owner.get (), "base-value", 19, nullptr);
  g_object_get (owner.get (), "base-value", &value, nullptr);
  g_assert_cmpint (value, ==, 19);
  property_trace (1, 1, 0, 0); // GObject dispatches to the property's owner class.
  g_assert_cmpint (painter_hierarchy_get (G_OBJECT (owner.get ()), TRUE), ==, 31);
  painter_hierarchy_reset_property_trace ();
  g_object_set (owner.get (), "child-value", 37, nullptr);
  g_object_get (owner.get (), "child-value", &value, nullptr);
  g_assert_cmpint (value, ==, 37);
  property_trace (0, 0, 1, 1);
  g_assert_cmpint (painter_hierarchy_get (G_OBJECT (owner.get ()), FALSE), ==, 19);
}

void inherited_property_delegation ()
{
  stats = {};
  auto owner = child ();
  auto *object = G_OBJECT (owner.get ());
  auto *klass = G_OBJECT_GET_CLASS (owner.get ());
  auto *base_spec = g_object_class_find_property (klass, "base-value");
  auto *child_spec = g_object_class_find_property (klass, "child-value");
  GValue value = G_VALUE_INIT;
  g_value_init (&value, G_TYPE_INT);
  painter_hierarchy_reset_property_trace ();
  g_value_set_int (&value, 23);
  // Call the derived C vfunc directly to execute its actual fallback branch.
  // Ordinary g_object_set/get above do not exercise this path for inherited IDs.
  klass->set_property (object, base_spec->param_id, &value, base_spec);
  g_value_set_int (&value, 0);
  klass->get_property (object, base_spec->param_id, &value, base_spec);
  g_assert_cmpint (g_value_get_int (&value), ==, 23);
  property_trace (1, 1, 1, 1);
  g_assert_cmpint (painter_hierarchy_get (object, TRUE), ==, 31);

  painter_hierarchy_reset_property_trace ();
  g_value_set_int (&value, 47);
  klass->set_property (object, child_spec->param_id, &value, child_spec);
  g_value_set_int (&value, 0);
  klass->get_property (object, child_spec->param_id, &value, child_spec);
  g_assert_cmpint (g_value_get_int (&value), ==, 47);
  property_trace (0, 0, 1, 1);
  g_assert_cmpint (painter_hierarchy_get (object, FALSE), ==, 23);

  painter_hierarchy_reset_property_trace ();
  g_value_set_int (&value, 53);
  g_test_expect_message (nullptr, G_LOG_LEVEL_WARNING, "*invalid property id 99*base-value*");
  klass->set_property (object, 99, &value, base_spec);
  g_test_assert_expected_messages ();
  g_test_expect_message (nullptr, G_LOG_LEVEL_WARNING, "*invalid property id 99*base-value*");
  klass->get_property (object, 99, &value, base_spec);
  g_test_assert_expected_messages ();
  property_trace (1, 1, 1, 1); // Fixed parent, exactly once, no recursive redispatch.
  g_assert_cmpint (g_value_get_int (&value), ==, 53);
  g_assert_cmpint (painter_hierarchy_get (object, FALSE), ==, 23);
  g_assert_cmpint (painter_hierarchy_get (object, TRUE), ==, 47);
  g_assert_cmpint (stats.constructed, ==, 2);
  g_value_unset (&value);
}

template<class T> void accepted_type (GObject *object)
{
  using Ref = ObjectRef<T>;
  for (auto factory : {&Ref::retain, &Ref::adopt, &Ref::sink})
    {
      // adopt receives a distinct native producer-owned reference.
      if (factory == &Ref::adopt) g_object_ref (object);
      auto accepted = factory (reinterpret_cast<T *> (object));
      g_assert_true (reinterpret_cast<GObject *> (accepted.get ()) == object);
      g_assert_cmpuint (object->ref_count, ==, 2);
      accepted.reset ();
      g_assert_cmpuint (object->ref_count, ==, 1);
    }
}

template<class T> void rejected_type (GObject *object)
{
  using Ref = ObjectRef<T>;
  for (auto factory : {&Ref::retain, &Ref::adopt, &Ref::sink})
    {
      bool rejected = false;
      try { auto bad = factory (reinterpret_cast<T *> (object)); }
      catch (const Error& error)
        { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
      g_assert_true (rejected);
      g_assert_cmpuint (object->ref_count, ==, 1);
    }
}

void type_ancestry ()
{
  stats = {};
  {
    auto owner = child ();
    auto *object = G_OBJECT (owner.get ());
    accepted_type<PainterHierarchyChild> (object);
    accepted_type<PainterHierarchyBase> (object);
    accepted_type<PainterReadable> (object);
    accepted_type<GObject> (object);
    g_assert_cmpint (stats.destroyed, ==, 0);
  }
  g_assert_cmpint (stats.destroyed, ==, 2);
  stats = {};
  {
    auto owner = ObjectRef<PainterHierarchyBase>::adopt (
      static_cast<PainterHierarchyBase *> (g_object_new (painter_hierarchy_base_get_type (), nullptr)));
    auto *object = G_OBJECT (owner.get ());
    accepted_type<PainterHierarchyBase> (object);
    rejected_type<PainterHierarchyChild> (object);
    rejected_type<PainterReadable> (object);
    g_assert_cmpint (stats.destroyed, ==, 0);
  }
  g_assert_cmpint (stats.destroyed, ==, 1);
}
}
void painter_hierarchy_init_binding (GObject *owner,gboolean is_child)
{
  GError* error=nullptr;boundary_void (&error,[&]{auto& store=BindingStore::ensure (owner);if(is_child)store.emplace<ChildSlot>(2);else store.emplace<BaseSlot>(1);});g_assert_no_error (error);
}
void painter_hierarchy_activate_binding (GObject *owner)
{ GError* error=nullptr;boundary_void (&error,[&]{BindingStore::require(owner).activate();});g_assert_no_error(error); }
void painter_hierarchy_set (GObject *owner,gboolean is_child,gint value)
{
  GError* error=nullptr;boundary_void (&error,[&]{auto& store=BindingStore::require(owner);auto set=[value](Impl& i){i.value=value;};
    if(store.state()==BindingStore::State::constructing){if(is_child)store.initialize<ChildSlot>(set);else store.initialize<BaseSlot>(set);}
    else {if(is_child)store.with<ChildSlot>(set);else store.with<BaseSlot>(set);}});g_assert_no_error(error);
}
gint painter_hierarchy_get (GObject *owner,gboolean is_child)
{ GError* error=nullptr;auto value=boundary<gint>(&error,-1,[&]{auto& store=BindingStore::require(owner);auto read=[](const Impl& s){return s.value;};return is_child?store.read<ChildSlot>(read):store.read<BaseSlot>(read);});g_assert_no_error(error);return value; }
gint painter_hierarchy_read (PainterReadable *owner,GError **error)
{
  return boundary<gint> (error, -1, [&] {
    auto& store = BindingStore::require (G_OBJECT (owner));
    const int failure = store.read<ChildSlot> ([] (const Impl& impl) { return impl.read_failure; });
    if (failure)
      {
        ++stats.interface_entered;
        struct Unwind { ~Unwind () { ++stats.interface_unwound; } } unwind;
        switch (failure)
          {
          case 1: throw Error (GIMP_PAINTER_ERROR_CLOSED, "typed interface failure");
          case 2: throw std::runtime_error ("standard interface failure");
          case 3: throw std::bad_alloc ();
          default: throw 17;
          }
      }
    const int base = painter_hierarchy_get (G_OBJECT (owner), FALSE);
    const int child = painter_hierarchy_get (G_OBJECT (owner), TRUE);
    if (child == 13) throw std::runtime_error ("interface failure");
    return base + child;
  });
}
void painter_hierarchy_parent_dispose (GObject *owner)
{
  ++stats.parent_calls;
  if (stats.trace_shutdown) {
    g_assert_cmpint (stats.closed_count,==,stats.constructed);
    g_assert_cmpint (stats.destroyed,==,0);
    g_assert_cmpint (stats.finalize_count,==,0);
    g_assert_cmpint (stats.closed[0],==,stats.constructed);
    if (stats.constructed==2) g_assert_cmpint (stats.closed[1],==,1);
  }
  g_assert_true(BindingStore::require(owner).state()==BindingStore::State::closed);
  g_assert_cmpint(painter_hierarchy_get(owner,FALSE),>=,0);
  if(G_TYPE_CHECK_INSTANCE_TYPE(owner,painter_hierarchy_child_get_type()))g_assert_cmpint(painter_hierarchy_get(owner,TRUE),>=,0);
  if(stats.reenter){stats.reenter=false;g_object_run_dispose(owner);}
}
void painter_hierarchy_finalize_phase (gint phase)
{
  if (!stats.trace_shutdown) return;
  g_assert_cmpint (stats.finalize_count,<,4);
  stats.finalize_phases[stats.finalize_count++]=phase;
  g_assert_cmpint (stats.closed_count,==,stats.constructed);
  g_assert_cmpint (stats.destroyed,==,phase>0?0:stats.constructed);
}
void painter_test_register_hierarchy ()
{
  g_test_add_func("/painter/hierarchy/properties-interface",hierarchy_properties_interface);
  g_test_add_func("/painter/hierarchy/interface-exception",interface_exception_boundary);
  g_test_add_func("/painter/hierarchy/interface-exception-policy",interface_exception_policy);
  g_test_add_func("/painter/hierarchy/slot-identity",slot_identity);
  g_test_add_func("/painter/hierarchy/base-missing-derived",base_missing_derived);
  g_test_add_func("/painter/hierarchy/native-property-owner-dispatch",native_property_owner_dispatch);
  g_test_add_func("/painter/hierarchy/inherited-property-delegation",inherited_property_delegation);
  g_test_add_func("/painter/hierarchy/parent-dispose-reentry",parent_dispose_reentry);
  g_test_add_func("/painter/hierarchy/shutdown-paths",hierarchy_shutdown_paths);
  g_test_add_func("/painter/hierarchy/type-ancestry",type_ancestry);
}
