/* Pinned painter floating pixel expressions; see provenance in README.md. */
#ifndef __PIXEL_HPP__
#define __PIXEL_HPP__
#include <glib.h>
namespace GimpPainter { namespace MyPaint { namespace LegacyPixel {

struct Pixel {
  // Currently BPP=8 is assumed.
  typedef guchar  data_t;
  typedef guint32 internal_t;
  typedef float  real;
  
  template<typename T> struct expressions {
    typedef T E_TYPE;
    T e; 
    expressions(const T& src) : e(src) {};
  };
  
  
  struct value_t {
    static const int DEGREE = 1;
    data_t v;
    value_t(const data_t src) : v(src) {}
    expressions<value_t> wrap() const { return expressions<value_t>(*this); }
  };
  struct real_t {
    static const int DEGREE = 1;
    real v;
    real_t(const real src) : v(src) {}
    expressions<real_t> wrap() const { return expressions<real_t>(*this); }
  };
  template<typename T1, typename T2> struct prod_t {
    static const int DEGREE = T1::E_TYPE::DEGREE + T2::E_TYPE::DEGREE;
    T1 left;
    T2 right;
    prod_t(const T1& l, const T2& r) : left(l), right(r) {}
    expressions<prod_t> wrap() const { return expressions<prod_t>(*this); }
  };
  template<typename T1, typename T2> struct add_t {
    static const int DEGREE = T1::E_TYPE::DEGREE > T2::E_TYPE::DEGREE ? 
      T1::E_TYPE::DEGREE : T2::E_TYPE::DEGREE;
    T1 left;
    T2 right;
    add_t(const T1& l, const T2& r) : left(l), right(r) {}
    expressions<add_t> wrap() const { return expressions<add_t>(*this); }
  };
  template<typename T1, typename T2> struct sub_t {
    static const int DEGREE = T1::E_TYPE::DEGREE > T2::E_TYPE::DEGREE ? 
      T1::E_TYPE::DEGREE : T2::E_TYPE::DEGREE;
    T1 left;
    T2 right;
    sub_t(const T1& l, const T2& r) : left(l), right(r) {}
    expressions<sub_t> wrap() const { return expressions<sub_t>(*this); }
  };
  template<typename T1, typename T2> struct div_t {
    static const int DEGREE = T1::E_TYPE::DEGREE - T2::E_TYPE::DEGREE;
    T1 numer;
    T2 denom;
    div_t(const T1& l, const T2& r) : numer(l), denom(r) {}
    expressions<div_t> wrap() const { return expressions<div_t>(*this); }
  };
  static const int DEPTH = sizeof(data_t) * 8;
  static const data_t MAX_VALUE = (data_t)((internal_t)(1 << DEPTH) - 1);
  
  static data_t from_f(const float v) { 
    return (data_t)(CLAMP(CLAMP(v, 0, 1.0) * MAX_VALUE + 0.5f, 0, MAX_VALUE));
  };
  static float to_f(const data_t v) {
    return ((float)v) / (float)MAX_VALUE;
  };
  static data_t from_u8(const guchar v) {
    return v;
  };
  static guchar to_u8(const data_t v) {
    return v;
  };
  typedef expressions<Pixel::value_t> pixel_t;
  typedef expressions<Pixel::real_t>  rpixel_t;
  static data_t clamp(const internal_t value, const internal_t min, const internal_t max) {
    return (data_t)((value < min)? min: (value > max)? max: value);
  }
  static real clamp(const real value, const real min, const real max) {
    return (real)((value < min)? min: (value > max)? max: value);
  }
};



template<typename T1, typename T2>
inline
Pixel::expressions<Pixel::prod_t<Pixel::expressions<T1>, Pixel::expressions<T2> > > 
operator * (const Pixel::expressions<T1>& l,
             const Pixel::expressions<T2>& r) {
  return Pixel::prod_t<Pixel::expressions<T1>, Pixel::expressions<T2> >(l, r).wrap();
}

template<typename T1, typename T2>
inline
Pixel::expressions<Pixel::div_t<Pixel::expressions<T1>, Pixel::expressions<T2> > > 
operator / (const Pixel::expressions<T1>& l,
             const Pixel::expressions<T2>& r) {
  return Pixel::div_t<Pixel::expressions<T1>,Pixel::expressions<T2> >(l, r).wrap();
}

template<typename T1, typename T2>
inline
Pixel::expressions<Pixel::add_t<Pixel::expressions<T1>, Pixel::expressions<T2> > >
operator + (const Pixel::expressions<T1>& l, const Pixel::expressions<T2>& r) {
  return Pixel::add_t<Pixel::expressions<T1>,Pixel::expressions<T2> >(l, r).wrap();
}


template<typename T1, typename T2>
inline
Pixel::expressions<Pixel::sub_t<Pixel::expressions<T1>, Pixel::expressions<T2> > >
operator - (const Pixel::expressions<T1>& l, const Pixel::expressions<T2>& r) {
  return Pixel::sub_t<Pixel::expressions<T1>,Pixel::expressions<T2> >(l, r).wrap();
}

#ifndef REAL_CALC

typedef Pixel::pixel_t pixel_t;
typedef Pixel::internal_t internal_t;
typedef Pixel::data_t result_t;

// x -> P(x)
inline
pixel_t pix(const Pixel::data_t v) { 
  return Pixel::value_t(v).wrap();
}

inline
pixel_t pix(const Pixel::real v) { 
  return Pixel::value_t(Pixel::from_f(v)).wrap();
}
// f -> P(_(f))
inline
pixel_t f2p(const Pixel::real f) { 
  return pix(f);
}

inline
internal_t raw(const pixel_t& c) {
  return (Pixel::internal_t)c.e.v;
}

inline
internal_t raw(const Pixel::expressions<Pixel::real_t>& c) {
  return (Pixel::internal_t)(Pixel::from_f(c.e.v));
}

inline
result_t eval(const pixel_t& c) {
  return (result_t)c.e.v;
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::prod_t<T1, T2> >& t) {
  return raw(t.e.left) * raw(t.e.right);
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::div_t<T1, T2> >& t) {
  return raw(t.e.numer) / raw(t.e.denom);
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::add_t<T1, T2> >& t) {
  switch (T1::E_TYPE::DEGREE - T2::E_TYPE::DEGREE) {
  case -3:
    return raw(t.e.left * f2p(1.0) * f2p(1.0) * f2p(1.0)) + raw(t.e.right);
  case -2:
    return raw(t.e.left * f2p(1.0) * f2p(1.0)) + raw(t.e.right);
  case -1:
    return raw(t.e.left * f2p(1.0)) + raw(t.e.right);
  case  0:
    return raw(t.e.left) + raw(t.e.right);
  case  1:
    return raw(t.e.left) + raw(t.e.right * f2p(1.0));
  case  2:
    return raw(t.e.left) + raw(t.e.right * f2p(1.0) * f2p(1.0));
  case  3:
    return raw(t.e.left) + raw(t.e.right * f2p(1.0) * f2p(1.0) * f2p(1.0));
  default:
    throw "unsupported complexity";
  }
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::sub_t<T1, T2> >& t) {
  switch (T1::E_TYPE::DEGREE - T2::E_TYPE::DEGREE) {
  case -3:
    return raw(t.e.left * f2p(1.0) * f2p(1.0) * f2p(1.0)) - raw(t.e.right);
  case -2:
    return raw(t.e.left * f2p(1.0) * f2p(1.0)) - raw(t.e.right);
  case -1:
    return raw(t.e.left * f2p(1.0)) - raw(t.e.right);
  case  0:
    return raw(t.e.left) - raw(t.e.right);
  case  1:
    return raw(t.e.left) - raw(t.e.right * f2p(1.0));
  case  2:
    return raw(t.e.left) - raw(t.e.right * f2p(1.0) * f2p(1.0));
  case  3:
    return raw(t.e.left) - raw(t.e.right * f2p(1.0) * f2p(1.0) * f2p(1.0));
  default:
    throw "unsupported complexity";
  }
}

template<typename T>
inline
result_t eval(const Pixel::expressions<T>& t) {
  const int deg = T::DEGREE;
  Pixel::internal_t value = 0;
  switch (deg) {
  case 3:
    value = raw(t) / raw(f2p(1.0)*f2p(1.0));
    break;
  case 2:
    value = raw(t) / raw(f2p(1.0));
    break;
  case 1:
    value = raw(t);
    break;
  case 0:
    value = raw(t) * raw(f2p(1.0));
    break;
  case -1:
    value = raw(t) * raw(f2p(1.0) * f2p(1.0));
    break;
  case -2:
    value = raw(t) * raw(f2p(1.0) * f2p(1.0) * f2p(1.0));
    break;
  default:
    throw "unsupported type";
  }
  return Pixel::clamp(value, Pixel::from_f(0.0), Pixel::from_f(1.0));
}
template<typename T1, typename T2>
inline
result_t eval(const Pixel::expressions<Pixel::div_t<T1, T2> >& t) {
  const int deg = Pixel::div_t<T1, T2>::DEGREE;
  Pixel::internal_t value = 0;
  switch (deg) {
  case 3:
    value = raw(t) / raw(f2p(1.0)*f2p(1.0));
    break;
  case 2:
    value = raw(t) / raw(f2p(1.0));
    break;
  case 1:
    value = raw(t);
    break;
  case 0:
    value = raw(t.e.numer) * raw(f2p(1.0)) / raw(t.e.denom);
    break;
  case -1:
    value = raw(t.e.numer) * raw(f2p(1.0) * f2p(1.0)) / raw(t.e.denom);
    break;
  case -2:
    value = raw(t.e.numer) * raw(f2p(1.0) * f2p(1.0) * f2p(1.0)) / raw(t.e.denom);
    break;
  default:
    throw "unsupported type";
  }
  return Pixel::clamp(value, Pixel::from_f(0.0), Pixel::from_f(1.0));
}

inline Pixel::data_t r2d(const result_t v) {
  return v;
}

inline internal_t r2i(const result_t v) {
  return (internal_t)v;
}

inline Pixel::real r2f(const result_t v) {
  return Pixel::to_f(v);
}


#else /* REAL_CALC */

typedef Pixel::rpixel_t pixel_t;
typedef Pixel::real internal_t;
typedef Pixel::real result_t;

// x -> P(x)
inline
pixel_t pix(const Pixel::data_t v) { 
  return Pixel::real_t((float)v / (float)(Pixel::MAX_VALUE)).wrap();
}

inline
pixel_t pix(const Pixel::real v) { 
  return Pixel::real_t(v).wrap();
}
// f -> P(_(f))
inline
pixel_t f2p(const Pixel::real f) { 
  return pix(f);
}

inline
internal_t raw(const Pixel::pixel_t& c) {
  return (internal_t)Pixel::to_f(c.e.v);
}

inline
internal_t raw(const pixel_t& c) {
  return (internal_t)c.e.v;
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::prod_t<T1, T2> >& t) {
  return raw(t.e.left) * raw(t.e.right);
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::div_t<T1, T2> >& t) {
  return raw(t.e.numer) / raw(t.e.denom);
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::add_t<T1, T2> >& t) {
  return raw(t.e.left) + raw(t.e.right);
}

template<typename T1, typename T2>
inline
internal_t raw(const Pixel::expressions<Pixel::sub_t<T1, T2> >& t) {
  return raw(t.e.left) - raw(t.e.right);
}

template<typename T>
inline result_t eval(const Pixel::expressions<T>& t) {
  return Pixel::clamp(raw(t), 0.0, 1.0);
}

inline Pixel::data_t r2d(const result_t v) {
  return (Pixel::data_t)Pixel::clamp(((float)Pixel::MAX_VALUE) * v + 0.5f, 0.0f, (Pixel::real)Pixel::MAX_VALUE);
}

inline internal_t r2i(const result_t v) {
  return v;
}

inline Pixel::real r2f(const result_t v) {
  return v;
}

#endif
} } }
#endif
