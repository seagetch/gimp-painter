/* Isolated language control, never linked into GIMP. */
struct Base { virtual ~Base () = default; };
struct Derived : Base { int value = 47; };
int main ()
{
  Derived derived;
  Base *base = &derived;
  Derived *result = dynamic_cast<Derived *> (base);
  return result && result->value == 47 ? 0 : 1;
}
