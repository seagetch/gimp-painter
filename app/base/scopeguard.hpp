#ifndef __SCOPE_GUARD_HPP__
#define __SCOPE_GUARD_HPP__

template<class T>
bool always_true(const T) { return true; }

template<class T>
T default_value() { return T(); }

template<class T, typename Destructor, Destructor* Free, bool (*IsNull)(const T) = always_true<T>, T (*Clear)() = default_value<T> >
class ScopeGuard {
protected:
  T obj;
public:
  ScopeGuard() : obj(Clear()) {  };
  ScopeGuard(T ptr) : obj(ptr) {  };
  ~ScopeGuard() {
    if (!IsNull(this->obj)) {
      Free(this->obj);
      obj = Clear();
    }
  }
  T& ref() { return obj; }
  const T& ref() const { return obj; }
};



template<class T>
bool is_null(const T mem) { return mem == NULL; };


template<class T>
T zero() { return NULL; }


template<class T, typename Destructor, Destructor* f>
class ScopedPointer : public ScopeGuard<T*, Destructor, f, is_null<T*>, zero<T*> > {
public:
  ScopedPointer() : ScopeGuard<T*, Destructor, f, is_null<T*>, zero<T*> >(NULL) {};
  ScopedPointer(T* ptr) : ScopeGuard<T*, Destructor, f, is_null<T*>, zero<T*> >(ptr) {};
  T* ptr() { return ScopeGuard<T*, Destructor, f, is_null<T*>, zero<T*> >::ref(); };
  T* const ptr() const { return ScopeGuard<T*, Destructor, f, is_null<T*>, zero<T*> >::ref(); };
  T* operator ->() { return ptr(); };
  T& operator *() { return *ptr(); };
  operator T*() { return ptr(); };
  operator T* const () const { return ptr(); };
  ScopedPointer& operator =(T* src) {
    if (this->obj == src)
      return *this;
    if (!is_null<T*>(this->obj)) {
      (*f)(this->obj);
    }
    this->obj = src;
    return *this;
  }
  operator bool() const { return !is_null(ptr()); }
};



template<class T>
void destroy_instance(T* instance)
{
  delete instance;
};


template<class T>
class CXXPointer : public ScopedPointer<T, void(T*), destroy_instance<T> >
{
  typedef ScopedPointer<T, void(T*), destroy_instance<T> > super;
public:
  CXXPointer() : super(NULL) { };
  CXXPointer(T* src) : super(src) { };
  CXXPointer(CXXPointer&& src) : super(src.obj) {
    src.obj = NULL;
  }
  T* operator ->() { return super::ptr(); }
  T& operator *() { return *super::ptr(); }
  operator T*() { return super::ptr(); }
  CXXPointer& operator=(T* src) {
    if (super::obj == src)
      return *this;
    if (!is_null(super::ptr()))
      destroy_instance(super::ptr());
    super::obj = src;
    return *this;
  };
  CXXPointer& operator=(CXXPointer&& src) {
    if (this == &src)
      return *this;
    if (super::obj == src.obj)
      return *this;
    g_print("steal assign %p from %p\n", src.obj, &src);
    if (!is_null(super::ptr()))
      destroy_instance(super::ptr());
    super::obj = src.obj;
    src.obj = NULL;
    return *this;
  }
};

template<class T>
CXXPointer<T> guard(T* ptr) { return ptr; }

template<class T>
class Nullable
{
  T* ptr;
public:
  Nullable(T* src) : ptr(src) { };
  operator const T*() { return ptr(); }
  operator T() { if(ptr) return *ptr; return T(); }
  T operator=(const T val) { if(ptr) *ptr = val; return val; };
  T operator[](gsize index) { if(ptr) return *(ptr + index); else return T(); }
};

template<class T>
Nullable<T> nullable(T* src) { return Nullable<T>(src); }
#endif
