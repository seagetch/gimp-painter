/* Ordinary virtual dispatch does not require C++ RTTI. */
struct Base {
  virtual ~Base () = default;
  virtual int value () const = 0;
};
struct Derived : Base { int value () const override { return 47; } };
static int dispatch (const Base& base) { return base.value (); }
int main () { Derived derived; return dispatch (derived) == 47 ? 0 : 1; }
