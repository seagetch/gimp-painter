#ifndef __DELEGATORS_HPP__
#define __DELEGATORS_HPP__

extern "C" {
#include <glib-object.h>
};
#include <utility>
#include <functional>

namespace Delegators {
template<typename T>
void destroy_cxx_object_callback(gpointer ptr) {
  delete reinterpret_cast<T*>(ptr);
}

///////////////////////////////////////////////////////////////////////////////
#include <execinfo.h>
template<typename Decl> class Delegator;
template<typename Ret, typename... Args>
class Delegator<Ret(Args...)>
{
public:
  typedef std::function<Ret (Args...)> Function;

protected:
  Function   func_ref;
  Delegator() {};
public:
  template<typename F> Delegator(F f) : func_ref(f) {};
  virtual ~Delegator() {};

  static Ret callback(Args... args, gpointer ptr) {
//    try {
      Delegator* delegator = reinterpret_cast<Delegator*>(ptr);
      return (*delegator)(args...);
#if 0
    } catch(...) {
      const int trace_size = 10;
      void* trace[trace_size];
      int size = backtrace(trace, trace_size);
      backtrace_symbols_fd(trace, size, 1);
    }
#endif
  }

  Ret operator()(Args... args) {
//    try {
      return func_ref(args...);
#if 0
    } catch(...) {
      const int trace_size = 10;
      void* trace[trace_size];
      int size = backtrace(trace, trace_size);
      backtrace_symbols_fd(trace, size, 1);
    }
#endif
  }
};

///////////////////////////////////////////////////////////////////////////////
template<typename Type, typename Decl> class ObjectDelegator;
template<typename Type, typename Ret, typename... Args>
class ObjectDelegator<Type, Ret(Args...)> : public Delegator<Ret(Args...)>
{
  Type* obj;
  Ret (Type::*f)(Args...);
public:
  ObjectDelegator(Type* o, Ret (Type::*_f)(Args...)) : Delegator<Ret(Args...)>()
  {
    obj = o;
    f = _f;
    this->func_ref = [this](Args... args)->Ret { 
//      try {
      return (obj->*f)(args...); 
      #if 0
      } catch(...) {
        const int trace_size = 10;
        void* trace[trace_size];
        int size = backtrace(trace, trace_size);
        char** symbols = backtrace_symbols(trace, size);
        g_print("%s\n", *symbols);
        free(symbols);
      }
      #endif
    };
//    this->func_ref = std::bind(std::mem_fn(f), obj);
  }
};
///////////////////////////////////////////////////////////////////////////////
template<typename Type, typename Ret, typename... Args>
inline auto
delegator(Type* obj, Ret (Type::*f)(Args...)) 
{
  return new ObjectDelegator<Type, Ret(Args...)>(obj, f);
}

template<typename Ret, typename... Args>
inline auto
delegator(std::function<Ret (Args...)> f)
{
  return new Delegator<Ret(Args...)>(f);
}

template<typename Ret, typename... Args>
inline auto
delegator(Ret (*f)(Args...)) {
  return new Delegator<Ret (Args...)>(f);
}

///////////////////////////////////////////////////////////////////////////////
class Connection {
  gulong    handler_id;
  GObject*  target;
  gchar*    signal;
  GClosure* closure;
public:
  Connection(gulong handler_id_,
             GObject* target_,
             const gchar*   signal_,
             GClosure* closure_)
  {
    handler_id = handler_id_;
    target  = target_;
    signal  = g_strdup(signal_);
    closure = closure_;
  };

  ~Connection()
  {
      disconnect();
  }
  
  bool is_valid() { return handler_id > 0; }

  void disconnect() {
    if (target && closure) {
      gulong handler_id = g_signal_handler_find(gpointer(target), G_SIGNAL_MATCH_CLOSURE, 0, 0, closure, NULL, NULL);
      if (handler_id > 0)
        g_signal_handler_disconnect(gpointer(target), handler_id);
    }
    target  = NULL;
    closure = NULL;
  };
  
  void block() {
    if (target)
      g_signal_handler_block (target, handler_id);
  }
  
  void unblock() {
    if (target)
      g_signal_handler_unblock (target, handler_id);
  }
};


}; // namespace
///////////////////////////////////////////////////////////////////////////////
template<typename T>
void closure_destroy_notify(gpointer data, GClosure* closure)
{
  Delegators::destroy_cxx_object_callback<T>(data);
}

template<typename Ret, typename... Args>
Delegators::Connection *
g_signal_connect_delegator (GObject* target, 
                            const gchar* event, 
                            Delegators::Delegator<Ret(Args...)>* delegator,
                            bool after=false)
{
  GClosure *closure;  
  closure = g_cclosure_new (G_CALLBACK (Delegators::Delegator<Ret(Args...)>::callback),
                            (gpointer)delegator, 
                            closure_destroy_notify<Delegators::Delegator<Ret(Args...)> >);
  gulong handler_id = g_signal_connect_closure (target, event, closure, after);
  return new Delegators::Connection(handler_id, target, event, closure);
}

template<typename Ret, typename... Args>
void
g_signal_connect_delegator_noret (GObject* target, 
                                  const gchar* event, 
                                  Delegators::Delegator<Ret(Args...)>* delegator,
                                  bool after=false)
{
  GClosure *closure;  
  closure = g_cclosure_new (G_CALLBACK (Delegators::Delegator<Ret(Args...)>::callback),
                            (gpointer)delegator, 
                            closure_destroy_notify<Delegators::Delegator<Ret(Args...)> >);
  gulong handler_id = g_signal_connect_closure (target, event, closure, after);
}

template<typename T>
inline void g_object_set_cxx_object (GObject* target, const gchar* key, T* object)
{
  g_object_set_data_full(target, key, (gpointer)object, Delegators::destroy_cxx_object_callback<T>);
}

template<typename T>
inline void g_object_set_cxx_object (GObject* target, const GQuark key, T* object)
{
  g_object_set_qdata_full(target, key, (gpointer)object, Delegators::destroy_cxx_object_callback<T>);
}

inline gulong
g_signal_handler_get_id(GObject* target, GClosure* closure)
{
  return g_signal_handler_find(gpointer(target), G_SIGNAL_MATCH_CLOSURE, 0, 0, closure, NULL, NULL);
}

inline void
g_signal_disconnect (GObject* target,
                     GClosure* closure)
{
  gulong handler_id = g_signal_handler_get_id(target, closure);
	g_signal_handler_disconnect(gpointer(target), handler_id);
}

inline void
g_signal_disconnect (GObject* target,
                     gulong handler_id)
{
	g_signal_handler_disconnect(gpointer(target), handler_id);
}

template<typename Ret, typename... Args>
decltype(&Delegators::Delegator<Ret(Args...)>::callback) proxy(Ret (*f)(Args...)) {
  return &Delegators::Delegator<Ret(Args...)>::callback;
}

template<typename Ret, typename... Args>
decltype(&Delegators::Delegator<Ret(Args...)>::callback) proxy(std::function<Ret(Args...)>& f) {
  return &Delegators::Delegator<Ret(Args...)>::callback;
}

#endif
