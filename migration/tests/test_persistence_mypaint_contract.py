#!/usr/bin/env python3
"""10.001 MyPaint saved-field metadata contract (source-only; never runs GIMP).

The keyed population comes from four pinned C initializer tables, independently
of the editorial ledger/native metadata JSON. Native registration templates,
resource read/write expressions and fixed JSON grammar are checked explicitly.
This certifies source metadata, not runtime equivalence or property ordering.
"""
from __future__ import annotations
from persistence_successors import resolve as resolve_source_successor
import argparse
import csv
import hashlib
from functools import lru_cache
import json
from pathlib import Path
import re
import subprocess

LEGACY = 'afa43fae3e920210146abed514f136fd49f671b5'
RESOURCE = 'app/paint/painter-mypaint/resource.cpp'
OPTIONS = 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'
METADATA = 'app/paint/painter-brush-settings/brush-settings.json'
LEGACY_METADATA = 'app/core/mypaintbrush-brushsettings.c'
LOAD = 'app/core/gimpmypaintbrush-load.cpp'
SAVE = 'app/core/gimpmypaintbrush-save.cpp'
LEGACY_OPTIONS = 'app/paint/gimpmypaintoptions.cpp'
BRUSH = 'app/core/gimppaintermybrush.cpp'
TEST_FILE = 'migration/tests/test_persistence_mypaint_contract.py'


@lru_cache(maxsize=None)
def source(root, path, revision='native'):
    if revision == 'native':
        return (Path(root) / path).read_text()
    assert revision == 'legacy', revision
    return subprocess.check_output(['git', 'show', LEGACY + ':' + path],
                                   cwd=root, text=True)


def _clean(text):
    # Keep quoted strings intact while removing comments.
    return re.sub(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|/\*.*?\*/|//[^\n]*',
                  lambda m: m[1] or '', text, flags=re.S)


def _block(text, signature):
    text = _clean(text)
    start = text.index(signature)
    brace = text.index('{', start)
    tokens = re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[brace:])
    depth = 0
    for token in tokens:
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0: return text[brace + 1:brace + token.start()]
    raise AssertionError('unclosed scope ' + signature)


def _require_scope(text, signature, *expressions):
    body = _block(text, signature)
    for expression in expressions:
        assert expression in body, (signature, expression)
    return body


def extract_legacy_tables(root):
    """Read names, defaults and bounds from genuine pinned C, not ledger JSON."""
    text = source(root, LEGACY_METADATA, 'legacy')
    out = {}
    scalar = r'([+-]?(?:\d+(?:\.\d*)?|\.\d+))'
    quoted = r'"(?:\\.|[^"\\])*"'
    patterns = {
        'settings': r'\{\s*"([a-z0-9_]+)"\s*,\s*(BRUSH_[A-Z0-9_]+)\s*,\s*_\('+quoted+r'\)\s*,\s*(?:True|False)\s*,\s*'+scalar+r'\s*,\s*'+scalar+r'\s*,\s*'+scalar,
        'switches': r'\{\s*"([a-z0-9_]+)"\s*,\s*(BRUSH_[A-Z0-9_]+)\s*,\s*_\('+quoted+r'\)\s*,\s*(TRUE|FALSE)',
        'texts': r'\{\s*"([a-z0-9_]+)"\s*,\s*(BRUSH_[A-Z0-9_]+)\s*,\s*_\('+quoted+r'\)\s*,\s*(NULL)',
        'inputs': r'\{\s*"([a-z0-9_]+)"\s*,\s*(INPUT_[A-Z0-9_]+)\s*,'
    }
    for kind, table in [('settings','settings_list'), ('switches','switches_list'),
                        ('texts','text_list'), ('inputs','inputs_list')]:
        scope = _block(text, table+'[]')
        matches = re.findall(patterns[kind], scope)
        assert matches, table
        names = [m[0] for m in matches]
        assert len(names) == len(set(names)), table
        out[kind] = matches
    return out


def expected_members(root):
    table = extract_legacy_tables(root)
    result = set(FIXED_RULES)
    for kind in ('settings','switches','texts'):
        for item in table[kind]:
            result.add('mypaint.resource.'+item[0])
            result.add('mypaint.option.'+item[0])
            if kind != 'texts': result.add('mypaint.text.'+item[0])
    for item in table['inputs']: result.add('mypaint.curve.'+item[0])
    return result


def _validate_source_semantics(root):
    r = source(root, RESOURCE); o = source(root, OPTIONS)
    old = source(root, LOAD, 'legacy'); oldo = source(root, LEGACY_OPTIONS, 'legacy')
    oldsave = source(root, SAVE, 'legacy')
    # Scoped checks tie semantic assertions to their actual reader/writer owners.
    header=source(root,'app/paint/painter-mypaint/resource.hpp')
    assert 'namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {' in header
    _require_scope(header,'class Resource', 'std::unique_ptr<Impl> impl_;')
    private=source(root,'app/core/gimpmypaintbrush-private.hpp','legacy')
    _require_scope(private,'class GimpMypaintBrushPrivate',
       'float base_value;', 'bool   switches[BRUSH_BOOL_COUNT];', 'char* text[BRUSH_TEXT_COUNT];')
    _require_scope(r,'double number (JsonNode *n',
       'json_node_get_value_type (n) != G_TYPE_INT64 && json_node_get_value_type (n) != G_TYPE_DOUBLE',
       'double result = json_node_get_double (n);', 'if (!std::isfinite (result))')
    _require_scope(r, 'double Resource::base_value',
       'if (v) return number (member (v, "base_value"), key);',
       'return painter_mypaint_settings[id].default_value;')
    _require_scope(r, 'void Resource::set_base_value',
       'finite_value (value);', 'json_object_set_double_member (v, "base_value", value);')
    _require_scope(r, 'bool Resource::switch_value', 'if (!n) return false;',
       'json_node_get_value_type (n) != G_TYPE_BOOLEAN', 'return json_node_get_boolean (n);')
    _require_scope(r, 'const char *Resource::peek_text',
       'if (!n || JSON_NODE_HOLDS_NULL (n)) return nullptr;',
       'json_node_get_value_type (n) != G_TYPE_STRING', 'return json_node_get_string (n);')
    _require_scope(r, 'void Resource::set_text',
       'if (value) json_object_set_string_member (s, key, value); else json_object_set_null_member (s, key);')
    _require_scope(r, 'void Resource::set_switch',
       'json_object_set_boolean_member (section (impl_->object (), "switches", true), switch_name (id), value);')
    _require_scope(r, 'void finite_value',
       '!std::isfinite (value) || std::abs (value) > std::numeric_limits<float>::max ()')
    _require_scope(r, 'std::vector<Point> Resource::curve',
       'if (id == BRUSH_OPAQUE_MULTIPLY && input == INPUT_PRESSURE) return {{0,0},{1,1}};',
       'json_array_get_length (json_node_get_array (p)) != 2',
       'json_array_get_element (pair, 0), "curve.x"', 'json_array_get_element (pair, 1), "curve.y"',
       'finite_value (x); finite_value (y); points.push_back ({x,y});')
    _require_scope(r, 'void Resource::set_curve',
       'const double base = base_value (id);',
       'if (!member (v, "base_value")) json_object_set_double_member (v, "base_value", base);',
       'json_array_add_double_element (pair, p.x); json_array_add_double_element (pair, p.y);',
       'json_object_set_array_member (ins, input_name (input), array);')
    _require_scope(old, 'MyPaintBrushReader::load_defaults',
       'priv->set_base_value(setting->index, setting->default_value);',
       'v->mapping->set_point(INPUT_PRESSURE, 0, 0.0, 0.0);',
       'v->mapping->set_point(INPUT_PRESSURE, 1, 1.0, 1.0);')
    _require_scope(source(root,'app/core/gimpmypaintbrush.cpp','legacy'),
       'GimpMypaintBrushPrivate::GimpMypaintBrushPrivate()',
       'parent_brush_name = g_strdup("");', 'group = g_strdup("");',
       'settings[i].base_value = 0;', 'switches[i] = FALSE;', 'text[i] = NULL;', 'icon_image = NULL;')
    init = _require_scope(o, 'static void gimp_painter_mybrush_options_class_init',
       'const auto flags=GParamFlags(G_PARAM_READWRITE|GIMP_CONFIG_PARAM_SERIALIZE);')
    assert set(re.findall(r'properties\[(PROP_\w+)\]\s*=',init)) == {'PROP_JSON','PROP_DIRTY','PROP_CONFLICT','PROP_BRUSH_MODE','PROP_DRAFT_NAME','PROP_NAME_DIRTY'}
    registrations = re.findall(r'for\(const auto&s:(painter_mypaint_\w+)\)\{properties\[s.index\+1\]=(g_param_spec_\w+)\(([^;]+)\);\}', init)
    assert registrations == [
      ('painter_mypaint_settings','g_param_spec_double','s.internal_name,s.displayed_name,s.tooltip,s.minimum,s.maximum,s.default_value,flags'),
      ('painter_mypaint_switches','g_param_spec_boolean','s.internal_name,s.displayed_name,nullptr,FALSE,flags'),
      ('painter_mypaint_texts','g_param_spec_string','s.internal_name,s.displayed_name,nullptr,nullptr,flags')], registrations
    transient = re.findall(r'properties\[(PROP_\w+)\]=g_param_spec_\w+\(((?:"(?:\\.|[^"\\])*"|[^;"])*?)G_PARAM_READABLE\);',init,re.S)
    assert {key for key,_ in transient} == {'PROP_DIRTY','PROP_CONFLICT','PROP_DRAFT_NAME','PROP_NAME_DIRTY'}
    for key, expr in transient:
        assert 'flags' not in expr and 'SERIALIZE' not in expr, key
    _require_scope(o,'void set_property(GObject*object',
       'if(!json)return;',
       'if(id<=BRUSH_MAPPING_END)next.set_base_value(id-1,g_value_get_double(value));',
       'else if(id<=BRUSH_BOOL_END)next.set_switch(id-1,g_value_get_boolean(value));',
       'else next.set_text(id-1,g_value_get_string(value));')
    _require_scope(o,'void get_property(GObject*object',
       'g_value_set_string(value,i.draft.encode().c_str());',
       'g_value_set_double(value,i.draft.base_value(id-1));',
       'g_value_set_boolean(value,i.draft.switch_value(id-1));',
       'g_value_set_string(value,i.draft.text_is_null(id-1)?nullptr:i.draft.text_value(id-1).c_str());')
    old_init = _block(oldo, 'gimp_mypaint_options_class_init (')
    for typ in ('DOUBLE','BOOLEAN','STRING'):
        call = re.search(r'GIMP_CONFIG_INSTALL_PROP_'+typ+r'\s*\((.*?);',old_init,re.S)[1]
        assert 'setting->index + 1' in call and 'signal_name' in call
        assert 'setting->default_value' in call and 'GIMP_PARAM_STATIC_STRINGS' in call
    _require_scope(oldsave,'MypaintBrushWriter::save_brush', 'JsonNode* root = build_json();','save_icon (icon_fullpath.ptr());')
    assert 'write_file' not in _block(oldsave,'MypaintBrushWriter::save_brush')
    _require_scope(oldsave,'MypaintBrushWriter::build_json',
       'json_builder_add_double_value(builder, priv->get_base_value(setting->index));',
       'json_builder_add_boolean_value(builder, value);', 'json_builder_add_string_value(builder, value);',
       'if (n == 0)', 'json_builder_add_double_value(builder, x);', 'json_builder_add_double_value(builder, y);')
    _require_scope(r,'Resource Resource::decode',
       'int version = 1; bool seen_version = false;',
       'version < 3 || version > G_MAXINT',
       'json_object_set_int_member (legacy, "version", version);',
       'json_object_set_string_member (legacy, "source", text.c_str ());',
       'json_object_set_string_member (raw, key.c_str (), value.c_str ());',
       'if (id < 0) continue;',
       'r.set_base_value (id, migrate (key, ascii_number (parts[0])));')
    _require_scope(old,'MyPaintBrushReader::read_file_v2', 'version = g_ascii_strtoll (tokens[1], NULL, 0);', 'if (version > CURRENT_BRUSHFILE_VERSION)')
    # Native root-member census is derived from literal root/impl_->object() accesses.
    root_names = set(re.findall(r'(?:member|section|json_object_(?:set_int_member|set_string_member|remove_member))\s*\(\s*(?:root|impl_->object\s*\(\s*\)|r.impl_->object\s*\(\s*\)|json_node_get_object\s*\(root.get\s*\(\s*\)\))\s*,\s*"([a-z_]+)"',r))
    assert root_names == {'version','parent_brush_name','group','settings','switches','texts','painter_legacy','painter_preview_png'},root_names
    json_reader_keys = set(re.findall(r'json_reader_read_member\(reader, "([a-z_]+)"\)',_block(old,'MyPaintBrushReader::parse_v3')))
    assert json_reader_keys == {'version','parent_brush_name','group','settings','base_value','inputs','switches','texts'}
    _require_scope(source(root,BRUSH),'GList *gimp_painter_mybrush_load',
       'if (!embedded.empty ()) {',
       'if (embedded.empty () && path && g_str_has_suffix (path.get (), ".myb"))',
       'icon_path += "_prev.png";',
       'if (icon && !gimp_painter_mybrush_set_icon (owner.get (), icon.get (), error))')
    brush = source(root,BRUSH)
    encoder = _require_scope(brush, 'bool encode_icon (GdkPixbuf *icon',
       'encoded.clear ();', 'if (!icon) return true;',
       'gdk_pixbuf_save_to_buffer (icon, &raw, &size, "png", error, nullptr)',
       'String bytes (raw);', 'if (!success) return false;',
       'g_base64_encode (reinterpret_cast<const guchar *> (raw), size)',
       'encoded = base64.get ();')
    assert encoder.index('String bytes (raw);') < encoder.index('if (!success) return false;')
    setter = _require_scope(brush,'gboolean gimp_painter_mybrush_set_icon',
       'auto icon = ObjectRef<GdkPixbuf>::retain (pixbuf);',
       'if (!encode_icon (icon.get (), encoded, error)) return FALSE;',
       'impl.resource.set_preview_png_base64 (encoded);')
    assert setter.index('if (!encode_icon') < setter.index('impl.resource.set_preview_png_base64')
    _require_scope(r,'std::vector<Point> parse_legacy_curve',
       'if (result.size () == 1 || result.size () > 8)',
       'if (result[i].x < result[i-1].x)')
    modes = re.findall(r'\{([0-3]), "(GIMP_MYPAINT_[A-Z_]+)", "([a-z-]+)"\}',_block(o,'GType legacy_brush_mode_type'))
    assert modes == [('0','GIMP_MYPAINT_NORMAL','normal'),('1','GIMP_MYPAINT_NORMAL_AND_ERASE','normal-and-erase'),('2','GIMP_MYPAINT_LOCK_ALPHA','lock-alpha'),('3','GIMP_MYPAINT_COLORIZE','colorize')]
    paint = source(root,'app/paint/gimppaintoptions.c')
    for prop,typ,args in [
      ('brush-view-type','ENUM',['GIMP_TYPE_VIEW_TYPE','GIMP_VIEW_TYPE_GRID']),
      ('brush-view-size','INT',['GIMP_VIEW_SIZE_TINY','GIMP_VIEWABLE_MAX_BUTTON_SIZE','GIMP_VIEW_SIZE_SMALL'])]:
        match = re.search(r'GIMP_CONFIG_PROP_'+typ+r'\s*\([^;]*"'+prop+r'"([^;]*);',paint,re.S)
        assert match,prop
        assert all(a in match[1] for a in args)
    _require_scope(r, 'struct Resource::Impl',
       'Node root = object_node ();', 'int source_version = 3;',
       'json_object_set_int_member (json_node_get_object (root.get ()), "version", 3);')
    _require_scope(r, 'Resource Resource::decode',
       'if (parent && json_string (parent, "parent_brush_name").empty () && !source_basename.empty ())',
       'r.set_parent_brush_name (source_basename);',
       'if (idx < 0) {',
       'json_object_set_string_member (section (setting, "inputs", true), input.c_str (),',
       'strip (parts[i].substr (sep+1)).c_str ());',
       'r.set_base_value (BRUSH_COLOR_H, h); r.set_base_value (BRUSH_COLOR_S, s); r.set_base_value (BRUSH_COLOR_V, max);',
       'red /= 255; green /= 255; blue /= 255;')
    for member_name, accessor in [('parent_brush_name','parent_brush_name'),('group','group'),('painter_preview_png','preview_png_base64')]:
        _require_scope(r,'std::string Resource::'+accessor+' () const',
          'return n ? json_string (n, "'+member_name+'") : "";')
    _require_scope(r,'void Resource::set_preview_png_base64',
       'if (value.empty ()) json_object_remove_member (impl_->object (), "painter_preview_png");',
       'else json_object_set_string_member (impl_->object (), "painter_preview_png", value.c_str ());')
    serializer = _require_scope(brush,'gboolean save (GimpData *data',
       'auto snapshot = impl.resource;', 'auto icon = impl.icon;',
       'const char *name = gimp_object_get_name (data);',
       'snapshot.set_parent_brush_name (name ? name : "");',
       'if (!encode_icon (icon.get (), encoded, error)) return false;',
       'snapshot.set_preview_png_base64 (encoded);',
       'return snapshot.save (output, nullptr, error);')
    assert serializer.index('if (!encode_icon') < serializer.index('snapshot.set_preview_png_base64') < serializer.index('return snapshot.save')
    assert 'impl.resource.set_' not in serializer, 'save must not mutate the source resource'

    _require_scope(r,'float migrate (const std::string& key, double value)',
       'const float f = value;', 'if (key == "color_hue") return f*64.0/360.0;',
       'if (key == "color_saturation" || key == "color_value") return f*128.0/256.0;', 'return f;')
    _require_scope(r,'std::string migrated_name',
       'if (key == "color_hue") return "change_color_h";',
       'if (key == "color_saturation" || key == "change_color_s") return "change_color_hsv_s";',
       'if (key == "color_value") return "change_color_v";',
       'if (key == "speed_slowness") return "speed1_slowness";',
       'if (key == "stroke_treshold") return "stroke_threshold";')
    _require_scope(r,'std::vector<Point> parse_legacy_curve',
       'finite_value (x); finite_value (y); result.push_back ({x, migrate (key, y)});')
    _require_scope(r,'Node clone (JsonNode *n)',
       'json_object_set_member (dst, key, clone (member (src, key)).release ());',
       'json_array_add_element (dst, clone (json_array_get_element (src, i)).release ());',
       'return Node (json_node_copy (n));')
    assert 'std::string Resource::encode () const { return encode_node (impl_->root.get ()); }' in r
    _require_scope(r,'std::string encode_node (JsonNode *node)',
       'json_generator_set_root (gen.get (), node);', 'json_generator_to_data (gen.get (), nullptr)')
    mapping=source(root,'app/paint/painter-mypaint/mapping.hpp')
    _require_scope(mapping,'void set_n (int input, int n)', 'if (n < 0 || n > 8 || n == 1)')
    _require_scope(mapping,'void set_point (int input, int index, float x, float y)',
       '!std::isfinite (x) || !std::isfinite (y)', '(index && x < p.xvalues[index-1])')
    assert 'G_DEFINE_TYPE_WITH_CODE (GimpPainterMybrushOptions, gimp_painter_mybrush_options, GIMP_TYPE_PAINT_OPTIONS,' in o
    assert 'G_DEFINE_TYPE (GimpMypaintOptions, gimp_mypaint_options, GIMP_TYPE_TOOL_OPTIONS);' in oldo
    assert 'struct OptionsImpl {' in o and 'Resource draft;' in _block(o,'struct OptionsImpl')
    _require_scope(source(root,'app/core/gimpcontext.c'),'gimp_context_serialize (GimpConfig',
       'return gimp_config_serialize_changed_properties (config, writer);')
    _require_scope(source(root,'libgimpconfig/gimpconfig-serialize.c'),
       'gimp_config_serialize_changed_properties (GimpConfig',
       'if (! (prop_spec->flags & GIMP_CONFIG_PARAM_SERIALIZE))',
       'if (! g_param_value_defaults (prop_spec, &value))')
    legacy_macro=source(root,'libgimpconfig/gimpconfig-params.h','legacy')
    macro_flags=re.search(r'#define GIMP_CONFIG_PARAM_FLAGS\s*\(([^)]+)\)',legacy_macro)[1]
    assert set(re.findall(r'G_[A-Z_]+|GIMP_[A-Z_]+',macro_flags)) == {'G_PARAM_READWRITE','G_PARAM_CONSTRUCT','GIMP_CONFIG_PARAM_SERIALIZE'}
    for spec in ('DOUBLE','BOOLEAN','STRING'):
        macro=re.search(r'#define GIMP_CONFIG_INSTALL_PROP_'+spec+r'\([^\n]+\)(.*?)(?=\n\n)',legacy_macro,re.S)[1]
        assert 'flags | GIMP_CONFIG_PARAM_FLAGS' in macro
    generator=source(root,'app/paint/painter-brush-settings/generate.py')
    assert 'float minimum, default_value, maximum;' in generator
    assert "[number(e[f]) for f in ('minimum', 'default_value', 'maximum')]" in generator
    assert len(manifest_states := json.loads(source(root,METADATA))['states'])==30
    assert 'painter_mypaint_states' not in r and 'painter_mypaint_states' not in o
    return registrations


def _equal(row, expected):
    for key,value in expected.items():
        assert row.get(key) == value, (row['id'], key, row.get(key), value)


def validate_family(data, root):
    """Return the exact executed assertion IDs, raising on any failed contract."""
    root = Path(root)
    source.cache_clear()  # Validate the current source even when called repeatedly by mutation tests.
    _validate_source_semantics(root)
    table = extract_legacy_tables(root)
    manifest = json.loads((root/METADATA).read_text())
    by_id = {row['id']:row for row in data['fields']}
    assert len(by_id) == len(data['fields']), 'duplicate field ID'
    assert set(by_id) == expected_members(root), ('field completeness',set(by_id)^expected_members(root))
    for kind, entries in table.items():
        native = manifest[kind]
        assert [(e['name'],e['symbol']) for e in native] == [(e[0],e[1]) for e in entries],kind
        for offset, (c, m) in enumerate(zip(entries,native)):
            expected_index = offset + {'settings':0,'switches':45,'texts':50,'inputs':0}[kind]
            assert m['index'] == expected_index,(kind,m)
            if kind == 'settings':
                assert (m['minimum'],m['default_value'],m['maximum']) == c[2:],c
                default = f'Native loaded/missing default: double(float32({c[3]})); legacy loaded default: float32({c[3]}). Legacy raw private constructor is 0 before load_defaults.'
                option_default = f'Native and legacy GParamSpec default: double(float32({c[3]})); minimum double(float32({c[2]})); maximum double(float32({c[4]})).'
            elif kind == 'switches':
                assert m['default_value'] == (c[2]=='TRUE'),c
                default = 'Native and legacy resource constructor/missing default: FALSE; metadata switch default is not loaded into the resource.'
                option_default = f'Native GParamSpec default: FALSE; legacy GParamSpec metadata default: {c[2]}. Resource missing default remains FALSE.'
            elif kind == 'texts':
                assert m['default_value'] is None,c
                default = 'Native and legacy resource constructor/missing default: NULL; native text_value convenience accessor returns empty string for NULL.'
                option_default = 'Native and legacy GParamSpec default: NULL; explicit empty string is distinct.'
            else:
                row = by_id['mypaint.curve.'+c[0]]
                _equal(row, {**CURVE_RULE,
                  'saved_name':'settings.{declared-setting}.inputs.'+c[0],
                  'domain':{'kind':'inputs','name':c[0],'index':expected_index},
                  'default':'For opaque_multiply only: [[0,0],[1,1]]; other settings: empty.' if c[0]=='pressure' else 'Empty for every declared setting.'})
                continue
            domain = {'kind':kind,'name':c[0],'index':expected_index}
            _equal(by_id['mypaint.resource.'+c[0]], {**SCALAR_RULES[kind]['resource'],
              'domain':domain,'saved_name':kind+'.'+c[0]+('.base_value' if kind=='settings' else ''),'default':default})
            _equal(by_id['mypaint.option.'+c[0]], {**SCALAR_RULES[kind]['option'],
              'domain':domain,'saved_name':c[0].replace('_','-'),'default':option_default})
            if kind != 'texts':
                _equal(by_id['mypaint.text.'+c[0]],{**SCALAR_RULES[kind]['text'],
                  'domain':domain,'saved_name':c[0]+(' <base> [| <input> <points> ...]' if kind=='settings' else ' <0|1>'),
                  'native_type':'UTF-8 numeric -> float32 via migrate -> double canonical settings.'+c[0]+'.base_value' if kind=='settings' else SCALAR_RULES[kind]['text']['native_type'],
                  'default':default if kind=='settings' else 'Resource switch remains FALSE; raw line has no synthesized default.'})
    for id, rule in FIXED_RULES.items(): _equal(by_id[id],rule)
    # Schema aliases are checked against genuine old migration initializer tuples,
    # rather than trusting metadata manifest or the ledger's aliases.
    old_meta = _clean(source(root,LEGACY_METADATA,'legacy'))
    aliases = re.findall(r'\{"([a-z_]+)", "([a-z0-9_]+)", (transform_[a-z_]+|NULL)\}',old_meta)
    assert {(a,b) for a,b,_ in aliases} == {(m['old_name'],m['new_name']) for m in manifest['legacy_migrations']}
    native_alias_keys=set(re.findall(r'key == "([a-z_]+)"',_block(source(root,RESOURCE),'std::string migrated_name')))
    assert native_alias_keys == {a for a,_,_ in aliases}, 'native alias completeness'
    assert {'mypaint.text.alias.'+a for a,_,_ in aliases} == {id for id in by_id if id.startswith('mypaint.text.alias.')}
    for old,new,transform in aliases:
        _equal(by_id['mypaint.text.alias.'+old],{'domain':{'kind':'alias','name':old,'target':new,'transform':{'color_hue':'float32(float32(value)*64.0/360.0)','color_saturation':'float32(float32(value)*128.0/256.0)','color_value':'float32(float32(value)*128.0/256.0)'}.get(old,'float32(value)')}})
    candidates = {row['legacy_site']:row for row in data['candidates']}
    assert len(candidates)==len(data['candidates'])
    census = {r['legacy_site']:r for r in csv.DictReader((root/'migration/inventory/persistence-sites.tsv').open(),delimiter='\t') if r['legacy_site'].split(':')[0] in {LOAD,SAVE,LEGACY_OPTIONS}}
    assert set(candidates)==set(census), 'candidate completeness'
    for site in census:
        assert candidates[site] == CANDIDATE_RULES[site], ('candidate assignment',site)
        path, line = site.rsplit(':',1)
        assert census[site]['operation'] in source(root,path,'legacy').splitlines()[int(line)-1],site
        assert all(id in by_id for id in candidates[site]['fields']),site
    assertions=[]
    for id,row in by_id.items():
        assert row['assertion']==id+'.metadata',id
        assert row['test_file']==TEST_FILE,id
        assert row['route'] in {'07','08','09','12'},id
        assert row['storage']==STORAGE[row['route']] and row['version']==VERSIONS[row['route']],id
        for key in ('reader','writer','legacy_reader','legacy_writer'):
            assert row[key],(id,key)
            for location in row[key]:
                path, anchor=location.split('#',1)
                assert anchor in source(root,path,'legacy' if key.startswith('legacy') else 'native'),(id,key,location)
        assert row['evidence'], (id,'empty evidence')
        for evidence in row['evidence']:
            text=source(root,evidence['path'],evidence['revision'])
            assert evidence['expected'] in text,(id,evidence)
            assert evidence['anchor'] in text,(id,'evidence anchor',evidence)
        assertions.append(row['assertion'])
    actual_sources={(r['revision'],r['path']):r['sha256'] for r in data['source_files']}
    assert set(actual_sources)==SOURCE_FILES,'source manifest completeness'
    for (revision,path),digest in actual_sources.items():
        actual_digest = hashlib.sha256(source(root,path,revision).encode()).hexdigest()
        resolve_source_successor(root, revision, path, digest, actual_digest)
    assert data['unresolved']==[]
    return assertions


# Fixed grammar/specification expectations, distinct from the field ledger.
# Explicit expression checks above establish their reader/writer semantics.
FIXED_RULES = {'mypaint.json.version': {'route': '07',
                          'disposition': 'saved-field',
                          'saved_name': 'version',
                          'native_type': 'JSON integral number -> int',
                          'legacy_type': 'JSON integer -> gint64',
                          'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                          'default': 'Native new resource version = 3; legacy writer '
                                     'CURRENT_BRUSHFILE_VERSION = 3.',
                          'absence': 'Native requires integer >=3 and <=G_MAXINT; legacy absent version '
                                     'leaves reader version 0.',
                          'flags': 'Native preserves future >=3 version and diagnoses unsupported semantics; '
                                   'legacy reads version without enforcing current match.',
                          'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                          'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                          'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                          'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.parent': {'route': '07',
                         'disposition': 'saved-field',
                         'saved_name': 'parent_brush_name',
                         'native_type': 'JSON UTF-8 string -> std::string',
                         'legacy_type': 'JSON string -> owned char*',
                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                         'default': 'Native and legacy private parent name default: empty string.',
                         'absence': 'Absent leaves empty parent and brush filename-derived display name; '
                                    'explicit empty JSON parent becomes source basename when provided.',
                         'flags': 'Resource::encode retains stored parent; native GimpPainterMybrush save '
                                  'overwrites the snapshot parent with current brush object name, matching '
                                  'legacy writer.',
                         'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::parent_brush_name'],
                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode',
                                    'app/core/gimppaintermybrush.cpp#save'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.group': {'route': '07',
                        'disposition': 'saved-field',
                        'saved_name': 'group',
                        'native_type': 'JSON UTF-8 string -> std::string',
                        'legacy_type': 'JSON string -> owned char*',
                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                        'default': 'Native and legacy private group default: empty string.',
                        'absence': 'Absent returns empty string without materializing native member.',
                        'flags': 'Legacy always emits group; native retains or explicitly sets member.',
                        'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::group'],
                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.settings': {'route': '07',
                           'disposition': 'dynamic-schema',
                           'saved_name': 'settings',
                           'native_type': 'JSON object; known members as listed, unknown values opaque',
                           'legacy_type': 'JSON object consumed via JsonReader',
                           'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                           'default': 'No native section materialized by Resource constructor; legacy reader '
                                      'leaves loaded defaults.',
                           'absence': 'Absent section leaves per-member defaults; malformed native section '
                                      'rejected.',
                           'flags': 'Native lossless subtree; legacy writer emits section and every declared '
                                    'scalar.',
                           'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                           'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                           'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                           'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.switches': {'route': '07',
                           'disposition': 'dynamic-schema',
                           'saved_name': 'switches',
                           'native_type': 'JSON object; known members as listed, unknown values opaque',
                           'legacy_type': 'JSON object consumed via JsonReader',
                           'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                           'default': 'No native section materialized by Resource constructor; legacy reader '
                                      'leaves loaded defaults.',
                           'absence': 'Absent section leaves per-member defaults; malformed native section '
                                      'rejected.',
                           'flags': 'Native lossless subtree; legacy writer emits section and every declared '
                                    'scalar.',
                           'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::switch_value'],
                           'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                           'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                           'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.texts': {'route': '07',
                        'disposition': 'dynamic-schema',
                        'saved_name': 'texts',
                        'native_type': 'JSON object; known members as listed, unknown values opaque',
                        'legacy_type': 'JSON object consumed via JsonReader',
                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                        'default': 'No native section materialized by Resource constructor; legacy reader '
                                   'leaves loaded defaults.',
                        'absence': 'Absent section leaves per-member defaults; malformed native section '
                                   'rejected.',
                        'flags': 'Native lossless subtree; legacy writer emits section and every declared '
                                 'scalar.',
                        'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::peek_text'],
                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.setting': {'route': '07',
                          'disposition': 'dynamic-schema',
                          'saved_name': 'settings.{declared-setting}',
                          'native_type': 'JSON object with required base_value and optional inputs; unknown '
                                         'members opaque',
                          'legacy_type': 'JSON object interpreted for declared setting name',
                          'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                          'default': 'Absent setting uses its numeric default and default curves.',
                          'absence': 'Present setting without base_value rejected natively; legacy logs and '
                                     'skips invalid setting.',
                          'flags': 'Domain is all 45 declared numeric settings; fixed base_value members are '
                                   'individual resource rows.',
                          'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::base_value'],
                          'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                          'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                          'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.inputs': {'route': '07',
                         'disposition': 'dynamic-schema',
                         'saved_name': 'settings.{declared-setting}.inputs',
                         'native_type': 'JSON object indexed by declared input; unknown members opaque',
                         'legacy_type': 'JSON object indexed by MyPaintBrushInputSettings.name',
                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                         'default': 'Missing mapping: opaque_multiply/pressure [[0,0],[1,1]], all other '
                                    'setting/input pairs empty.',
                         'absence': 'Missing section or known input preserves default; explicit [] clears '
                                    'even pressure default.',
                         'flags': 'Domain is 45 declared settings x 9 inputs; JSON arbitrary counts/order '
                                  'preserved, engine mapping rejects unsupported curves.',
                         'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::curve'],
                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#load_defaults'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.curve.point': {'route': '07',
                         'disposition': 'dynamic-schema',
                         'saved_name': 'settings.{setting}.inputs.{input}[i]',
                         'native_type': 'JSON array exactly [x,y]',
                         'legacy_type': 'JSON number/pair -> float Mapping coordinates',
                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                         'default': 'No synthesized point.',
                         'absence': 'Absent point reduces sequence length; native non-pair rejected.',
                         'flags': 'Parameterized 45 settings x 9 declared inputs; native retains finite '
                                  'coordinates within float representability; legacy casts to float.',
                         'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::curve'],
                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.curve.x': {'route': '07',
                     'disposition': 'dynamic-schema',
                     'saved_name': 'settings.{setting}.inputs.{input}[i][0]',
                     'native_type': 'JSON finite number -> double',
                     'legacy_type': 'JSON number/pair -> float Mapping coordinates',
                     'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                     'default': 'No coordinate default.',
                     'absence': 'Missing coordinate rejected natively.',
                     'flags': 'Parameterized 45 settings x 9 declared inputs; native retains finite '
                              'coordinates within float representability; legacy casts to float.',
                     'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::curve'],
                     'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                     'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                     'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.curve.y': {'route': '07',
                     'disposition': 'dynamic-schema',
                     'saved_name': 'settings.{setting}.inputs.{input}[i][1]',
                     'native_type': 'JSON finite number -> double',
                     'legacy_type': 'JSON number/pair -> float Mapping coordinates',
                     'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                     'default': 'No coordinate default.',
                     'absence': 'Missing coordinate rejected natively.',
                     'flags': 'Parameterized 45 settings x 9 declared inputs; native retains finite '
                              'coordinates within float representability; legacy casts to float.',
                     'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::curve'],
                     'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                     'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                     'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.unknown': {'route': '07',
                          'disposition': 'dynamic-schema',
                          'saved_name': '{unknown JSON member at any depth}',
                          'native_type': 'Opaque JsonNode object/array/scalar/null',
                          'legacy_type': 'Unknown keys logged and dropped by legacy reader',
                          'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                          'default': 'No synthesized unknown member.',
                          'absence': 'Absent stays absent.',
                          'flags': 'Preserved recursively by native clone and encode; this is an open '
                                   'dictionary schema, not an exhaustive list of future keys.',
                          'reader': ['app/paint/painter-mypaint/resource.cpp#clone'],
                          'writer': ['app/paint/painter-mypaint/resource.cpp#encode_node'],
                          'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
                          'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
 'mypaint.json.preview': {'route': '07',
                          'disposition': 'saved-field',
                          'saved_name': 'painter_preview_png',
                          'native_type': 'Base64 string encoding PNG bytes',
                          'legacy_type': 'No legacy JSON member; separate cairo PNG sidecar',
                          'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                          'default': 'Absent or empty accessor = empty string / no embedded icon.',
                          'absence': 'Empty setter removes member; malformed embedded PNG causes brush load '
                                     'failure.',
                          'flags': 'Native extension; loader prefers embedded bytes over sidecar; legacy '
                                   'opaque/unrecognized root member.',
                          'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::preview_png_base64'],
                          'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::set_preview_png_base64'],
                          'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#load_icon'],
                          'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#save_icon']},
 'mypaint.json.legacy': {'route': '07',
                         'disposition': 'dynamic-schema',
                         'saved_name': 'painter_legacy',
                         'native_type': 'JSON object',
                         'legacy_type': 'No legacy JSON member; original line parser GHashTable strings',
                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                         'default': 'Created only for native text import.',
                         'absence': 'Absent for new/ordinary JSON resources.',
                         'flags': 'Native preservation extension; typed migrated values coexist with '
                                  'retained original source.',
                         'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#read_file_v2'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.json.legacy.version': {'route': '07',
                                 'disposition': 'saved-field',
                                 'saved_name': 'painter_legacy.version',
                                 'native_type': 'JSON integer 1 or 2',
                                 'legacy_type': 'No legacy JSON member; original line parser GHashTable '
                                                'strings',
                                 'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                 'default': 'Native absent text version defaults to 1; legacy reader absent '
                                            'version was 0 (<=1 rules).',
                                 'absence': 'Always emitted by native text import.',
                                 'flags': 'Native preservation extension; typed migrated values coexist with '
                                          'retained original source.',
                                 'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                                 'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                 'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#read_file_v2'],
                                 'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.json.legacy.source': {'route': '07',
                                'disposition': 'saved-field',
                                'saved_name': 'painter_legacy.source',
                                'native_type': 'Original complete UTF-8 source string',
                                'legacy_type': 'No legacy JSON member; original line parser GHashTable '
                                               'strings',
                                'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                'default': 'Exact original text, including comments/whitespace.',
                                'absence': 'Always emitted by native text import.',
                                'flags': 'Native preservation extension; typed migrated values coexist with '
                                         'retained original source.',
                                'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                                'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#read_file_v2'],
                                'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.json.legacy.fields': {'route': '07',
                                'disposition': 'dynamic-schema',
                                'saved_name': 'painter_legacy.fields',
                                'native_type': 'JSON object of raw key -> stripped value strings',
                                'legacy_type': 'No legacy JSON member; original line parser GHashTable '
                                               'strings',
                                'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                'default': 'Empty object if input has no non-version fields.',
                                'absence': 'Always emitted by native text import.',
                                'flags': 'Native preservation extension; typed migrated values coexist with '
                                         'retained original source.',
                                'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                                'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#read_file_v2'],
                                'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.json.legacy.raw': {'route': '07',
                             'disposition': 'dynamic-schema',
                             'saved_name': 'painter_legacy.fields.{raw-key}',
                             'native_type': 'JSON UTF-8 string',
                             'legacy_type': 'No legacy JSON member; original line parser GHashTable strings',
                             'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                             'default': 'No synthesized raw keys; duplicate raw keys keep last value.',
                             'absence': 'Absent raw key remains absent.',
                             'flags': 'Native preservation extension; typed migrated values coexist with '
                                      'retained original source.',
                             'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                             'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                             'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#read_file_v2'],
                             'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.version': {'route': '08',
                          'disposition': 'saved-field',
                          'saved_name': 'version',
                          'native_type': 'ASCII integer 1 or 2',
                          'legacy_type': 'ASCII integer -> gint64 via g_ascii_strtoll(base0); >3 stops scan; no1/2-only restriction',
                          'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                          'default': 'Native missing version =1; legacy missing version =0 (<=1 parsing).',
                          'absence': 'Missing uses listed default; native duplicate/unsupported version '
                                     'rejected.',
                          'flags': 'Native import requires version1/2 and writes canonical JSON3; legacy text reader allows <=3 and stops at >3. Dormant legacy writer emits CURRENT_BRUSHFILE_VERSION=3.',
                          'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                          'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                          'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#read_file_v2'],
                          'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.parent': {'route': '08',
                         'disposition': 'saved-field',
                         'saved_name': 'parent_brush_name',
                         'native_type': 'Percent-escaped string -> UTF-8 string',
                         'legacy_type': 'Percent-escaped string -> UTF-8 string',
                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                         'default': 'Empty stored parent name.',
                         'absence': 'Missing leaves parent empty; no native text empty-to-basename '
                                    'substitution.',
                         'flags': 'Native unescapes parent; canonical JSON writer preserves decoded value. '
                                  'Legacy dormant text writer does not emit parent.',
                         'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.group': {'route': '08',
                        'disposition': 'saved-field',
                        'saved_name': 'group',
                        'native_type': 'Raw UTF-8 text',
                        'legacy_type': 'Raw UTF-8 text',
                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                        'default': 'Empty group string.',
                        'absence': 'Missing leaves group empty.',
                        'flags': 'Both native and legacy keep raw group without percent decoding; dormant '
                                 'legacy writer does not emit group.',
                        'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.color': {'route': '08',
                        'disposition': 'saved-field',
                        'saved_name': 'color',
                        'native_type': 'v1 RGB double triple -> divide by255 -> HSV double settings',
                        'legacy_type': 'v1 integer RGB triple -> GimpRGB double -> GimpHSV double -> float '
                                       'settings',
                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                        'default': 'Absent uses color_h/color_s/color_v numeric defaults, all 0.',
                        'absence': 'Missing leaves each declared numeric default.',
                        'flags': 'v1 only; divide components by255 then RGB->HSV; migrated into three '
                                 'numeric settings; original retained.',
                        'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.painting_time': {'route': '08',
                                'disposition': 'saved-field',
                                'saved_name': 'painting_time',
                                'native_type': 'Opaque text line',
                                'legacy_type': 'Opaque text line',
                                'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                'default': 'No operational value/default.',
                                'absence': 'Missing raw key stays absent.',
                                'flags': 'v1 ignored by both readers; native original retained.',
                                'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                                'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.change_radius': {'route': '08',
                                'disposition': 'saved-field',
                                'saved_name': 'change_radius',
                                'native_type': 'Opaque obsolete text line',
                                'legacy_type': 'Opaque obsolete text line',
                                'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                'default': 'No operational value/default.',
                                'absence': 'Missing raw key stays absent.',
                                'flags': 'Legacy v1 ignores exact 0.0 and falls through unimplemented '
                                         'nonzero handling; native retains raw unsupported key.',
                                'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                                'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.adapt_color_from_image': {'route': '08',
                                         'disposition': 'saved-field',
                                         'saved_name': 'adapt_color_from_image',
                                         'native_type': 'Opaque obsolete text line',
                                         'legacy_type': 'Opaque obsolete text line',
                                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                         'default': 'No operational value/default.',
                                         'absence': 'Missing raw key stays absent.',
                                         'flags': 'Legacy <=v2 ignores exact 0.0 and falls through '
                                                  'unimplemented nonzero handling; native retains raw '
                                                  'unsupported key.',
                                         'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.speed': {'route': '08',
                        'disposition': 'saved-field',
                        'saved_name': 'speed',
                        'native_type': 'Opaque obsolete text line',
                        'legacy_type': 'Opaque obsolete text line',
                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                        'default': 'No operational value/default.',
                        'absence': 'Missing raw key stays absent.',
                        'flags': 'Legacy v1 renames setting key speed to speed1, which is not a declared '
                                 'setting; native retains raw unsupported key. This is not a speed input '
                                 'alias.',
                        'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.text.alias.color_hue': {'route': '08',
                                  'disposition': 'saved-field',
                                  'saved_name': 'color_hue',
                                  'native_type': 'Legacy text numeric/curve -> canonical change_color_h',
                                  'legacy_type': 'gfloat migration transform -> change_color_h',
                                  'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                  'default': 'Absent alias leaves change_color_h at float32(0.0).',
                                  'absence': 'No alias synthesized; canonical and alias duplicate-order '
                                             'behavior is not certified here.',
                                  'flags': 'Transforms base and curve y by '
                                           'float32(float32(value)*64.0/360.0); x unchanged; raw original '
                                           'retained.',
                                  'reader': ['app/paint/painter-mypaint/resource.cpp#migrated_name'],
                                  'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                  'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                  'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json'],
                                  'domain': {'kind': 'alias',
                                             'name': 'color_hue',
                                             'target': 'change_color_h',
                                             'transform': 'float32(float32(value)*64.0/360.0)'}},
 'mypaint.text.alias.color_saturation': {'route': '08',
                                         'disposition': 'saved-field',
                                         'saved_name': 'color_saturation',
                                         'native_type': 'Legacy text numeric/curve -> canonical '
                                                        'change_color_hsv_s',
                                         'legacy_type': 'gfloat migration transform -> change_color_hsv_s',
                                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                         'default': 'Absent alias leaves change_color_hsv_s at float32(0.0).',
                                         'absence': 'No alias synthesized; canonical and alias '
                                                    'duplicate-order behavior is not certified here.',
                                         'flags': 'Transforms base and curve y by '
                                                  'float32(float32(value)*128.0/256.0); x unchanged; raw '
                                                  'original retained.',
                                         'reader': ['app/paint/painter-mypaint/resource.cpp#migrated_name'],
                                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json'],
                                         'domain': {'kind': 'alias',
                                                    'name': 'color_saturation',
                                                    'target': 'change_color_hsv_s',
                                                    'transform': 'float32(float32(value)*128.0/256.0)'}},
 'mypaint.text.alias.color_value': {'route': '08',
                                    'disposition': 'saved-field',
                                    'saved_name': 'color_value',
                                    'native_type': 'Legacy text numeric/curve -> canonical change_color_v',
                                    'legacy_type': 'gfloat migration transform -> change_color_v',
                                    'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                    'default': 'Absent alias leaves change_color_v at float32(0.0).',
                                    'absence': 'No alias synthesized; canonical and alias duplicate-order '
                                               'behavior is not certified here.',
                                    'flags': 'Transforms base and curve y by '
                                             'float32(float32(value)*128.0/256.0); x unchanged; raw original '
                                             'retained.',
                                    'reader': ['app/paint/painter-mypaint/resource.cpp#migrated_name'],
                                    'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                    'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                    'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json'],
                                    'domain': {'kind': 'alias',
                                               'name': 'color_value',
                                               'target': 'change_color_v',
                                               'transform': 'float32(float32(value)*128.0/256.0)'}},
 'mypaint.text.alias.speed_slowness': {'route': '08',
                                       'disposition': 'saved-field',
                                       'saved_name': 'speed_slowness',
                                       'native_type': 'Legacy text numeric/curve -> canonical '
                                                      'speed1_slowness',
                                       'legacy_type': 'gfloat migration transform -> speed1_slowness',
                                       'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                       'default': 'Absent alias leaves speed1_slowness at float32(0.04).',
                                       'absence': 'No alias synthesized; canonical and alias duplicate-order '
                                                  'behavior is not certified here.',
                                       'flags': 'Transforms base and curve y by float32(value); x unchanged; '
                                                'raw original retained.',
                                       'reader': ['app/paint/painter-mypaint/resource.cpp#migrated_name'],
                                       'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                       'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                       'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json'],
                                       'domain': {'kind': 'alias',
                                                  'name': 'speed_slowness',
                                                  'target': 'speed1_slowness',
                                                  'transform': 'float32(value)'}},
 'mypaint.text.alias.change_color_s': {'route': '08',
                                       'disposition': 'saved-field',
                                       'saved_name': 'change_color_s',
                                       'native_type': 'Legacy text numeric/curve -> canonical '
                                                      'change_color_hsv_s',
                                       'legacy_type': 'gfloat migration transform -> change_color_hsv_s',
                                       'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                       'default': 'Absent alias leaves change_color_hsv_s at float32(0.0).',
                                       'absence': 'No alias synthesized; canonical and alias duplicate-order '
                                                  'behavior is not certified here.',
                                       'flags': 'Transforms base and curve y by float32(value); x unchanged; '
                                                'raw original retained.',
                                       'reader': ['app/paint/painter-mypaint/resource.cpp#migrated_name'],
                                       'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                       'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                       'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json'],
                                       'domain': {'kind': 'alias',
                                                  'name': 'change_color_s',
                                                  'target': 'change_color_hsv_s',
                                                  'transform': 'float32(value)'}},
 'mypaint.text.alias.stroke_treshold': {'route': '08',
                                        'disposition': 'saved-field',
                                        'saved_name': 'stroke_treshold',
                                        'native_type': 'Legacy text numeric/curve -> canonical '
                                                       'stroke_threshold',
                                        'legacy_type': 'gfloat migration transform -> stroke_threshold',
                                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                                        'default': 'Absent alias leaves stroke_threshold at float32(0.0).',
                                        'absence': 'No alias synthesized; canonical and alias '
                                                   'duplicate-order behavior is not certified here.',
                                        'flags': 'Transforms base and curve y by float32(value); x '
                                                 'unchanged; raw original retained.',
                                        'reader': ['app/paint/painter-mypaint/resource.cpp#migrated_name'],
                                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json'],
                                        'domain': {'kind': 'alias',
                                                   'name': 'stroke_treshold',
                                                   'target': 'stroke_threshold',
                                                   'transform': 'float32(value)'}},
 'mypaint.text.curves': {'route': '08',
                         'disposition': 'dynamic-schema',
                         'saved_name': '{numeric-setting} | {declared-input} {points}',
                         'native_type': 'v1 whitespace x y pairs; v2 (x y), (x y) pairs; native x double and '
                                        'y float32 promoted to double',
                         'legacy_type': 'v1/v2 text -> float Mapping coordinates',
                         'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                         'default': 'opaque_multiply pressure default [[0,0],[1,1]], otherwise empty.',
                         'absence': 'No curve clause leaves defaults; explicit valid empty curve clears in '
                                    'native parser.',
                         'flags': 'Native accepts 0 or2..8 finite ordered points, duplicates x allowed; '
                                  'rejects malformed. Legacy v1 has stray semicolon after x check; preserved '
                                  'as historical behavior, not copied. Dormant legacy writer prints (x,y), '
                                  'different from reader (x y).',
                         'reader': ['app/paint/painter-mypaint/resource.cpp#parse_legacy_curve'],
                         'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_points_v1'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']},
 'mypaint.preview.png': {'route': '09',
                         'disposition': 'saved-field',
                         'saved_name': '{brush-basename}_prev.png',
                         'native_type': 'PNG bytes -> GdkPixbuf, then PNG/base64 inside resource',
                         'legacy_type': 'PNG bytes -> cairo_surface_t',
                         'owner': 'GimpPainterMybrush::BrushImpl.icon/resource / '
                                  'GimpMypaintBrushPrivate::icon_image',
                         'default': 'No preview: NULL icon; native view supplies fallback.',
                         'absence': 'Missing/unreadable sidecar ignored by native load; embedded preview '
                                    'takes precedence.',
                         'flags': 'Native reads sidecar only with local .myb path and no embedded preview; '
                                  'next save embeds PNG, never writes sidecar. Legacy save_brush writes '
                                  'sidecar after JSON.',
                         'reader': ['app/core/gimppaintermybrush.cpp#gimp_painter_mybrush_load'],
                         'writer': ['app/core/gimppaintermybrush.cpp#gimp_painter_mybrush_set_icon',
                                    'app/core/gimppaintermybrush.cpp#save'],
                         'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#load_icon'],
                         'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#save_icon']},
 'mypaint.option.json': {'route': '12',
                         'disposition': 'dynamic-schema',
                         'saved_name': 'painter-settings',
                         'native_type': 'GParamSpecString / G_TYPE_STRING containing complete .myb JSON',
                         'legacy_type': 'No legacy full-resource option property',
                         'owner': 'GimpPainterMybrushOptions / GimpMypaintOptions',
                         'default': 'GParamSpec NULL means no full-resource override; getter encodes current '
                                    'draft.',
                         'absence': 'Absent or NULL setter leaves current full draft unchanged; scalar reset '
                                    'leaves curves/unknown fields/preview intact.',
                         'flags': 'Native G_PARAM_READWRITE|GIMP_CONFIG_PARAM_SERIALIZE; full resource '
                                  'schema is route07 including native preservation extensions.',
                         'reader': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#set_property'],
                         'writer': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#get_property'],
                         'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                         'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']},
 'mypaint.option.mode': {'route': '12',
                         'disposition': 'saved-field',
                         'saved_name': 'brush-mode',
                         'native_type': 'GParamSpecEnum GimpMypaintBrushMode; serialized enum nick',
                         'legacy_type': 'GParamSpecEnum GIMP_TYPE_MYPAINT_BRUSH_MODE; serialized enum nick',
                         'owner': 'GimpPainterMybrushOptions / GimpMypaintOptions',
                         'default': 'Native/legacy default 0 / normal / GIMP_MYPAINT_NORMAL.',
                         'absence': 'Absent config preserves current mode; reset sets0; no effect on '
                                    'resource draft/rendering selection.',
                         'flags': 'Native READWRITE|SERIALIZE; enum values 0 normal,1 normal-and-erase,2 '
                                  'lock-alpha,3 colorize.',
                         'reader': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#set_property'],
                         'writer': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#get_property'],
                         'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                         'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']},
 'mypaint.option.view-type': {'route': '12',
                              'disposition': 'saved-field',
                              'saved_name': 'brush-view-type',
                              'native_type': 'GParamSpecEnum GimpViewType',
                              'legacy_type': 'GParamSpecEnum GimpViewType',
                              'owner': 'GimpPaintOptions (inherited by GimpPainterMybrushOptions) / '
                                       'GimpMypaintOptions',
                              'default': 'Native/legacy GParamSpec default: GIMP_VIEW_TYPE_GRID (1).',
                              'absence': 'Absent config preserves current property; construction/reset '
                                         'applies GParamSpec default.',
                              'flags': 'Inherited native GimpPaintOptions config property; legacy property '
                                       'owned directly by GimpMypaintOptions. GIMP_CONFIG_PARAM_FLAGS plus '
                                       'STATIC_STRINGS.',
                              'reader': ['app/paint/gimppaintoptions.c#gimp_paint_options_set_property'],
                              'writer': ['app/paint/gimppaintoptions.c#gimp_paint_options_get_property'],
                              'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                              'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']},
 'mypaint.option.view-size': {'route': '12',
                              'disposition': 'saved-field',
                              'saved_name': 'brush-view-size',
                              'native_type': 'GParamSpecInt / G_TYPE_INT',
                              'legacy_type': 'GParamSpecInt / G_TYPE_INT',
                              'owner': 'GimpPaintOptions (inherited by GimpPainterMybrushOptions) / '
                                       'GimpMypaintOptions',
                              'default': 'Native/legacy GParamSpec default: GIMP_VIEW_SIZE_SMALL (24); min '
                                         'GIMP_VIEW_SIZE_TINY (12), max GIMP_VIEWABLE_MAX_BUTTON_SIZE = '
                                         'GIMP_VIEW_SIZE_GIGANTIC (192).',
                              'absence': 'Absent config preserves current property; construction/reset '
                                         'applies GParamSpec default.',
                              'flags': 'Inherited native GimpPaintOptions config property; legacy property '
                                       'owned directly by GimpMypaintOptions. GIMP_CONFIG_PARAM_FLAGS plus '
                                       'STATIC_STRINGS.',
                              'reader': ['app/paint/gimppaintoptions.c#gimp_paint_options_set_property'],
                              'writer': ['app/paint/gimppaintoptions.c#gimp_paint_options_get_property'],
                              'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                              'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']}}

SCALAR_RULES = {'settings': {'resource': {'route': '07',
                           'disposition': 'saved-field',
                           'native_type': 'JSON number -> double; finite and abs(value) <= FLT_MAX',
                           'legacy_type': 'JSON number -> gdouble -> float in Mapping::base_value',
                           'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                           'absence': 'Missing setting or settings section resolves the listed default '
                                      'without materializing JSON; present known setting without base_value '
                                      'is rejected natively; legacy missing base_value keeps loaded default.',
                           'flags': 'Native encode preserves only present JSON members; set_base_value '
                                    'writes this member. Legacy build_json emits every declared numeric '
                                    'setting. Metadata range is not a resource clamp.',
                           'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::base_value'],
                           'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::set_base_value',
                                      'app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                           'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3',
                                             'app/core/gimpmypaintbrush-load.cpp#load_defaults'],
                           'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
              'option': {'route': '12',
                         'disposition': 'saved-field',
                         'native_type': 'GParamSpecDouble / G_TYPE_DOUBLE',
                         'legacy_type': 'GParamSpecDouble / G_TYPE_DOUBLE',
                         'owner': 'GimpPainterMybrushOptions / GimpMypaintOptions',
                         'absence': 'Absent config property leaves current selected-resource draft '
                                    'unchanged; reset uses the GParamSpec default. Selected-resource '
                                    'construction may replace the initial unselected draft.',
                         'flags': 'Native G_PARAM_READWRITE|GIMP_CONFIG_PARAM_SERIALIZE; legacy '
                                  'GIMP_CONFIG_INSTALL_PROP_* adds GIMP_CONFIG_PARAM_FLAGS to '
                                  'GIMP_PARAM_STATIC_STRINGS. Native inherited GimpContext serializer emits '
                                  'only values unequal to their GParamSpec defaults; legacy config macros '
                                  'also include G_PARAM_CONSTRUCT.',
                         'reader': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#set_property',
                                    'app/core/gimptooloptions.c#gimp_tool_options_deserialize'],
                         'writer': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#get_property',
                                    'app/core/gimptooloptions.c#gimp_tool_options_serialize'],
                         'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                         'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']},
              'text': {'route': '08',
                       'disposition': 'saved-field',
                       'legacy_type': 'ASCII numeric -> gdouble -> float',
                       'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                       'absence': 'Missing line leaves loaded numeric and curve defaults; last duplicate raw '
                                  'key retained; native applies parsed lines in order.',
                       'flags': 'Import-only native route; Resource::encode writes migrated v3 JSON and raw '
                                'painter_legacy. Dormant legacy write_file emits numeric lines but '
                                'save_brush calls build_json.',
                       'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                       'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                       'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                       'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file',
                                         'app/core/gimpmypaintbrush-save.cpp#save_brush']}},
 'switches': {'resource': {'route': '07',
                           'disposition': 'saved-field',
                           'native_type': 'JSON boolean -> bool',
                           'legacy_type': 'JSON boolean -> gboolean -> bool',
                           'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                           'absence': 'Missing switches section or member resolves FALSE without '
                                      'materializing JSON; present non-boolean is rejected natively.',
                           'flags': 'Native encode retains present members; set_switch creates this boolean. '
                                    'Legacy build_json emits all five switches.',
                           'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::switch_value'],
                           'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::set_switch',
                                      'app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                           'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3',
                                             'app/core/gimpmypaintbrush-load.cpp#load_defaults'],
                           'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
              'option': {'route': '12',
                         'disposition': 'saved-field',
                         'native_type': 'GParamSpecBoolean / G_TYPE_BOOLEAN',
                         'legacy_type': 'GParamSpecBoolean / G_TYPE_BOOLEAN',
                         'owner': 'GimpPainterMybrushOptions / GimpMypaintOptions',
                         'absence': 'Absent config property leaves current selected-resource draft '
                                    'unchanged; reset uses the GParamSpec default. Selected-resource '
                                    'construction may replace the initial unselected draft.',
                         'flags': 'Native G_PARAM_READWRITE|GIMP_CONFIG_PARAM_SERIALIZE; legacy '
                                  'GIMP_CONFIG_INSTALL_PROP_* adds GIMP_CONFIG_PARAM_FLAGS to '
                                  'GIMP_PARAM_STATIC_STRINGS. Native inherited GimpContext serializer emits '
                                  'only values unequal to their GParamSpec defaults; legacy config macros '
                                  'also include G_PARAM_CONSTRUCT.',
                         'reader': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#set_property',
                                    'app/core/gimptooloptions.c#gimp_tool_options_deserialize'],
                         'writer': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#get_property',
                                    'app/core/gimptooloptions.c#gimp_tool_options_serialize'],
                         'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                         'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']},
              'text': {'route': '08',
                       'disposition': 'saved-field',
                       'native_type': 'UTF-8 raw field retained in painter_legacy.fields; no switch '
                                      'interpretation',
                       'legacy_type': 'Dormant writer int 0/1 line; legacy reader recognizes only numeric '
                                      'setting dictionary',
                       'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                       'absence': 'Missing raw field stays absent; imported switch value is not applied.',
                       'flags': 'Dormant legacy text writer emits this switch; neither legacy nor native '
                                'text importer applies switch lines. Native preserves raw text in canonical '
                                'JSON.',
                       'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::decode'],
                       'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                       'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_raw_v2'],
                       'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#write_file']}},
 'texts': {'resource': {'route': '07',
                        'disposition': 'saved-field',
                        'native_type': 'JSON string or null -> nullable const char*',
                        'legacy_type': 'JSON string/null -> owned char*',
                        'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
                        'absence': 'Missing texts section/member and explicit JSON null resolve NULL; '
                                   'explicit empty string remains distinct.',
                        'flags': 'Native encode retains present members; set_text writes string or explicit '
                                 'null. Legacy build_json emits both text members using '
                                 'json_builder_add_string_value.',
                        'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::peek_text'],
                        'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::set_text',
                                   'app/paint/painter-mypaint/resource.cpp#Resource::encode'],
                        'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3',
                                          'app/core/gimpmypaintbrush-load.cpp#load_defaults'],
                        'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']},
           'option': {'route': '12',
                      'disposition': 'saved-field',
                      'native_type': 'GParamSpecString / G_TYPE_STRING',
                      'legacy_type': 'GParamSpecString / G_TYPE_STRING',
                      'owner': 'GimpPainterMybrushOptions / GimpMypaintOptions',
                      'absence': 'Absent config property leaves current selected-resource draft unchanged; '
                                 'reset uses the GParamSpec default. Selected-resource construction may '
                                 'replace the initial unselected draft.',
                      'flags': 'Native G_PARAM_READWRITE|GIMP_CONFIG_PARAM_SERIALIZE; legacy '
                               'GIMP_CONFIG_INSTALL_PROP_* adds GIMP_CONFIG_PARAM_FLAGS to '
                               'GIMP_PARAM_STATIC_STRINGS. Native inherited GimpContext serializer emits '
                               'only values unequal to their GParamSpec defaults; legacy config macros also '
                               'include G_PARAM_CONSTRUCT.',
                      'reader': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#set_property',
                                 'app/core/gimptooloptions.c#gimp_tool_options_deserialize'],
                      'writer': ['app/paint/painter-mypaint-surface/gimp-painter-options.cpp#get_property',
                                 'app/core/gimptooloptions.c#gimp_tool_options_serialize'],
                      'legacy_reader': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_set_property'],
                      'legacy_writer': ['app/paint/gimpmypaintoptions.cpp#gimp_mypaint_options_get_property']}}}

CURVE_RULE = {'route': '07',
 'disposition': 'dynamic-schema',
 'native_type': 'JSON array of ordered [double x,double y] pairs',
 'legacy_type': 'JSON numeric pair array -> Mapping float coordinates',
 'owner': 'GimpPainter::MyPaint::Resource / GimpMypaintBrushPrivate',
 'absence': 'Absent uses listed curve default; explicit [] is empty.',
 'flags': 'All 45 numeric settings; coordinates finite abs<=FLT_MAX; native JSON preserves unsupported point '
          'count/order with diagnostics; engine accepts 0 or 2..8 ordered points.',
 'reader': ['app/paint/painter-mypaint/resource.cpp#Resource::curve'],
 'writer': ['app/paint/painter-mypaint/resource.cpp#Resource::encode'],
 'legacy_reader': ['app/core/gimpmypaintbrush-load.cpp#parse_v3'],
 'legacy_writer': ['app/core/gimpmypaintbrush-save.cpp#build_json']}

CANDIDATE_RULES = {'app/core/gimpmypaintbrush-load.cpp:230': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:230',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.version',
                                                       'mypaint.json.parent',
                                                       'mypaint.json.group',
                                                       'mypaint.json.settings',
                                                       'mypaint.json.switches',
                                                       'mypaint.json.texts',
                                                       'mypaint.json.setting',
                                                       'mypaint.json.inputs',
                                                       'mypaint.json.unknown',
                                                       'mypaint.json.preview',
                                                       'mypaint.json.legacy',
                                                       'mypaint.json.legacy.version',
                                                       'mypaint.json.legacy.source',
                                                       'mypaint.json.legacy.fields',
                                                       'mypaint.json.legacy.raw'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:241': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:241',
                                            'route': '07',
                                            'disposition': 'saved-field',
                                            'fields': ['mypaint.json.version'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:254': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:254',
                                            'route': '07',
                                            'disposition': 'saved-field',
                                            'fields': ['mypaint.json.parent'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:267': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:267',
                                            'route': '07',
                                            'disposition': 'saved-field',
                                            'fields': ['mypaint.json.group'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:275': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:275',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.settings'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:292': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:292',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.resource.opaque',
                                                       'mypaint.resource.opaque_multiply',
                                                       'mypaint.resource.opaque_linearize',
                                                       'mypaint.resource.radius_logarithmic',
                                                       'mypaint.resource.hardness',
                                                       'mypaint.resource.anti_aliasing',
                                                       'mypaint.resource.dabs_per_basic_radius',
                                                       'mypaint.resource.dabs_per_actual_radius',
                                                       'mypaint.resource.dabs_per_second',
                                                       'mypaint.resource.radius_by_random',
                                                       'mypaint.resource.speed1_slowness',
                                                       'mypaint.resource.speed2_slowness',
                                                       'mypaint.resource.speed1_gamma',
                                                       'mypaint.resource.speed2_gamma',
                                                       'mypaint.resource.offset_by_random',
                                                       'mypaint.resource.offset_by_speed',
                                                       'mypaint.resource.offset_by_speed_slowness',
                                                       'mypaint.resource.slow_tracking',
                                                       'mypaint.resource.slow_tracking_per_dab',
                                                       'mypaint.resource.tracking_noise',
                                                       'mypaint.resource.color_h',
                                                       'mypaint.resource.color_s',
                                                       'mypaint.resource.color_v',
                                                       'mypaint.resource.restore_color',
                                                       'mypaint.resource.change_color_h',
                                                       'mypaint.resource.change_color_l',
                                                       'mypaint.resource.change_color_hsl_s',
                                                       'mypaint.resource.change_color_v',
                                                       'mypaint.resource.change_color_hsv_s',
                                                       'mypaint.resource.smudge',
                                                       'mypaint.resource.smudge_length',
                                                       'mypaint.resource.smudge_radius_log',
                                                       'mypaint.resource.eraser',
                                                       'mypaint.resource.stroke_threshold',
                                                       'mypaint.resource.stroke_duration_logarithmic',
                                                       'mypaint.resource.stroke_holdtime',
                                                       'mypaint.resource.custom_input',
                                                       'mypaint.resource.custom_input_slowness',
                                                       'mypaint.resource.elliptical_dab_ratio',
                                                       'mypaint.resource.elliptical_dab_angle',
                                                       'mypaint.resource.direction_filter',
                                                       'mypaint.resource.lock_alpha',
                                                       'mypaint.resource.stroke_opacity',
                                                       'mypaint.resource.texture_grain',
                                                       'mypaint.resource.texture_contrast'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:302': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:302',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.inputs'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:343': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:343',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.resource.non_incremental',
                                                       'mypaint.resource.use_gimp_brushmark',
                                                       'mypaint.resource.use_gimp_texture',
                                                       'mypaint.resource.brushmark_specified',
                                                       'mypaint.resource.texture_specified'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-load.cpp:365': {'legacy_site': 'app/core/gimpmypaintbrush-load.cpp:365',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.resource.brushmark_name',
                                                       'mypaint.resource.texture_name'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:185': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:185',
                                            'route': '07',
                                            'disposition': 'saved-field',
                                            'fields': ['mypaint.json.version'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:188': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:188',
                                            'route': '07',
                                            'disposition': 'saved-field',
                                            'fields': ['mypaint.json.group'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:191': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:191',
                                            'route': '07',
                                            'disposition': 'saved-field',
                                            'fields': ['mypaint.json.parent'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:194': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:194',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.settings'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:199': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:199',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.settings', 'mypaint.json.setting'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:204': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:204',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.resource.opaque',
                                                       'mypaint.resource.opaque_multiply',
                                                       'mypaint.resource.opaque_linearize',
                                                       'mypaint.resource.radius_logarithmic',
                                                       'mypaint.resource.hardness',
                                                       'mypaint.resource.anti_aliasing',
                                                       'mypaint.resource.dabs_per_basic_radius',
                                                       'mypaint.resource.dabs_per_actual_radius',
                                                       'mypaint.resource.dabs_per_second',
                                                       'mypaint.resource.radius_by_random',
                                                       'mypaint.resource.speed1_slowness',
                                                       'mypaint.resource.speed2_slowness',
                                                       'mypaint.resource.speed1_gamma',
                                                       'mypaint.resource.speed2_gamma',
                                                       'mypaint.resource.offset_by_random',
                                                       'mypaint.resource.offset_by_speed',
                                                       'mypaint.resource.offset_by_speed_slowness',
                                                       'mypaint.resource.slow_tracking',
                                                       'mypaint.resource.slow_tracking_per_dab',
                                                       'mypaint.resource.tracking_noise',
                                                       'mypaint.resource.color_h',
                                                       'mypaint.resource.color_s',
                                                       'mypaint.resource.color_v',
                                                       'mypaint.resource.restore_color',
                                                       'mypaint.resource.change_color_h',
                                                       'mypaint.resource.change_color_l',
                                                       'mypaint.resource.change_color_hsl_s',
                                                       'mypaint.resource.change_color_v',
                                                       'mypaint.resource.change_color_hsv_s',
                                                       'mypaint.resource.smudge',
                                                       'mypaint.resource.smudge_length',
                                                       'mypaint.resource.smudge_radius_log',
                                                       'mypaint.resource.eraser',
                                                       'mypaint.resource.stroke_threshold',
                                                       'mypaint.resource.stroke_duration_logarithmic',
                                                       'mypaint.resource.stroke_holdtime',
                                                       'mypaint.resource.custom_input',
                                                       'mypaint.resource.custom_input_slowness',
                                                       'mypaint.resource.elliptical_dab_ratio',
                                                       'mypaint.resource.elliptical_dab_angle',
                                                       'mypaint.resource.direction_filter',
                                                       'mypaint.resource.lock_alpha',
                                                       'mypaint.resource.stroke_opacity',
                                                       'mypaint.resource.texture_grain',
                                                       'mypaint.resource.texture_contrast'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:207': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:207',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.inputs'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:218': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:218',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.curve.pressure',
                                                       'mypaint.curve.speed1',
                                                       'mypaint.curve.speed2',
                                                       'mypaint.curve.random',
                                                       'mypaint.curve.stroke',
                                                       'mypaint.curve.direction',
                                                       'mypaint.curve.tilt_declination',
                                                       'mypaint.curve.tilt_ascension',
                                                       'mypaint.curve.custom',
                                                       'mypaint.curve.point',
                                                       'mypaint.curve.x',
                                                       'mypaint.curve.y'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:238': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:238',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.switches'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:243': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:243',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.resource.non_incremental',
                                                       'mypaint.resource.use_gimp_brushmark',
                                                       'mypaint.resource.use_gimp_texture',
                                                       'mypaint.resource.brushmark_specified',
                                                       'mypaint.resource.texture_specified'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:249': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:249',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.json.texts'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/core/gimpmypaintbrush-save.cpp:255': {'legacy_site': 'app/core/gimpmypaintbrush-save.cpp:255',
                                            'route': '07',
                                            'disposition': 'dynamic-schema',
                                            'fields': ['mypaint.resource.brushmark_name',
                                                       'mypaint.resource.texture_name'],
                                            'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                      'registration expanded into the named fixed members '
                                                      'and parameterized domains. No candidate is a new '
                                                      'engine state field.'},
 'app/paint/gimpmypaintoptions.cpp:115': {'legacy_site': 'app/paint/gimpmypaintoptions.cpp:115',
                                          'route': '12',
                                          'disposition': 'dynamic-schema',
                                          'fields': ['mypaint.option.opaque',
                                                     'mypaint.option.opaque_multiply',
                                                     'mypaint.option.opaque_linearize',
                                                     'mypaint.option.radius_logarithmic',
                                                     'mypaint.option.hardness',
                                                     'mypaint.option.anti_aliasing',
                                                     'mypaint.option.dabs_per_basic_radius',
                                                     'mypaint.option.dabs_per_actual_radius',
                                                     'mypaint.option.dabs_per_second',
                                                     'mypaint.option.radius_by_random',
                                                     'mypaint.option.speed1_slowness',
                                                     'mypaint.option.speed2_slowness',
                                                     'mypaint.option.speed1_gamma',
                                                     'mypaint.option.speed2_gamma',
                                                     'mypaint.option.offset_by_random',
                                                     'mypaint.option.offset_by_speed',
                                                     'mypaint.option.offset_by_speed_slowness',
                                                     'mypaint.option.slow_tracking',
                                                     'mypaint.option.slow_tracking_per_dab',
                                                     'mypaint.option.tracking_noise',
                                                     'mypaint.option.color_h',
                                                     'mypaint.option.color_s',
                                                     'mypaint.option.color_v',
                                                     'mypaint.option.restore_color',
                                                     'mypaint.option.change_color_h',
                                                     'mypaint.option.change_color_l',
                                                     'mypaint.option.change_color_hsl_s',
                                                     'mypaint.option.change_color_v',
                                                     'mypaint.option.change_color_hsv_s',
                                                     'mypaint.option.smudge',
                                                     'mypaint.option.smudge_length',
                                                     'mypaint.option.smudge_radius_log',
                                                     'mypaint.option.eraser',
                                                     'mypaint.option.stroke_threshold',
                                                     'mypaint.option.stroke_duration_logarithmic',
                                                     'mypaint.option.stroke_holdtime',
                                                     'mypaint.option.custom_input',
                                                     'mypaint.option.custom_input_slowness',
                                                     'mypaint.option.elliptical_dab_ratio',
                                                     'mypaint.option.elliptical_dab_angle',
                                                     'mypaint.option.direction_filter',
                                                     'mypaint.option.lock_alpha',
                                                     'mypaint.option.stroke_opacity',
                                                     'mypaint.option.texture_grain',
                                                     'mypaint.option.texture_contrast'],
                                          'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                    'registration expanded into the named fixed members and '
                                                    'parameterized domains. No candidate is a new engine '
                                                    'state field.'},
 'app/paint/gimpmypaintoptions.cpp:126': {'legacy_site': 'app/paint/gimpmypaintoptions.cpp:126',
                                          'route': '12',
                                          'disposition': 'dynamic-schema',
                                          'fields': ['mypaint.option.non_incremental',
                                                     'mypaint.option.use_gimp_brushmark',
                                                     'mypaint.option.use_gimp_texture',
                                                     'mypaint.option.brushmark_specified',
                                                     'mypaint.option.texture_specified'],
                                          'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                    'registration expanded into the named fixed members and '
                                                    'parameterized domains. No candidate is a new engine '
                                                    'state field.'},
 'app/paint/gimpmypaintoptions.cpp:136': {'legacy_site': 'app/paint/gimpmypaintoptions.cpp:136',
                                          'route': '12',
                                          'disposition': 'dynamic-schema',
                                          'fields': ['mypaint.option.brushmark_name',
                                                     'mypaint.option.texture_name'],
                                          'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                    'registration expanded into the named fixed members and '
                                                    'parameterized domains. No candidate is a new engine '
                                                    'state field.'},
 'app/paint/gimpmypaintoptions.cpp:142': {'legacy_site': 'app/paint/gimpmypaintoptions.cpp:142',
                                          'route': '12',
                                          'disposition': 'saved-field',
                                          'fields': ['mypaint.option.mode'],
                                          'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                    'registration expanded into the named fixed members and '
                                                    'parameterized domains. No candidate is a new engine '
                                                    'state field.'},
 'app/paint/gimpmypaintoptions.cpp:148': {'legacy_site': 'app/paint/gimpmypaintoptions.cpp:148',
                                          'route': '12',
                                          'disposition': 'saved-field',
                                          'fields': ['mypaint.option.view-type'],
                                          'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                    'registration expanded into the named fixed members and '
                                                    'parameterized domains. No candidate is a new engine '
                                                    'state field.'},
 'app/paint/gimpmypaintoptions.cpp:154': {'legacy_site': 'app/paint/gimpmypaintoptions.cpp:154',
                                          'route': '12',
                                          'disposition': 'saved-field',
                                          'fields': ['mypaint.option.view-size'],
                                          'reason': 'Exact parse_v3/build_json dispatch or GParamSpec '
                                                    'registration expanded into the named fixed members and '
                                                    'parameterized domains. No candidate is a new engine '
                                                    'state field.'}}

SOURCE_FILES = {('legacy', 'app/core/gimpmypaintbrush-load.cpp'),
 ('legacy', 'app/core/gimpmypaintbrush-private.hpp'),
 ('legacy', 'app/core/gimpmypaintbrush-save.cpp'),
 ('legacy', 'app/core/gimpmypaintbrush.cpp'),
 ('legacy', 'app/core/mypaintbrush-brushsettings.c'),
 ('legacy', 'app/paint/gimpmypaintoptions.cpp'),
 ('legacy', 'libgimpconfig/gimpconfig-params.h'),
 ('native', 'app/core/core-enums.h'),
 ('native', 'app/core/gimpcontext.c'),
 ('native', 'app/core/gimppaintermybrush.cpp'),
 ('native', 'app/paint/gimppaintoptions.c'),
 ('native', 'app/paint/painter-brush-settings/brush-settings.json'),
 ('native', 'app/paint/painter-brush-settings/generate.py'),
 ('native', 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'),
 ('native', 'app/paint/painter-mypaint/mapping.hpp'),
 ('native', 'app/paint/painter-mypaint/resource.cpp'),
 ('native', 'app/paint/painter-mypaint/resource.hpp'),
 ('native', 'libgimpconfig/gimpconfig-params.h'),
 ('native', 'libgimpconfig/gimpconfig-serialize.c')}

STORAGE = {'07': '.myb JSON',
 '12': 'tool-options GimpConfig',
 '08': '.myb legacy line text imported to canonical JSON',
 '09': 'paired PNG preview imported to embedded PNG'}

VERSIONS = {'07': 'JSON v3 and opaque future versions >=3',
 '12': 'legacy and native GimpConfig text',
 '08': 'legacy v1/v2; native canonical JSON v3 output',
 '09': 'PNG sidecar _prev.png; native painter_preview_png JSON extension'}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ledger',type=Path,default=Path('migration/inventory/persistence-mypaint-fields.json'))
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    args=parser.parse_args()
    path=args.ledger if args.ledger.is_absolute() else args.root/args.ledger
    ids=validate_family(json.loads(path.read_text()),args.root)
    print(f'PASS: {len(ids)} MyPaint saved-field metadata assertions; all 27 legacy candidate mappings exact; no native runtime executed.')
if __name__=='__main__': main()
