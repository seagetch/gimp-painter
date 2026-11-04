"""Actual system GLib test selector behavior; no GIMP/GUI/sanitizers."""
import ctypes as C,sys
lib=C.CDLL('libglib-2.0.so.0')
argv=(C.c_char_p*(len(sys.argv)+1))(*[x.encode() for x in sys.argv],None)
ptr=C.cast(argv,C.POINTER(C.c_char_p));argc=C.c_int(len(sys.argv))
lib.g_test_init(C.byref(argc),C.byref(ptr),None)
callback=C.CFUNCTYPE(None)
@callback
def noop():pass
lib.g_test_add_func.argtypes=[C.c_char_p,callback]
for name in [b'/ownership/display/parity',b'/ownership/renderer/public-smoke']:lib.g_test_add_func(name,noop)
sys.exit(lib.g_test_run())
