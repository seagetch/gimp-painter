"""Bounded system-GObject semantic check, NOT native canvas regression proof."""
import ctypes as c
import json
lib=c.CDLL('/lib/x86_64-linux-gnu/libgobject-2.0.so.0')
lib.g_object_get_type.restype=c.c_size_t
lib.g_object_new.argtypes=[c.c_size_t,c.c_void_p];lib.g_object_new.restype=c.c_void_p
weak_type=c.CFUNCTYPE(None,c.c_void_p,c.c_void_p)
destroy_type=c.CFUNCTYPE(None,c.c_void_p)
lib.g_object_weak_ref.argtypes=[c.c_void_p,weak_type,c.c_void_p]
lib.g_object_set_data_full.argtypes=[c.c_void_p,c.c_char_p,c.c_void_p,destroy_type]
lib.g_object_run_dispose.argtypes=[c.c_void_p]
lib.g_object_unref.argtypes=[c.c_void_p]
events=[]
@weak_type
def weak_notify(data,obj):events.append('weak-notify')
@destroy_type
def release_data(data):events.append('owned-data-release')
obj=lib.g_object_new(lib.g_object_get_type(),None)
assert obj
lib.g_object_weak_ref(obj,weak_notify,None)
lib.g_object_set_data_full(obj,b'owned-data',c.c_void_p(1),release_data)
lib.g_object_run_dispose(obj)
after_dispose=list(events)
assert after_dispose==['weak-notify'],after_dispose
lib.g_object_unref(obj)
after_unref=list(events)
assert after_unref==['weak-notify','owned-data-release'],after_unref
print(json.dumps({'scope':'system GObject semantics only; no GTK canvas execution','after_explicit_dispose_with_owning_reference':after_dispose,'after_final_unref':after_unref,'passed':True},indent=2))
