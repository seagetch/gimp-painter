/* Isolated language control, never linked into GIMP. */
#include <typeinfo>
struct Base { virtual ~Base () = default; };
struct Derived : Base {};
int main ()
{
  Derived derived;
  Base *base = &derived;
  return typeid (*base) == typeid (Derived) ? 0 : 1;
}
