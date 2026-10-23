/* An unambiguous upcast, including a virtual base, needs no runtime RTTI. */
struct Interface { virtual ~Interface () = default; virtual int value () const = 0; };
struct Implementation : virtual Interface { int value () const override { return 47; } };
int main ()
{
  Implementation implementation;
  Interface *base = dynamic_cast<Interface *> (&implementation);
  return base && base->value () == 47 ? 0 : 1;
}
