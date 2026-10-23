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
struct Stats { int constructed=0,destroyed=0,parent_calls=0; bool reenter=false; int closed_count=0; std::array<int,2> closed {{0,0}}; };
Stats stats;
struct Impl {
  explicit Impl (int tag):tag(tag) { ++stats.constructed; }
  ~Impl () { ++stats.destroyed; }
  void close () noexcept { g_assert_cmpint (stats.closed_count,<,2); stats.closed[stats.closed_count++]=tag; }
  int tag,value=-1;
};
struct BaseSlot:SlotSpec<PainterHierarchyBase,Impl>{};
struct ChildSlot:SlotSpec<PainterHierarchyChild,Impl>{};
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
void parent_dispose_reentry ()
{
  stats={};auto owner=child ();stats.reenter=true;
  g_object_run_dispose (G_OBJECT (owner.get ()));g_object_run_dispose (G_OBJECT (owner.get ()));
  g_assert_true ((stats.closed==std::array<int,2>({{2,1}})));g_assert_cmpint (stats.parent_calls,>=,3);
  g_assert_cmpint (stats.destroyed,==,0);owner.reset ();g_assert_cmpint (stats.destroyed,==,2);
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
{ return boundary<gint>(error,-1,[&]{int base=painter_hierarchy_get(G_OBJECT(owner),FALSE),child=painter_hierarchy_get(G_OBJECT(owner),TRUE);if(child==13)throw std::runtime_error("interface failure");return base+child;}); }
void painter_hierarchy_parent_dispose (GObject *owner)
{
  ++stats.parent_calls;
  g_assert_true(BindingStore::require(owner).state()==BindingStore::State::closed);
  g_assert_cmpint(painter_hierarchy_get(owner,FALSE),>=,0);
  if(G_TYPE_CHECK_INSTANCE_TYPE(owner,painter_hierarchy_child_get_type()))g_assert_cmpint(painter_hierarchy_get(owner,TRUE),>=,0);
  if(stats.reenter){stats.reenter=false;g_object_run_dispose(owner);}
}
void painter_test_register_hierarchy ()
{
  g_test_add_func("/painter/hierarchy/properties-interface",hierarchy_properties_interface);
  g_test_add_func("/painter/hierarchy/interface-exception",interface_exception_boundary);
  g_test_add_func("/painter/hierarchy/parent-dispose-reentry",parent_dispose_reentry);
  g_test_add_func("/painter/hierarchy/type-ancestry",type_ancestry);
}
