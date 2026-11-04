"""No display, native application, ptrace, babl or developer headers required.

The exact phase.inc runs against real GObject signals/weak pointers. Only GTK
window enumeration/getters are modeled. Separate introspection checks GTK ABI.
"""
import ctypes as c
from pathlib import Path
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parents[1]
PREFIX = r'''
#define GTK_WIDGET(x) ((GtkWidget *)(x))
#define GTK_WINDOW(x) ((GtkWidget *)(x))
#define G_TYPE_BOOLEAN 20
#define G_TYPE_PARAM 76
#define G_SIGNAL_TYPE_STATIC_SCOPE 1
#define G_SIGNAL_DETAILED 16
#define G_PARAM_READABLE 1
#define G_OBJECT_GET_CLASS(x) ((x)->g_class)
#define G_PARAM_SPEC_VALUE_TYPE(x) ((x)->value_type)
#define GTK_TYPE_STATE_FLAGS flags_type
#define g_signal_connect(o,n,c,d) g_signal_connect_data(o,n,c,d,NULL,0)
typedef struct {GTypeClass *g_class; const char *name; guint flags; GType value_type, owner_type;} GParamSpec;
typedef struct {guint value;const char *value_name,*value_nick;} GFlagsValue;
extern GType g_flags_register_static(const char*,const GFlagsValue*);
extern GParamSpec *g_param_spec_boolean(const char*,const char*,const char*,gboolean,guint);
extern void g_param_spec_unref(GParamSpec*);
extern gboolean g_signal_handler_is_connected(void*,gulong);
static GtkWidget *objects[20];
static GdkFrameClock *clocks[20];
static GObject *theme;
static guint active[20], state[20], count;
static GType flags_type;
static GParamSpec *active_spec;
static gboolean omit_property;
static GParamSpec *model_property(void *klass,const char *name) {
  (void)klass;(void)name;return omit_property?NULL:active_spec;
}
#define g_object_class_find_property model_property
static GType model_type(const char *name){(void)name;return G_TYPE_OBJECT;}
#define g_type_from_name model_type
static gboolean model_is_a(GType type,GType parent){return type==parent;}
#define g_type_is_a model_is_a
static GList *gtk_window_list_toplevels(void){GList *l=NULL;for(int i=(int)count-1;i>=0;i--)l=g_list_prepend(l,objects[i]);return l;}
static guint index_of(GtkWidget *w){for(guint i=0;i<count;i++)if(objects[i]==w)return i;assert(0);return 0;}
static gboolean gtk_window_is_active(GtkWidget *w){return active[index_of(w)];}
static guint gtk_widget_get_state_flags(GtkWidget *w){return state[index_of(w)];}
static GdkFrameClock *gtk_widget_get_frame_clock(GtkWidget *w){return clocks[index_of(w)];}
static GObject *gtk_icon_theme_get_default(void){return theme;}
'''
SUFFIX = r'''
int main(int argc,char **argv) {
  assert(argc==2); const char *mode=argv[1];
  const GFlagsValue flags[]={{1,"one","one"},{128,"backdrop","backdrop"},{0,NULL,NULL}};
  flags_type=g_flags_register_static("PhaseModelFlags",flags);
  active_spec=g_param_spec_boolean("is-active","active","active",FALSE,1);
  count=!strcmp(mode,"zero")?0:!strcmp(mode,"owners")?20:2;
  /* Construct first to initialize GObject and its built-in notify signal. */
  theme=g_object_new(G_TYPE_OBJECT,NULL);
  for(guint i=0;i<count;i++) {
    objects[i]=g_object_new(G_TYPE_OBJECT,NULL);clocks[i]=g_object_new(G_TYPE_OBJECT,NULL);
    active[i]=1;
  }
  g_signal_new("state-flags-changed",G_TYPE_OBJECT,1,0,NULL,NULL,NULL,G_TYPE_NONE,1,flags_type);
  g_signal_new("style-updated",G_TYPE_OBJECT,!strcmp(mode,"nohooks")?65:1,0,NULL,NULL,NULL,
               !strcmp(mode,"badreturn")?G_TYPE_BOOLEAN:G_TYPE_NONE,
               !strcmp(mode,"badshape")?1:0,G_TYPE_BOOLEAN);
  g_signal_new("check-resize",G_TYPE_OBJECT,2,0,NULL,NULL,NULL,G_TYPE_NONE,0);
  g_signal_new("layout",G_TYPE_OBJECT,2,0,NULL,NULL,NULL,G_TYPE_NONE,0);
  g_signal_new("destroy",G_TYPE_OBJECT,76,0,NULL,NULL,NULL,G_TYPE_NONE,0);
  g_signal_new("changed",G_TYPE_OBJECT,2,0,NULL,NULL,NULL,G_TYPE_NONE,0);
  omit_property=!strcmp(mode,"property");
  if(!strcmp(mode,"missingclock")){g_object_unref(clocks[0]);clocks[0]=NULL;}
  phase_observe_begin();phase_observe_begin();
  for(guint role=1;role<=12;role++)phase_observe_mark(role);
  if(count) {
    active[0]=0;state[0]=128;
    g_signal_emit_by_name(objects[0],"notify::is-active",active_spec);
    g_signal_emit_by_name(objects[0],"state-flags-changed",0u);
    if(strcmp(mode,"badshape") && strcmp(mode,"badreturn"))g_signal_emit_by_name(objects[0],"style-updated");
    g_signal_emit_by_name(objects[0],"check-resize");
    if(clocks[0])g_signal_emit_by_name(clocks[0],"layout");
  }
  g_signal_emit_by_name(theme,"changed");
  if(!strcmp(mode,"events"))for(guint i=0;i<300;i++)g_signal_emit_by_name(theme,"changed");
  if(!strcmp(mode,"destroy"))g_signal_emit_by_name(objects[0],"destroy");
  if(!strcmp(mode,"finalize")){g_object_unref(objects[0]);objects[0]=NULL;}
  if(!strcmp(mode,"replace")){g_object_unref(clocks[0]);clocks[0]=g_object_new(G_TYPE_OBJECT,NULL);}
  phase_observe_finish();phase_observe_finish();
  guint events=phase_observation.events;
  if(count && objects[0])g_signal_emit_by_name(objects[0],"check-resize");
  g_signal_emit_by_name(theme,"changed");
  assert(events==phase_observation.events);
  /* Finalizing everything after finish catches stale weak-pointer storage. */
  for(guint i=0;i<count;i++){if(objects[i])g_object_unref(objects[i]);if(clocks[i])g_object_unref(clocks[i]);}
  g_object_unref(theme);g_param_spec_unref(active_spec);
  return 0;
}
'''

class PhaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.directory.name) / 'phase'
        source = cls.exe.with_suffix('.c')
        base = (HERE / 'synthetic-prefix.c').read_text().split('static GtkWidget *objects')[0]
        source.write_text(base + PREFIX + (HERE / 'phase.inc').read_text() + SUFFIX)
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(source),
                        '-o', str(cls.exe), '-l:libgobject-2.0.so.0', '-l:libglib-2.0.so.0'], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_model(self, mode):
        run = subprocess.run([str(self.exe), mode], capture_output=True, text=True, check=True)
        self.assertEqual(run.stderr, '')
        rows = [line.split()[1:] for line in run.stdout.splitlines()]
        self.assertEqual(sum(r[0] == 'complete' for r in rows), 1)
        self.assertEqual(rows[-1], ['complete'])
        for r in rows[:-1]:
            self.assertEqual(len(r), {'summary': 5, 'window': 8, 'event': 5}[r[0]])
            self.assertTrue(all(v.isdecimal() for v in r[1:]))
        summary = list(map(int, rows[0][1:]))
        windows = [list(map(int, r[1:])) for r in rows if r[0] == 'window']
        events = [list(map(int, r[1:])) for r in rows if r[0] == 'event']
        self.assertEqual(summary[:2], [len(windows), len(events)])
        self.assertEqual([e[3] for e in events], sorted(e[3] for e in events))
        return summary, windows, events

    def test_all_signal_roles_and_final_values(self):
        s, w, e = self.run_model('normal')
        self.assertEqual(s, [2, 18, 0, 0])
        self.assertEqual(w[0], [0, 1, 1, 0, 0, 128, 0])
        self.assertEqual([v[:3] for v in e[:12]], [[i, 16, 0] for i in range(1, 13)])
        self.assertEqual([v[:3] for v in e[12:]], [[20, 0, 0], [21, 0, 128],
                         [22, 0, 0], [23, 0, 0], [24, 0, 0], [25, 16, 0]])

    def test_first_events_preserved_and_capacity_explicit(self):
        s, _, events = self.run_model('events')
        self.assertEqual(s, [2, 256, 1, 0])
        self.assertEqual([e[0] for e in events[:12]], list(range(1, 13)))
        self.assertEqual(self.run_model('owners')[0][0:3:2], [16, 1])

    def test_lifetime_and_clock_replacement(self):
        s, w, e = self.run_model('destroy')
        self.assertEqual(w[0][1], 0)
        self.assertEqual(e[-1][:3], [26, 0, 0])
        self.assertEqual(self.run_model('finalize')[1][0][1], 0)
        self.assertEqual(self.run_model('replace')[1][0][-1], 1)

    def test_missing_or_malformed_hooks_explicit(self):
        for mode, missing in [('zero', 1), ('badshape', 2), ('badreturn', 2), ('nohooks', 2), ('property', 2), ('missingclock', 1)]:
            with self.subTest(mode=mode):
                self.assertEqual(self.run_model(mode)[0][3], missing)

    def test_real_gtk_signal_shapes(self):
        g = c.CDLL('libgobject-2.0.so.0')
        gtk = c.CDLL('libgtk-3.so.0')
        gdk = c.CDLL('libgdk-3.so.0')
        class Query(c.Structure):
            _fields_ = [('id', c.c_uint), ('name', c.c_char_p), ('itype', c.c_ulong),
                        ('flags', c.c_uint), ('result', c.c_ulong), ('n', c.c_uint),
                        ('params', c.POINTER(c.c_ulong))]
        g.g_type_class_ref.argtypes = [c.c_ulong]
        g.g_type_class_ref.restype = c.c_void_p
        g.g_type_class_unref.argtypes = [c.c_void_p]
        g.g_signal_lookup.argtypes = [c.c_char_p, c.c_ulong]
        g.g_signal_query.argtypes = [c.c_uint, c.POINTER(Query)]
        gtk.gtk_state_flags_get_type.restype = c.c_ulong
        flags_type = gtk.gtk_state_flags_get_type()
        groups = [(gtk, 'gtk_window_get_type', [('notify', [76], True),
                  ('state-flags-changed', [flags_type], False), ('style-updated', [], False),
                  ('check-resize', [], False), ('destroy', [], True)]),
                  (gdk, 'gdk_frame_clock_get_type', [('layout', [], False)]),
                  (gtk, 'gtk_icon_theme_get_type', [('changed', [], False)])]
        for lib, typename, signals in groups:
            get_type = getattr(lib, typename)
            get_type.restype = c.c_ulong
            kind = get_type()
            klass = g.g_type_class_ref(kind)
            try:
                for name, params, no_hooks in signals:
                    q = Query()
                    g.g_signal_query(g.g_signal_lookup(name.encode(), kind), c.byref(q))
                    self.assertNotEqual(q.id, 0)
                    self.assertEqual(q.result, 4)
                    self.assertEqual([v & ~1 for v in q.params[:q.n]], params)
                    self.assertEqual(bool(q.flags & 64), no_hooks)
            finally:
                g.g_type_class_unref(klass)

if __name__ == '__main__':
    unittest.main()
