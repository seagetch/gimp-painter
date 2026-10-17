#!/usr/bin/env python3
"""Source-grounded XCF field/type/default contract assertions (task 10.001).

No old executable is used.  Coverage is discovered from the native and pinned
legacy decoder branches, structured read operations and GimpConfig members.
The reviewed semantic rules below are checked against those source structures,
not against a promised row count or the historical acceptance matrix.
"""
import argparse
import csv
import json
from pathlib import Path
import re
from persistence_source import source, norm, function, calls, property_blocks, config_members, exact, LEGACY, function_ranges

ROOT = Path(__file__).resolve().parents[2]
LOAD = 'app/xcf/xcf-load.c'
SAVE = 'app/xcf/xcf-save.c'
PRESERVE = 'app/xcf/painter-xcf-preserve.cpp'
ARGS = 'app/xcf/painter-xcf-arguments.cpp'
MULTI = 'app/xcf/painter-xcf-multipart.cpp'
TEST = 'migration/tests/test_persistence_xcf_contract.py'
OWNERS = {'image':'GimpImage', 'layer':'GimpLayer (including derived Clone/Filter/text/group)',
          'channel':'GimpChannel/GimpLayerMask', 'path':'GimpPath', 'effect':'FilterData/GimpDrawableFilter'}

def proof(root, path, anchor, expected, revision='native'):
    text = source(str(root), path, revision)
    if anchor and anchor not in text: raise AssertionError(f'missing anchor {path}#{anchor}')
    try: scoped=function(text,anchor); scope='function'
    except AssertionError: scoped=text; scope='declaration/expression (parsed separately)'
    if norm(expected) not in norm(scoped): raise AssertionError(f'source rule changed in {scope}: {path}#{anchor}: {expected}')
    return dict(path=path, revision=revision, anchor=anchor, expected=expected, scope=scope)

def row(key, route, owner, name, typ, legacy_type, default, absence,
        reader, writer, evidence, *, version='native XCF; legacy Painter v0..4 where present',
        flags='XCF record writer dispatch; no GParamSpec flag gate', disposition='saved-field',
        legacy_reader=None, legacy_writer=None, **extra):
    return dict(id='xcf.'+key, route=route, disposition=disposition, storage='XCF', version=version,
                saved_name=name, native_type=typ, legacy_type=legacy_type, owner=owner,
                default=default, absence=absence, flags=flags, reader=reader, writer=writer,
                legacy_reader=legacy_reader or [], legacy_writer=legacy_writer or [],
                evidence=evidence, assertion='/persistence/xcf/'+key, test_file=TEST, **extra)

# Default semantics are tied to the constructor or load carrier, not the
# spelling of a property.  The corresponding exact source rules are checked.
def absence_rule(root, owner, prop):
    p = prop.removeprefix('PROP_')
    ev = []
    def e(path, anchor, code): ev.append(proof(root,path,anchor,code))
    if p == 'END': return 'Required terminator; missing/truncated property list is a read failure', ev
    if owner == 'effect':
        e(LOAD,'xcf_load_effect','filter = g_new0 (FilterData, 1);')
        mapping = {'VISIBLE':'FALSE', 'FLOAT_OPACITY':'0.0', 'MODE':'0 (GIMP_LAYER_MODE_NORMAL_LEGACY)',
                   'BLEND_SPACE':'0 (GIMP_LAYER_COLOR_SPACE_AUTO)', 'COMPOSITE_SPACE':'0 (GIMP_LAYER_COLOR_SPACE_AUTO)',
                   'COMPOSITE_MODE':'0 (GIMP_LAYER_COMPOSITE_AUTO)', 'FILTER_REGION':'0 (selection)', 'FILTER_CLIP':'FALSE',
                   'FILTER_ARGUMENT':'no assignment to the new GEGL node; installed operation constructor/property default remains'}
        return mapping[p] + '; zero-filled FilterData or untouched new operation', ev
    if p == 'VISIBLE':
        e('app/core/gimpitem.c','gimp_item_init','private->visible = TRUE;'); return 'TRUE',ev
    if p in ('LOCK_CONTENT','LOCK_POSITION','LOCK_VISIBILITY','COLOR_TAG','OFFSETS'):
        item=source(str(root),'app/core/gimpitem.c')
        members=['offset_x','offset_y'] if p=='OFFSETS' else [p.lower()]
        init=function(item,'gimp_item_init'); constructed=function(item,'gimp_item_constructed')
        declarations=function(item,'gimp_item_class_init')
        for member in members:
            # Instance private storage is zero-filled by GObject.  Verify the
            # member exists, has a matching zero-valued read-only ParamSpec,
            # and neither constructor hook changes it.
            assert re.search(r'\b'+member+r'\b',item)
            assert not re.search(r'private\s*->\s*'+member+r'\s*=',init+constructed)
            name=member.replace('_','-')
            matches=[a for _,a,_,_ in calls(declarations,r'g_param_spec_(?:int|boolean|enum)') if a[0]=='"'+name+'"']
            exact(len(matches),1,'item zero default '+member)
            exact(matches[0][-2], 'GIMP_COLOR_TAG_NONE' if p=='COLOR_TAG' else 'FALSE' if p.startswith('LOCK') else '0',member+' ParamSpec default')
            exact(matches[0][-1],'GIMP_PARAM_READABLE',member+' ParamSpec flags')
            ev.append(dict(path='app/core/gimpitem.c',revision='native',anchor='gimp_item_init/gimp_item_constructed',
                           expected=member+' has zero-filled instance storage and no assignment in either constructor hook',
                           scope='checked absence of assignment plus parsed zero-valued read-only ParamSpec'))
        return {'OFFSETS':'x=0,y=0', 'COLOR_TAG':'0 (GIMP_COLOR_TAG_NONE)'}.get(p,'FALSE') + '; zero-initialized GimpItemPrivate, not changed by load constructor',ev
    if p == 'LOCK_ALPHA':
        e('app/core/gimplayer.c','gimp_layer_init','layer->lock_alpha = FALSE;'); return 'FALSE',ev
    if p in ('APPLY_MASK','EDIT_MASK','SHOW_MASK','GROUP_ITEM_FLAGS','TEXT_LAYER_FLAGS'):
        var, val = {'APPLY_MASK':('apply_mask','TRUE'),'EDIT_MASK':('edit_mask','FALSE'),
                    'SHOW_MASK':('show_mask','FALSE'),'GROUP_ITEM_FLAGS':('group_layer_flags','0'),
                    'TEXT_LAYER_FLAGS':('text_layer_flags','0')}[p]
        body=function(source(str(root),LOAD),'xcf_load_layer')
        match=re.search(r'\b'+var+r'\s*=\s*'+val+r'\s*;',body); assert match
        ev.append(proof(root,LOAD,'xcf_load_layer',match[0])); return val+' in the layer load carrier',ev
    if p in ('OPACITY','FLOAT_OPACITY'):
        if owner=='layer':
            e('app/core/gimplayer.c','gimp_layer_init','layer->opacity = GIMP_OPACITY_OPAQUE;'); return '1.0 (GIMP_OPACITY_OPAQUE)',ev
        e('app/core/gimpchannel.c','gimp_channel_init','gegl_color_new ("black")')
        e('app/core/gimpselection.c','gimp_selection_new','gimp_color_set_alpha (black, 0.5);')
        e(LOAD,'xcf_load_channel_props','*channel = mask;')
        return 'ordinary channel/layer-mask black GeglColor alpha=1.0; PROP_SELECTION replacement constructs black alpha0.5, retained unless a later supplied opacity changes it',ev
    if p in ('COLOR','FLOAT_COLOR'):
        e('app/core/gimpchannel.c','gimp_channel_init','gegl_color_new ("black")')
        e('app/core/gimpselection.c','gimp_selection_new','gimp_color_set_alpha (black, 0.5);')
        return 'black RGB=0,0,0; ordinary channel/mask alpha1.0, selection replacement alpha0.5 unless later supplied opacity changes it',ev
    if p=='SHOW_MASKED':
        e('app/core/gimpchannel.c','gimp_channel_new','channel->show_masked = TRUE;'); return 'TRUE from gimp_channel_new/gimp_layer_mask_new (base init is FALSE)',ev
    if p=='MODE':
        e('app/core/gimplayer.c','gimp_layer_init','layer->mode = GIMP_LAYER_MODE_NORMAL;')
        e(LOAD,'xcf_load_layer','info->painter_historical_modes ? GIMP_LAYER_MODE_PAINTER_NORMAL : GIMP_LAYER_MODE_NORMAL')
        return 'GIMP_LAYER_MODE_PAINTER_NORMAL when painter_historical_modes, otherwise GIMP_LAYER_MODE_NORMAL; supplied by xcf_load_layer constructor argument',ev
    if p in ('BLEND_SPACE','COMPOSITE_SPACE','COMPOSITE_MODE'):
        var=p.lower(); value='GIMP_LAYER_COMPOSITE_AUTO' if p=='COMPOSITE_MODE' else 'GIMP_LAYER_COLOR_SPACE_AUTO'
        e('app/core/gimplayer.c','gimp_layer_init',f'layer->{var} = {value};'); return value,ev
    if p=='RESOLUTION' or p=='UNIT':
        expr='private->xresolution = gimp_template_get_resolution_x (template);' if p=='RESOLUTION' else 'private->resolution_unit = gimp_template_get_resolution_unit (template);'
        e('app/core/gimpimage.c','gimp_image_constructed',expr)
        return ('x/y resolution from config->default_image template' if p=='RESOLUTION' else 'resolution unit from config->default_image template'),ev
    if p=='TATTOO':
        if owner=='image':
            e('app/core/gimpimage.c','gimp_image_init','private->tattoo_state = 0;')
            e(LOAD,'xcf_load_image','if (info->tattoo_state > 0)')
            e(LOAD,'xcf_load_image','gimp_image_set_tattoo_state (image, info->tattoo_state);')
            return 'missing saved-state override leaves constructor/item-allocation tattoo state; initial0 is advanced by selection and loaded items',ev
        e('app/core/gimpitem.c','gimp_item_set_image','private->tattoo = gimp_image_get_new_tattoo (image);'); return 'unique image tattoo allocated by gimp_item_set_image; no saved identity supplied',ev
    mapping={
      'COLORMAP':'no stored palette; native indexed image initialization supplies its palette state',
      'COMPRESSION':'COMPRESS_NONE in zero-initialized XcfInfo',
      'GUIDES':'empty guide list', 'SAMPLE_POINTS':'empty sample-point list','OLD_SAMPLE_POINTS':'empty sample-point list',
      'PARASITES':'no persisted entries added; constructor-owned native state remains',
      'PATHS':'no imported compatibility paths', 'VECTORS':'no imported compatibility paths',
      'USER_UNIT':'keep template resolution unit; user-unit record is optional',
      'ITEM_SET':'empty named item-set lists', 'ITEM_SET_ITEM':'item is not a member of a persisted named set',
      'ACTIVE_LAYER':'no marker override; selection produced by sequential gimp_image_add_layer calls remains',
      'ACTIVE_CHANNEL':'no marker override; selection produced by sequential gimp_image_add_channel calls remains',
      'SELECTED_PATH':'no marker override; selection produced by sequential gimp_image_add_path calls remains',
      'SELECTION':'initial empty image selection; no stored selection channel',
      'FLOATING_SELECTION':'no floating selection', 'LINKED':'FALSE; no legacy-linked named-set membership',
      'GROUP_ITEM':'ordinary layer constructor remains', 'ITEM_PATH':'root of layer tree (NULL path)',
    }
    if p not in mapping: raise AssertionError('unmapped default '+owner+'.'+p)
    if p in ('COMPRESSION','ACTIVE_CHANNEL','SELECTED_PATH','FLOATING_SELECTION','LINKED','ITEM_SET_ITEM'):
        e('app/xcf/xcf.c','xcf_load_stream_with_dialect','XcfInfo info = { 0, };')
        member={'COMPRESSION':'compression','ACTIVE_CHANNEL':'selected_channels','SELECTED_PATH':'selected_vectors',
                'FLOATING_SELECTION':'floating_sel','LINKED':'linked_'+('layers' if owner=='layer' else 'channels'),
                'ITEM_SET_ITEM':('layer_sets' if owner=='layer' else 'channel_sets' if owner=='channel' else 'path_sets')}[p]
        assert member in source(str(root),'app/xcf/xcf-private.h')
        ev.append(proof(root,LOAD,'xcf_load_'+owner+'_props','info->'+member))
        if p in ('ACTIVE_CHANNEL','SELECTED_PATH'):
            kind='channel' if p=='ACTIVE_CHANNEL' else 'path'
            e('app/core/gimpimage.c','gimp_image_add_'+kind,'gimp_image_set_selected_'+('channels' if kind=='channel' else 'paths'))
    elif p in ('GUIDES','SAMPLE_POINTS','OLD_SAMPLE_POINTS','COLORMAP'):
        member={'GUIDES':'guides','SAMPLE_POINTS':'sample_points','OLD_SAMPLE_POINTS':'sample_points','COLORMAP':'palette'}[p]
        e('app/core/gimpimage.c','gimp_image_init','private->'+member+' = NULL;')
    elif p=='PARASITES':
        path='app/core/gimpimage.c' if owner=='image' else 'app/core/gimpitem.c'
        e(path,'gimp_image_init' if owner=='image' else 'gimp_item_init','private->parasites = gimp_parasite_list_new ();')
    elif p in ('PATHS','VECTORS'):
        e('app/core/gimpimage.c','gimp_image_init','private->paths = gimp_item_tree_new (image, GIMP_TYPE_ITEM_STACK, GIMP_TYPE_PATH);')
    elif p=='ITEM_SET':
        for member in ('stored_layer_sets','stored_channel_sets','stored_path_sets'):
            e('app/core/gimpimage.c','gimp_image_init','private->'+member+' = NULL;')
    elif p=='USER_UNIT':
        e('app/core/gimpimage.c','gimp_image_constructed','private->resolution_unit = gimp_template_get_resolution_unit (template);')
    elif p=='GROUP_ITEM':
        e(LOAD,'xcf_load_layer','gimp_layer_new (image, width, height,')
    elif p=='ITEM_PATH':
        e(LOAD,'xcf_load_image','GList *item_path = NULL;')
    elif p=='ACTIVE_LAYER':
        e(LOAD,'xcf_load_image','gimp_image_set_selected_layers')
        e('app/core/gimpimage.c','gimp_image_add_layer','selected_layers = g_list_prepend (NULL, layer);')
        e('app/core/gimpimage.c','gimp_image_add_layer','gimp_image_set_selected_layers (image, selected_layers);')
        e('app/xcf/xcf.c','xcf_load_stream_with_dialect','XcfInfo info = { 0, };')
    elif p=='SELECTION':
        e('app/core/gimpimage.c','gimp_image_constructed','gimp_selection_new')
    return mapping[p],ev

def legacy_absence_rule(root,owner,prop):
    p=prop.removeprefix('PROP_'); ev=[]
    def e(path,anchor,expr): ev.append(proof(root,path,anchor,expr,'legacy'))
    if p=='END': return 'required property-list terminator; no absence default',ev
    if p in ('VISIBLE','LOCK_CONTENT','LINKED','OFFSETS'):
        assignments={'VISIBLE':[('visible','TRUE')],'LOCK_CONTENT':[('lock_content','FALSE')],'LINKED':[('linked','FALSE')],'OFFSETS':[('offset_x','0'),('offset_y','0')]}[p]
        for member,value in assignments:e('app/core/gimpitem.c','gimp_item_init','private->'+member+' = '+value+';')
        return ','.join(member+'='+value for member,value in assignments)+' in GimpItemPrivate',ev
    if p in ('OPACITY','COLOR'):
        if owner=='layer':
            e('app/core/gimplayer.c','gimp_layer_init','layer->opacity = GIMP_OPACITY_OPAQUE;')
            e('app/core/gimplayer.c','gimp_layer_new','opacity = CLAMP (opacity, GIMP_OPACITY_TRANSPARENT, GIMP_OPACITY_OPAQUE);')
            return '1.0 GIMP_OPACITY_OPAQUE; xcf_load_layer passes255 and gimp_layer_new clamps it to1',ev
        e('app/core/gimpchannel.c','gimp_channel_init','gimp_rgba_set (&channel->color, 0.0, 0.0, 0.0, GIMP_OPACITY_OPAQUE);')
        e('app/core/gimpselection.c','gimp_selection_new','GimpRGB black = { 0.0, 0.0, 0.0, 0.5 };')
        e(LOAD,'xcf_load_channel_props','*channel = mask;')
        return 'GimpRGB black RGB0/0/0 alpha1 for ordinary channel/mask; PROP_SELECTION replacement alpha0.5 unless a later supplied opacity changes it',ev
    if p in ('MODE','LOCK_ALPHA'):
        member,value=('mode','GIMP_NORMAL_MODE') if p=='MODE' else ('lock_alpha','FALSE')
        e('app/core/gimplayer.c','gimp_layer_init','layer->'+member+' = '+value+';');return value,ev
    if p in ('APPLY_MASK','EDIT_MASK','SHOW_MASK','GROUP_ITEM_FLAGS','TEXT_LAYER_FLAGS'):
        member,value={'APPLY_MASK':('apply_mask','TRUE'),'EDIT_MASK':('edit_mask','FALSE'),'SHOW_MASK':('show_mask','FALSE'),'GROUP_ITEM_FLAGS':('group_layer_flags','0'),'TEXT_LAYER_FLAGS':('text_layer_flags','0')}[p]
        e(LOAD,'xcf_load_layer',member+' = '+value+';');return value+' from loader carrier (overrides mask object construction)',ev
    if p=='SHOW_MASKED':
        e('app/core/gimpchannel.c','gimp_channel_new','channel->show_masked = TRUE;');return 'TRUE from gimp_channel_new',ev
    if p in ('RESOLUTION','UNIT','USER_UNIT'):
        for member,method in ([('xresolution','resolution_x'),('yresolution','resolution_y')] if p=='RESOLUTION' else [('resolution_unit','resolution_unit')]):
            e('app/core/gimpimage.c','gimp_image_constructed','private->'+member+' = gimp_template_get_'+method+' (template);')
        return 'config->default_image resolution_x/y' if p=='RESOLUTION' else 'config->default_image resolution_unit; missing custom unit does not override it',ev
    if p=='TATTOO':
        if owner=='image':
            e('app/xcf/xcf.c','xcf_load_invoker','info.tattoo_state = 0;')
            e(LOAD,'xcf_load_image','if (info->tattoo_state > 0)')
            e(LOAD,'xcf_load_image','gimp_image_set_tattoo_state (image, info->tattoo_state);')
            return 'no saved-state override; constructor/selection/loaded-item allocations determine resulting image tattoo state',ev
        e('app/core/gimpitem.c','gimp_item_set_image','private->tattoo = gimp_image_get_new_tattoo (image);')
        return 'new image tattoo when item tattoo0 or image changed; saved tattoo absent does not force identity0',ev
    if p=='PARASITES':
        path='app/core/gimpimage.c' if owner=='image' else 'app/core/gimpitem.c'
        e(path,'gimp_image_init' if owner=='image' else 'gimp_item_init','private->parasites = gimp_parasite_list_new ();')
        e('app/core/gimpparasitelist.c','gimp_parasite_list_init','list->table = NULL;')
        return 'empty named parasite list; no persisted entries imported',ev
    if p=='COLORMAP':
        e('app/core/gimpimage.c','gimp_image_constructed','gimp_image_colormap_init (image);')
        e('app/core/gimpimage-colormap.c','gimp_image_colormap_init','private->n_colors = 0;')
        e('app/core/gimpimage-colormap.c','gimp_image_colormap_init','private->colormap = g_new0 (guchar, GIMP_IMAGE_COLORMAP_SIZE);')
        return 'indexed image: n_colors0 and zero-filled legacy byte colormap; no imported palette entries',ev
    if p in ('GUIDES','SAMPLE_POINTS'):
        e('app/core/gimpimage.c','gimp_image_init','private->'+('guides' if p=='GUIDES' else 'sample_points')+' = NULL;')
        return 'empty list',ev
    if p in ('PATHS','VECTORS'):
        e('app/core/gimpimage.c','gimp_image_init','private->vectors = gimp_item_tree_new (image, GIMP_TYPE_ITEM_STACK, GIMP_TYPE_VECTORS);')
        return 'empty GimpVectors tree, no imported paths or active-index override',ev
    if p in ('ACTIVE_LAYER','ACTIVE_CHANNEL'):
        kind='layer' if p=='ACTIVE_LAYER' else 'channel'
        e('app/xcf/xcf.c','xcf_load_invoker','info.active_'+kind+' = NULL;')
        e('app/core/gimpimage.c','gimp_image_add_'+kind,'gimp_image_set_active_'+kind+' (image, '+kind+');')
        return 'no explicit active-'+kind+' override; sequential item insertion activation remains, using old single-active-item ownership',ev
    if p=='COMPRESSION':
        e('app/xcf/xcf.c','xcf_load_invoker','info.compression = COMPRESS_NONE;');return 'COMPRESS_NONE',ev
    if p=='FLOATING_SELECTION':
        e('app/xcf/xcf.c','xcf_load_invoker','info.floating_sel = NULL;');return 'no floating selection',ev
    if p=='SELECTION':
        e('app/core/gimpimage.c','gimp_image_constructed','gimp_selection_new');return 'constructor-created empty image selection; no loaded channel replacement',ev
    if p=='GROUP_ITEM':
        e(LOAD,'xcf_load_layer','gimp_layer_new');return 'ordinary GimpLayer, not group',ev
    if p=='ITEM_PATH':
        e(LOAD,'xcf_load_image','GList *item_path = NULL;');return 'root layer-tree path NULL',ev
    raise AssertionError('unmapped legacy default '+owner+'.'+prop)

READ_TYPES={'int8':'u8','int16':'u16 big-endian','int32':'u32 big-endian',
            'int64':'u64 big-endian','float':'IEEE754 f32 big-endian',
            'double':'IEEE754 f64 big-endian','offset':'u32 offset before XCF11; u64 offset at XCF11+',
            'string':'u32 byte length including NUL + bytes; zero length is NULL',
            'component':'image-format component bytes'}

def transfers(body):
    out=[]
    for name,args,expr,_ in calls(body,r'xcf_read_(?:int8|int16|int32|int64|float|double|offset|string|component)'):
        if len(args)<3: raise AssertionError(expr)
        typ=READ_TYPES[name.removeprefix('xcf_read_')]
        out.append(dict(type=typ,destination=args[1],count=args[2],expression=expr))
    return out

def property_fields(root):
    result=[]; native=source(str(root),LOAD); old=source(str(root),LOAD,'legacy')
    native_tags={k:int(v) for k,v in re.findall(r'(PROP_[A-Z0-9_]+)\s*=\s*(\d+)',source(str(root),'app/xcf/xcf-private.h'))}
    old_tags={k:int(v) for k,v in re.findall(r'(PROP_[A-Z0-9_]+)\s*=\s*(\d+)',source(str(root),'app/xcf/xcf-private.h','legacy'))}
    for owner in OWNERS:
        fn='xcf_load_'+owner+'_props'; blocks=property_blocks(native,fn)
        oldfn=fn
        try: legacy=property_blocks(old,oldfn)
        except AssertionError: legacy={}
        for prop,body in blocks.items():
            route='04' if prop=='PROP_MODE' else '05' if prop=='PROP_PARASITES' else '01'
            oldprop='PROP_SAMPLE_POINTS' if prop=='PROP_OLD_SAMPLE_POINTS' else prop
            if prop=='PROP_SAMPLE_POINTS':oldprop=None
            old_present=oldprop in legacy
            typ=transfers(body); oldtyp=transfers(legacy[oldprop]) if old_present else []
            default,evidence=absence_rule(root,owner,prop)
            evidence.append(proof(root,LOAD,fn,prop))
            savefn='xcf_save_'+owner+'_props'; savebody=function(source(str(root),SAVE),savefn)
            write_calls=[expr for _,a,expr,_ in calls(savebody,'xcf_save_prop') if len(a)>2 and a[2]==prop]
            canonical=[]
            if prop=='PROP_PARASITES':
                savefn='xcf_save_painter_parasites'
                write_calls=[expr for _,a,expr,_ in calls(function(source(str(root),SAVE),savefn),'xcf_save_prop') if a[2]==prop]
            elif prop=='PROP_LINKED':
                # Retired raw flag becomes an explicit named item set.
                canonical=['PROP_ITEM_SET','PROP_ITEM_SET_ITEM']
                savefn='xcf_save_'+owner+'_props'
                write_calls=[expr for _,a,expr,_ in calls(savebody,'xcf_save_prop') if len(a)>2 and a[2]=='PROP_ITEM_SET_ITEM']
                evidence.append(proof(root,LOAD,'xcf_load_image','gimp_item_list_named_new'))
            assert write_calls,(owner,prop,'no native writer or canonical replacement')
            for expr in write_calls: evidence.append(proof(root,SAVE,savefn,expr))
            old_write=[]
            if legacy:
                old_write=[expr for _,a,expr,_ in calls(function(source(str(root),SAVE,'legacy'),'xcf_save_'+owner+'_props'),'xcf_save_prop') if len(a)>2 and a[2]==oldprop]
            legacy_default,legacy_evidence=legacy_absence_rule(root,owner,oldprop) if old_present else ('not a property in pinned legacy owner schema',[])
            evidence+=legacy_evidence
            evidence.append(proof(root,'app/xcf/xcf-private.h',prop,prop+' = '+str(native_tags[prop])))
            # The actual ordered primitive reads and delegated readers are part
            # of the assertion, not merely evidence strings.
            delegates=sorted(set(re.findall(r'\b(xcf_load_(?:parasite|old_paths|old_vectors))\s*\(',body)))
            typestr='; '.join(f"{x['type']} -> {x['destination']} [{x['count']}]" for x in typ) or ('delegated '+','.join(delegates) if delegates else 'presence marker; zero payload')
            legacystr='; '.join(f"{x['type']} -> {x['destination']} [{x['count']}]" for x in oldtyp) or ('presence/delegated record' if old_present else 'not present in pinned legacy format')
            result.append(row(owner+'.'+prop,route,OWNERS[owner],prop,typestr,legacystr,default,
                              default,[LOAD+'#'+fn],[SAVE+'#'+savefn],evidence,
                              legacy_reader=[LOAD+'#'+oldfn] if old_present else [],
                              legacy_writer=[SAVE+'#xcf_save_'+owner+'_props'] if old_present else [],
                              reads=typ,legacy_reads=oldtyp,delegates=delegates,writer_calls=write_calls,
                              legacy_writer_calls=old_write,canonical_writer_fields=canonical,legacy_default=legacy_default,wire_tag=native_tags[prop],
                              legacy_property=oldprop if old_present else None,legacy_wire_tag=old_tags[oldprop] if old_present else None,
                              disposition='dynamic-schema' if delegates or prop in ('PROP_ITEM_SET','PROP_FILTER_ARGUMENT','PROP_PARASITES') else 'saved-field'))
            if len(typ)>1 or any(t['count']!='1' for t in typ):
                used={}
                for t in typ:
                    dest=t['destination']; index=used.get(dest,0); used[dest]=index+1
                    name=dest+(f'@alternative{index}' if index else '')
                    key=owner+'.'+prop+'.'+re.sub(r'[^a-zA-Z0-9_.-]','_',name)
                    member_absence='When property is absent: '+default+'; within a present admitted property branch this member is required'
                    result.append(row(key,route,OWNERS[owner],prop+'/'+name,t['type']+' count='+t['count'],
                                      '; '.join(x['type']+' count='+x['count'] for x in oldtyp if x['destination']==dest) or 'not same-named in legacy; parent carries exact legacy read schema',
                                      member_absence,member_absence,[LOAD+'#'+fn],[SAVE+'#xcf_save_prop'],
                                      [proof(root,LOAD,fn,t['expression'])],
                                      legacy_reader=[LOAD+'#'+oldfn] if old_present else [],legacy_writer=[SAVE+'#xcf_save_prop'] if old_present else [],
                                      read=t,legacy_default=legacy_default,wire_tag=native_tags[prop],legacy_wire_tag=old_tags[oldprop] if old_present else None,disposition='dynamic-schema' if t['count']!='1' else 'saved-field'))
    return result

CONFIGS=[('app/text/gimptext.c','GimpText','gimp-text-layer'),('app/core/gimpgrid.c','GimpGrid','gimp-image-grid'),
         ('app/core/gimpsymmetry.c','GimpSymmetry','gimp-image-symmetry:{subtype}'),
         ('app/core/gimpsymmetry-mirror.c','GimpMirror','gimp-image-symmetry:GimpMirror'),
         ('app/core/gimpsymmetry-tiling.c','GimpTiling','gimp-image-symmetry:GimpTiling'),
         ('app/core/gimpsymmetry-mandala.c','GimpMandala','gimp-image-symmetry:GimpMandala')]

def config_fields(root):
    result=[]
    from test_persistence_config_contract import Source, registrations
    config_source=Source(root)
    for path,owner,prefix in CONFIGS:
        text=source(str(root),path); members=config_members(text)
        try:
            old=config_members(source(str(root),path,'legacy'))
            old_precise={m['name']:m for m in registrations(config_source,path,'legacy')}
        except subprocess_error(): old={}
        for name,m in members.items():
            default=m['default']; ev=[proof(root,path,m['macro'],m['expression'])]
            if default in ('black','white','gray'):
                ev.append(proof(root,path,'gegl_color_new',f'gegl_color_new ("{default}")')); default='GeglColor '+default
            elif default=='language':
                ev.append(proof(root,path,'gimp_text_class_init','language = gimp_get_default_language (NULL);')); default='gimp_get_default_language(NULL) at class initialization'
            elif default=='&identity':
                ev.append(proof(root,path,'gimp_text_class_init','gimp_matrix2_identity (&identity);')); default='identity matrix2 {1,0;0,1}'
            if m['type']=='FONT':
                default='GimpFont standard font: construct-time NULL ParamSpec value is replaced by gimp_font_get_standard() in setter'
                ev.append(proof(root,path,'gimp_text_set_property','g_set_object (&text->font, GIMP_FONT (gimp_font_get_standard ()));'))
            if owner=='GimpTiling' and name in ('interval-x','interval-y'):
                axis='width' if name.endswith('x') else 'height'; member=name.replace('-','_')
                expr=f'tiling->{member} = gimp_image_get_{axis} (sym->image) / 2;'
                default='constructed image '+axis+'/2 using integer division; overrides declared default '+m['default']
                ev.append(proof(root,path,'gimp_tiling_constructed',expr))
            if owner=='GimpSymmetry' and name=='version':
                default='constructor0; parasite reader sets-1 before deserialize and rejects payload if version remains-1'
                ev.append(proof(root,path,'gimp_symmetry_from_parasite','g_object_set (symmetry, "version", -1, NULL);'))
                ev.append(proof(root,path,'gimp_symmetry_from_parasite','if (version == -1)'))
            if owner=='GimpText' and name=='hinting':
                default='TRUE compatibility view of hint_style!=NONE; setter only changes hint_style while it is MEDIUM'
                ev.append(proof(root,path,'gimp_text_get_property','text->hint_style != GIMP_TEXT_HINT_STYLE_NONE'))
                ev.append(proof(root,path,'gimp_text_set_property','if (text->hint_style == GIMP_TEXT_HINT_STYLE_MEDIUM)'))
            reader=[path+'#'+('gimp_text_deserialize_property' if owner=='GimpText' else 'gimp_grid_from_parasite' if owner=='GimpGrid' else 'gimp_symmetry_from_parasite')]
            writer=[path+'#'+('gimp_text_serialize_property' if owner=='GimpText' else 'gimp_grid_to_parasite' if owner=='GimpGrid' else 'gimp_symmetry_to_parasite')]
            if owner in ('GimpMirror','GimpTiling','GimpMandala'):
                reader=['app/core/gimpsymmetry.c#gimp_symmetry_from_parasite']; writer=['app/core/gimpsymmetry.c#gimp_symmetry_to_parasite']
            legacy=old.get(name)
            resolved_legacy=old_precise.get(name) if legacy else None
            if resolved_legacy:
                ev.append(dict(path=path,revision='legacy',anchor=resolved_legacy['anchor'],
                               expected=resolved_legacy['expected'],scope='parsed exact legacy GParamSpec registration and default initialization'))
            macrotext=source(str(root),'libgimpconfig/gimpconfig-params.h')
            macroblock=re.search(r'#define\s+'+re.escape(m['macro'])+r'\([^\n]*\)(.*?)(?=\n\s*\n)',macrotext,re.S)
            assert macroblock,m['macro']
            bases='G_PARAM_READWRITE | GIMP_CONFIG_PARAM_SERIALIZE' if m['type'] in ('OBJECT','BOXED','POINTER','COLOR') else 'GIMP_CONFIG_PARAM_FLAGS (READWRITE|CONSTRUCT|STATIC_STRINGS|SERIALIZE)'
            assert ('GIMP_CONFIG_PARAM_FLAGS' in macroblock[1]) == (m['type'] not in ('OBJECT','BOXED','POINTER','COLOR'))
            storage_type='resource name string or NULL' if owner=='GimpText' and name=='outline-pattern' else 'markup string plus markupfont mappings with nested font identity schema' if owner=='GimpText' and name=='markup' else 'GimpConfig '+m['type']
            result.append(row('config.'+owner+'.'+name,'05',owner,prefix+'/'+name,storage_type,
                              'GimpConfig '+legacy['type'] if legacy else 'not in legacy owner schema',default,
                              'Within a present config payload, unmentioned member retains constructor default: '+default,
                              reader,writer,ev,flags=m['flags']+' | '+bases+' from '+m['macro'],
                              legacy_reader=[path+'#'+legacy['macro']] if legacy else [],legacy_writer=[path+'#'+legacy['macro']] if legacy else [],
                              registration=m,legacy_registration=legacy,
                              legacy_default=resolved_legacy['default'] if resolved_legacy else 'member absent from legacy owner schema',
                              legacy_flags=resolved_legacy['flags'] if resolved_legacy else 'not serialized by legacy owner',
                              legacy_native_type=resolved_legacy['type'] if resolved_legacy else 'not present'))
    # Non-macro member: the value-array itself is persisted; its element spec
    # is a parameterized member rather than an additional top-level key.
    t=source(str(root),'app/text/gimptext.c')
    for name,typ,default,expr in [
      ('outline-dash-info','GimpValueArray of gdouble','NULL (no dash pattern)', 'gimp_param_spec_value_array ("outline-dash-info", NULL, NULL, array_spec, GIMP_PARAM_STATIC_STRINGS | GIMP_CONFIG_PARAM_FLAGS)'),
      ('outline-dash-info[*]','gdouble in [0.0,2000.0]','1.0 element ParamSpec default; absent array has no elements','g_param_spec_double ("outline-dash-length", NULL, NULL, 0.0, 2000.0, 1.0, GIMP_PARAM_READWRITE)')]:
        result.append(row('config.GimpText.'+name,'05','GimpText','gimp-text-layer/'+name,typ,'not in legacy GimpText',default,default,
                          ['app/text/gimptext.c#gimp_text_deserialize_property'],['app/text/gimptext.c#gimp_text_serialize_property'],
                          [proof(root,'app/text/gimptext.c','gimp_text_class_init',expr)],flags='GIMP_CONFIG_PARAM_FLAGS on array; element GIMP_PARAM_READWRITE',
                          disposition='dynamic-schema' if '[*]' in name else 'saved-field'))
    return result

def extra_native_fields(root):
    result=[]
    def add(key,owner,name,typ,default,reader,writer,ev,legacy='native-only member',route='05',disposition='saved-field'):
        result.append(row(key,route,owner,name,typ,legacy,default,default,reader,writer,ev,disposition=disposition))
    # Custom font serializer is not a GObject-property serializer.
    path='app/text/gimpfont.c'; text=source(str(root),path)
    writer=function(text,'gimp_font_serialize'); reader=function(text,'gimp_font_deserialize_create')
    names=[a[1].strip('"') for _,a,_,_ in calls(writer,'gimp_config_writer_open')]
    for name in names:
        typ='string' if name in ('fonthash','fullname','family','style','psname') else 'gint decimal'
        val='NULL' if typ=='string' else '-1'
        match=re.search(r'\b'+name+r'\s*=\s*'+re.escape(val)+r'\s*;',reader); assert match,name
        add('font.'+name,'GimpFont','gimp-text-layer/font/'+name,typ,
            val+' search discriminator when member absent; empty serialized standard font resolves via gimp_font_get_standard()',
            [path+'#gimp_font_deserialize_create'],[path+'#gimp_font_serialize'],
            [proof(root,path,'gimp_font_deserialize_create',match[0]),proof(root,path,'gimp_font_serialize',f'gimp_config_writer_open (writer, "{name}")')],
            legacy='legacy text font is a name string, not a font identity dictionary')
    for name,typ,expr in [('markupfont.lookup-name','string','gimp_config_writer_string (writer, font_name+4)'),
                          ('markupfont.font','nested GimpFont identity config','gimp_config_writer_open (writer, "font")')]:
        add('text.'+name,'GimpText','gimp-text-layer/markup/'+name,typ,'no font mapping when markup is NULL; one mapping per distinct referenced font',
            ['app/text/gimptext.c#gimp_text_deserialize_property'],['app/text/gimptext.c#gimp_text_serialize_property'],
            [proof(root,'app/text/gimptext.c','gimp_text_serialize_property',expr)],disposition='dynamic-schema')
    # Finite image parasite names, separate from arbitrary third-party bytes.
    for key,typ,default,path,anchor,expr in [
      ('icc-profile','ICC byte payload','no embedded profile: gimp_image_get_builtin_color_profile based on image base type and transfer response curve','app/core/gimpimage-color-profile.c','gimp_image_get_builtin_color_profile','gimp_babl_get_builtin_color_profile (gimp_babl_format_get_base_type (format), gimp_babl_format_get_trc (format))'),
      ('simulation-icc-profile','ICC byte payload','NULL simulation_profile','app/core/gimpimage.c','gimp_image_init','private->simulation_profile = NULL;'),
      ('image-simulation-intent','single uint8 GimpColorRenderingIntent','GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC','app/core/gimpimage.c','gimp_image_init','private->simulation_intent = GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC;'),
      ('image-simulation-bpc','single uint8 Boolean','FALSE','app/core/gimpimage.c','gimp_image_init','private->simulation_bpc = FALSE;'),
      ('gimp-image-metadata','serialized GimpMetadata XML (XMP/Exif/IPTC schema owned by metadata library)','fresh GimpMetadata from gimp_create_image; no embedded metadata is overlaid','app/core/gimp.c','gimp_create_image','metadata = gimp_metadata_new ();')]:
        add('metadata.'+key,'GimpImage',key,typ,default,[LOAD+'#xcf_load_image'],[SAVE+'#xcf_save_image_props'],
            [proof(root,path,anchor,expr)],legacy='persistent parasite byte payload if present',disposition='dynamic-schema' if 'payload' in typ or 'XML' in typ else 'saved-field')
    add('metadata.gimp-comment','GimpImage persistent parasite','gimp-comment','NUL-terminated comment bytes; size=strlen(comment)+1',
        'XCF load calls gimp_create_image(...,FALSE), so missing comment stays absent. New image with attach_comment TRUE copies config->default_image comment only when non-NULL; template ParamSpec NULL and configured default-image comment are distinct.',
        [LOAD+'#xcf_load_parasite'],[SAVE+'#xcf_save_parasite'],
        [proof(root,'app/core/gimp.c','gimp_create_image','if (attach_comment)'),
         proof(root,'app/core/gimp.c','gimp_create_image','comment = gimp_template_get_comment (gimp->config->default_image);'),
         proof(root,'app/core/gimp.c','gimp_create_image','gimp_parasite_new ("gimp-comment", GIMP_PARASITE_PERSISTENT, strlen (comment) + 1, comment)'),
         proof(root,LOAD,'xcf_load_image','gimp_create_image (gimp, width, height, image_type, precision, FALSE)'),
         proof(root,LOAD,'xcf_load_image','gimp_create_image (gimp, width, height, image_type, FALSE)','legacy')],
        legacy='same persistent comment byte payload; old XCF loader also passes attach_comment FALSE')
    for name,typ,rule in [
        ('exif-data','Exif bytes','gimp_metadata_set_from_exif (metadata, parasite_data, parasite_data_size, &my_error)'),
        ('gimp-metadata','GIMP_XMP_1 prefix plus XMP bytes','gimp_metadata_set_from_xmp (metadata, (const guint8 *) xmp_data + 10, xmp_length - 10, &my_error)'),
        ('gimp-xcf-compatibility-mode','obsolete uninterpreted byte payload','gimp_parasite_list_remove (private->parasites, gimp_parasite_get_name (parasite))')]:
        add('metadata.'+name,'GimpImage imported persistent parasite',name,typ,
            'absent=no migration action; present payload is consumed/removed; '+('unused compatibility marker is discarded' if name=='gimp-xcf-compatibility-mode' else 'gimp-image-metadata wins; otherwise decode through metadata library and canonical writer emits gimp-image-metadata'),
            [LOAD+'#xcf_load_image'],[SAVE+'#xcf_save_image_props'],
            [proof(root,LOAD,'xcf_load_image','gimp_image_parasite_find (GIMP_IMAGE (image), "'+name+'")'),proof(root,LOAD,'xcf_load_image',rule)],
            legacy='opaque persistent parasite bytes; no internal metadata-key Cartesian expansion',disposition='dynamic-schema')
    for key,default,path,reader,writer,expr in [
      ('gimp-image-grid','missing whole parasite retains config->default_grid duplicate; present payload missing members uses GimpGrid constructor defaults','app/core/gimpgrid.c','gimp_grid_from_parasite','gimp_grid_to_parasite','gimp_config_deserialize_parasite'),
      ('gimp-image-symmetry:{subtype}','missing whole parasite leaves no stored nonidentity symmetry; present payload requires explicit version','app/core/gimpsymmetry.c','gimp_symmetry_from_parasite','gimp_symmetry_to_parasite','gimp_config_deserialize_parasite')]:
        add('metadata.'+key,'GimpImage',key,'GimpConfig text payload; fixed members enumerated by config owner',default,
            [path+'#'+reader],[path+'#'+writer],[proof(root,path,reader,expr),
            proof(root,'app/core/gimpimage.c','gimp_image_constructed' if key=='gimp-image-grid' else 'gimp_image_init',
                  'private->grid = gimp_config_duplicate (GIMP_CONFIG (config->default_grid));' if key=='gimp-image-grid' else 'private->symmetries = NULL;')],legacy='grid config serialized; symmetry absent from legacy',disposition='dynamic-schema')
    # Full property framing also applies to unknown ordered records.
    for name,typ,expr in [('tag','u32 BE','xcf_read_int32 (info, (guint32 *) prop_type, 1)'),
                          ('declared-length','u32 BE','xcf_read_int32 (info, (guint32 *) prop_size, 1)')]:
        add('framing.'+name,'all XCF property owners','property/'+name,typ,'required; no substitute for a missing framing word',
            [LOAD+'#xcf_load_prop'],[SAVE+'#xcf_save_prop'],[proof(root,LOAD,'xcf_load_prop',expr)],legacy=typ,route='01')
    add('framing.signature-version','GimpImage/XcfInfo','signature/version','14 bytes: gimp xcf file\\0 or gimp xcf vNNN\\0',
        'required; no default signature; file means version0; vNNN must be an admitted version',
        ['app/xcf/xcf.c#xcf_load_stream_with_dialect'],[SAVE+'#xcf_save_image'],
        [proof(root,'app/xcf/xcf.c','xcf_load_stream_with_dialect','memcmp (id, "gimp xcf ", 9) || id[13] != \'\\0\''),
         proof(root,SAVE,'xcf_save_image','xcf_write_int8_check_error (info, (guint8 *) version_tag, 14, ;)')],legacy='same14-byte framing; genuine old v4 has no precision word',route='01')
    add('framing.unknown-payload','image/layer/channel/mask/path/effect','unknown-property[*]/bytes','declared-length opaque bytes',
        'no unknown records by default; unknown ordered sequences are retained exactly',
        [LOAD+'#xcf_skip_unknown_prop'],[PRESERVE+'#preserve_records'],[proof(root,LOAD,'xcf_skip_unknown_prop','xcf_seek_pos (info, info->cp + size, NULL)')],
        legacy='old reader skips unknown length bytes; old writer has no generic preservation route',route='01',disposition='dynamic-schema')
    for name,typ,default,writer,expr in [
      ('hierarchy.upper-level.width','u32 BE','previous width/2 for each dummy higher level','xcf_save_buffer','width /= 2;'),
      ('hierarchy.upper-level.height','u32 BE','previous height/2 for each dummy higher level','xcf_save_buffer','height /= 2;'),
      ('hierarchy.upper-level.terminator','u32 BE even at XCF11+','0; intentionally not a64-bit offset','xcf_save_buffer','xcf_write_int32_check_error (info, (guint32 *) &tmp1, 1, ;)'),
      ('level.tile-offset-table.terminator','offset:32-bit before11,64-bit from11','0 at final slot','xcf_save_level','xcf_write_zero_offset_check_error (info, ntiles + 1, ;)')]:
        add('pixels.'+name,'GeglBuffer owned by GimpDrawable',name,typ,default,[LOAD+'#xcf_load_buffer'],[SAVE+'#'+writer],
            [proof(root,SAVE,writer,expr)],legacy='u32 BE higher-level dimensions/offset terminator',route='01',disposition='dynamic-schema')
    for name,fn,writer in [('uncompressed','xcf_load_tile','xcf_save_tile'),('RLE','xcf_load_tile_rle','xcf_save_tile_rle'),('ZLIB','xcf_load_tile_zlib','xcf_save_tile_zlib')]:
        add('pixels.'+name,'GeglBuffer owned by GimpDrawable','tile[*]/'+name,'image-format component bytes in64x64 edge-clipped tiles',
            'required for referenced tile; empty level allowed when first tile offset is0; incomplete drawable pixels are retained but save is refused',
            [LOAD+'#'+fn],[SAVE+'#'+writer],[proof(root,LOAD,fn,'gegl_buffer_set')],
            legacy='native byte tile layout; legacy uses8-bit components and NONE/RLE',route='01',disposition='dynamic-schema')
    for parent,reader,writer in [('VECTORS.path','xcf_load_old_vector','xcf_save_old_vectors'),('path','xcf_load_path','xcf_save_path')]:
        for axis,index,default in [('x',0,'required'),('y',1,'required'),('pressure',2,'1.0'),('xtilt',3,'0.0'),('ytilt',4,'0.0'),('wheel',5,'0.5')]:
            ev=[proof(root,LOAD,reader,f'anchor.position.{axis} = coords[{index}];'),
                proof(root,LOAD,reader,'gfloat coords[13] = GIMP_COORDS_DEFAULT_VALUES;'),
                proof(root,LOAD,reader,'if (num_axes < 2 || num_axes > 6)')]
            if index>=2:
                symbol='PRESSURE' if axis=='pressure' else 'WHEEL' if axis=='wheel' else 'TILT'
                ev.append(proof(root,'app/core/core-types.h','GIMP_COORDS_DEFAULT_'+symbol,'#define GIMP_COORDS_DEFAULT_'+symbol+' '+default))
            add('point.'+parent+'.'+axis,'GimpPath/GimpStroke/GimpAnchor',parent+'/stroke[*]/point[*]/'+axis,'f32 BE -> GimpCoords gdouble',
                default+' when axis omitted; point axes prefix has length2..6; writer normally emits x/y only',
                [LOAD+'#'+reader],[SAVE+'#'+writer],ev,legacy='legacy VECTORS same prefix; old GimpVectors owner; no native standalone path before18',route='01',disposition='dynamic-schema')
    # Recognized legacy dynamic-text payload, including intentionally ignored
    # fields.  Canonical native save is GimpText config, not a GDT10 writer.
    path='app/text/gimptext-parasite.c'; text=source(str(root),path); reader='gimp_text_from_gdyntext_parasite'
    tokens=['TEXT','ANTIALIAS','ALIGNMENT','ROTATION','LINE_SPACING','COLOR','LAYER_ALIGNMENT','XLFD']
    enum=re.search(r'enum\s*\{(.*?)NUM_PARAMS',text,re.S); assert enum
    exact(re.findall(r'\b([A-Z_]+)\s*=\s*\d+',enum[1]),tokens,'gdyntext fixed field domain')
    for i,name in enumerate(tokens):
        typ={'TEXT':'compressed escaped bytes -> UTF8 text','ANTIALIAS':'atoi Boolean','ALIGNMENT':'atoi0=left,1=center,2=right; other=left',
             'ROTATION':'legacy textual rotation, ignored by native interpreter','LINE_SPACING':'g_strtod double','COLOR':'hex integer -> opaque RGB',
             'LAYER_ALIGNMENT':'legacy textual alignment, ignored by native interpreter','XLFD':'XLFD font description string'}[name]
        add('gdyntext.'+name,'GimpText imported from legacy parasite',f'plug_in_gdyntext/data/GDT10{{}}[{i}:{name}]',typ, 'all8 tokens required; missing token rejects the text payload; ignored fields do not override native constructor values',
            [path+'#'+reader],['app/text/gimptext-parasite.c#gimp_text_to_parasite'],
            [proof(root,path,reader,'params = g_strsplit (str + strlen ("GDT10{"), "}{", -1);'),
             proof(root,path,reader,'for (i = 0; i < NUM_PARAMS; i++) if (!params[i]) goto cleanup;')],
            legacy='same GDT10 delimited member; no writer in this repository',disposition='dynamic-schema')
    return result

def effect_config_fields(root):
    """Open operation/property domain, with finite common config ancestors."""
    result=[]
    types={'INT':'G_TYPE_INT','UINT':'G_TYPE_UINT','BOOL':'G_TYPE_BOOLEAN','FLOAT':'G_TYPE_FLOAT or G_TYPE_DOUBLE',
           'STRING':'G_TYPE_STRING','ENUM':'subtype of G_TYPE_ENUM','CONFIG':'subtype of GIMP_TYPE_CONFIG','COLOR':'subtype of GEGL_TYPE_COLOR'}
    savebody=function(source(str(root),SAVE),'xcf_save_effect_props')
    actual=set(re.findall(r'filter_type\s*=\s*FILTER_PROP_([A-Z]+)',savebody))-{'UNKNOWN'}
    exact(actual,set(types),'GEGL argument writable type domain')
    for tag,typ in types.items():
        ev=[proof(root,SAVE,'xcf_save_effect_props','pspecs = gegl_operation_list_properties (operation, &n_pspecs);'),
            proof(root,SAVE,'xcf_save_effect_props','gegl_node_get_property (node, pspec->name, &value);'),
            proof(root,LOAD,'xcf_load_effect','gegl_node_set (filter->operation, "operation", filter->operation_name, NULL);'),
            proof(root,LOAD,'xcf_load_effect_props','gegl_operation_find_property (filter->operation_name, filter_prop_name)'),
            proof(root,SAVE,'xcf_save_effect_props','filter_type = FILTER_PROP_'+tag+';')]
        default='For the exact installed operation and property-name, leave the freshly created GEGL operation property unchanged when record absent; construction/installed ParamSpec defines its value. No universal literal is asserted.'
        result.append(row('effect-domain.'+tag,'01','installed GEGL operation/property identified by saved operation string and pspec->name',
                          'PROP_FILTER_ARGUMENT[property-name]/'+tag,typ+' mapped to FILTER_PROP_'+tag,'native-only effect schema',default,
                          default,[LOAD+'#xcf_load_effect_props'],[SAVE+'#xcf_save_effect_props'],ev,
                          version='native XCF effects',flags='all gegl_operation_list_properties members of admitted type; unknown types are not emitted as interpreted values',
                          disposition='dynamic-schema',domain={'operation':'installed safe operation with admitted saved version','property':'gegl_operation_list_properties(operation) / gegl_operation_find_property(operation,name)','type':typ}))
    from test_persistence_config_contract import Source, registrations, evidence
    src=Source(root)
    for path in ('app/core/gimpsettings.c','app/core/gimpviewable.c'):
        for m in registrations(src,path,'native'):
            result.append(row('effect-config.'+m['owner']+'.'+m['name'],'01',m['owner'],
                              'CONFIG[dynamic-GType]/'+m['name'],m['type'],'native effect payload only',m['default'],
                              'Member absent in present config: '+m['default'],
                              [LOAD+'#xcf_load_effect_props','libgimpconfig/gimpconfig-deserialize.c#gimp_config_deserialize_properties'],
                              [SAVE+'#xcf_save_prop','libgimpconfig/gimpconfig-serialize.c#gimp_config_serialize_property'],
                              [evidence(m)],flags=m['flags'],registration={k:v for k,v in m.items() if k not in ('start','end')},disposition='unchanged-standard'))
    result.append(row('effect-config.parameterized-member','01','dynamic pspec->value_type implementing GimpConfig',
                      'CONFIG[dynamic-GType]/serialized-property[*]','GParamSpec value_type with GimpConfig custom-property hooks','native effect payload only',
                      'g_object_new(pspec->value_type,NULL) constructs defaults before deserialize; absent members retain those exact constructor values',
                      'no universal scalar default; missing record leaves operation property unchanged; missing nested member leaves newly constructed config property unchanged',
                      [LOAD+'#xcf_load_effect_props'],[SAVE+'#xcf_save_prop','libgimpconfig/gimpconfig-serialize.c#gimp_config_serialize_property'],
                      [proof(root,LOAD,'xcf_load_effect_props','config = g_object_new (pspec->value_type, NULL);'),
                       proof(root,LOAD,'xcf_load_effect_props','gimp_config_deserialize_string (GIMP_CONFIG (config), serialized, -1, NULL, &error)'),
                       proof(root,SAVE,'xcf_save_prop','gimp_config_serialize_to_string (config, NULL)'),
                       proof(root,'libgimpconfig/gimpconfig-serialize.c','gimp_config_serialize_property','if (! (param_spec->flags & GIMP_CONFIG_PARAM_SERIALIZE))'),
                       proof(root,'libgimpconfig/gimpconfig-serialize.c','gimp_config_serialize_property','param_spec->flags & GIMP_CONFIG_PARAM_IGNORE || param_spec->flags & GIMP_PARAM_DONT_SERIALIZE')],
                      flags='SERIALIZE required; IGNORE/DONT_SERIALIZE omitted; DEFAULTS omits g_param_value_defaults matches; custom serializer may encode nested members',
                      disposition='dynamic-schema',domain={'owner':'runtime installed GType implementing GimpConfig','members':'g_object_class_list_properties including inherited properties, filtered by serialization flags; custom config interface semantics remain explicit'}))
    return result

def subprocess_error():
    import subprocess
    return subprocess.CalledProcessError

def record_writes(body):
    result=[]
    for name,args,expression,position in calls(body,r'xcf_write_(?:int8|int16|int32|int64|float|double|offset|string)(?:_check_error)?'):
        kind=name.removeprefix('xcf_write_').removesuffix('_check_error')
        result.append(dict(type=READ_TYPES[kind],source=args[1],count=args[2],expression=expression))
    return result

def structured_writer_member(label, transfer, ordinal, body):
    """Bind each reader slot to concrete writer transfer(s), including aliases.

    These checked alias rules are deliberately independent of the ledger, and
    distinguish canonical writer encodings from older admitted reader forms.
    """
    name=re.sub(r'^\([^)]*\)\s*','',transfer['destination']).lstrip('&')
    target=name; rules=[]; saved=name
    if label=='image-header':
        if name in ('width','height','image_type','p'):
            getter={'width':'width','height':'height','image_type':'base_type','p':'precision'}[name]
            rules=['value = gimp_image_get_'+getter+' (image);']; target='value'; saved='precision' if name=='p' else name
        elif name=='offset':
            saved=['layer-offset[*]','channel-offset[*]','path-offset[*]'][ordinal]
            rules=['for (list = '+['all_layers','all_channels','all_paths'][ordinal]+'; list; list = g_list_next (list))']
    elif label in ('layer-header','channel-header','mask-header'):
        owner='layer' if label=='layer-header' else 'channel'
        if name in ('width','height'):
            target='value'; rules=['value = gimp_item_get_'+name+' (GIMP_ITEM ('+owner+'));']
        elif name=='type':
            target='value'; rules=['value = gimp_babl_format_get_image_type (gimp_drawable_get_format (GIMP_DRAWABLE (layer)));']
        elif name=='name': target='string'; rules=['string = gimp_object_get_name ('+owner+');']
        elif name=='hierarchy_offset': target='offset'; rules=['offset = info->cp + '+('(2 + num_effects + 1) * info->bytes_per_offset;' if owner=='layer' else 'info->bytes_per_offset;')]
        elif name=='layer_mask_offset': target='offset'; rules=['if (gimp_layer_get_mask (layer))']
        elif name=='effects_offset': target='offset'; saved='effect-offset[*]'+(' next' if ordinal else ' first'); rules=['if (num_effects > 0)']
    elif label=='effect-header':
        if name=='string':
            target=['name','icon','operation','op_version'][ordinal]; saved=target
            if ordinal==0: rules=['if (has_custom_name)','gchar *empty = NULL;']
            if ordinal==3: rules=['if (info->file_version >= 22)','op_version = gegl_operation_get_op_version (operation);']
        elif name=='mask_offset': target='offset'; rules=['offset = info->cp + info->bytes_per_offset;']
    elif label=='parasite':
        target={'name':'string','flags':'value','size':'value','data':'parasite_data'}[name]
        rules=[{'name':'string = gimp_parasite_get_name (parasite);','flags':'value = gimp_parasite_get_flags (parasite);','size':'parasite_data = gimp_parasite_get_data (parasite, &value);','data':'parasite_data = gimp_parasite_get_data (parasite, &value);'}[name]]
    elif label=='level' and name=='offset': target='offset_table'; saved='tile-offset[*]'+(' next' if ordinal else ' first')
    elif label=='PATHS' and name=='last_selected_row': target='active_index'; saved='selected-row'
    elif label=='PATHS.path' and name=='dummy': target='pathtype'; saved='path-type'+('@version3' if ordinal else '@version2')
    elif label in ('path-header','VECTORS.path'):
        if name=='name' and label=='path-header': target='string'; rules=['string = gimp_object_get_name (vectors);']
        elif name=='plength': target='size'; saved='payload-size'; rules=['size = info->cp - base;']
        elif name=='stroke_type_id': target='stroke_type'
        elif name=='num_control_points': target='control_points->len'
        elif name=='coords': rules=['num_axes = 2;']; saved='point[*]/coordinates[0..num_axes-1]'
    writes=record_writes(body)
    selected=[w for w in writes if re.sub(r'^\([^)]*\)\s*','',w['source']).lstrip('&')==target]
    assert selected,(label,name,target,'unmapped concrete writer member')
    for rule in rules: assert norm(rule) in norm(body),(label,name,rule)
    if label=='effect-header' and name=='string' and ordinal==0:
        selected += [w for w in writes if 'empty' in w['source']]
    return saved,selected,rules

def structured_fields(root):
    """Every primitive member of fixed record layouts, independently extracted.

    Repeated calls in version alternatives are kept as named variants.  Array
    counts remain symbolic domains rather than expanded fake pixel/point keys.
    """
    layouts=[
      ('image-header','xcf_load_image','xcf_save_image','xcf_load_image','xcf_save_image'),
      ('layer-header','xcf_load_layer','xcf_save_layer','xcf_load_layer','xcf_save_layer'),
      ('channel-header','xcf_load_channel','xcf_save_channel','xcf_load_channel','xcf_save_channel'),
      ('mask-header','xcf_load_layer_mask','xcf_save_channel','xcf_load_layer_mask','xcf_save_channel'),
      ('effect-header','xcf_load_effect','xcf_save_effect',None,None),
      ('path-header','xcf_load_path','xcf_save_path',None,None),
      ('hierarchy','xcf_load_buffer','xcf_save_buffer','xcf_load_hierarchy','xcf_save_hierarchy'),
      ('level','xcf_load_level','xcf_save_level','xcf_load_level','xcf_save_level'),
      ('parasite','xcf_load_parasite','xcf_save_parasite','xcf_load_parasite','xcf_save_parasite'),
      ('PATHS','xcf_load_old_paths','xcf_save_old_paths','xcf_load_old_paths','xcf_save_old_paths'),
      ('PATHS.path','xcf_load_old_path','xcf_save_old_paths','xcf_load_old_path','xcf_save_old_paths'),
      ('VECTORS','xcf_load_old_vectors','xcf_save_old_vectors','xcf_load_vectors','xcf_save_vectors'),
      ('VECTORS.path','xcf_load_old_vector','xcf_save_old_vectors','xcf_load_vector','xcf_save_vectors')]
    result=[]
    for label,reader,writer,oldreader,oldwriter in layouts:
        body=function(source(str(root),LOAD),reader)
        # Image/header functions may contain additional metadata raw reads;
        # they are still saved members and retain their exact decoder position.
        reads=transfers(body); legacy=[]
        writebody=function(source(str(root),SAVE),writer)
        oldwritebody=function(source(str(root),SAVE,'legacy'),oldwriter) if oldwriter else None
        if oldreader:
            try: legacy=transfers(function(source(str(root),LOAD,'legacy'),oldreader))
            except AssertionError: oldreader=None; oldwriter=None
        seen={}
        for transfer in reads:
            name=transfer['destination']; ordinal=seen.get(name,0); seen[name]=ordinal+1
            key=re.sub(r'[^a-zA-Z0-9_.-]','_',label+'.'+name)+('.variant'+str(ordinal) if ordinal else '')
            matching=[v for v in legacy if v['destination']==name]
            default='required record member in this decoder branch; no declared substitute value (truncation behavior is a separate runtime gate)'
            if label=='image-header' and name=='(guint32 *) &p': default='absent in genuine legacy and native versions<4: GIMP_PRECISION_U8_NON_LINEAR; native4/5/6 use explicit precision remap'
            if label=='effect-header' and name=='&string' and ordinal==3: default='NULL for XCF<22; required operation-version string slot for XCF22+'
            if label=='PATHS.path' and 'tattoo' in name: default='0 when path version<3; allocate native tattoo unless nonzero saved tattoo supplied'
            if label=='VECTORS.path' and name=='coords': default='coordinates[0..num_axes-1] required; omitted pressure=1.0,xtilt=0.0,ytilt=0.0,wheel=0.5 from GIMP_COORDS_DEFAULT_VALUES; num_axes 2..6'
            saved,write_members,write_rules=structured_writer_member(label,transfer,ordinal,writebody)
            ev=[proof(root,LOAD,reader,transfer['expression'])]
            ev += [proof(root,SAVE,writer,w['expression']) for w in write_members]
            ev += [proof(root,SAVE,writer,rule) for rule in write_rules]
            if label.startswith(('PATHS','VECTORS')):
                ev.append(proof(root,LOAD,oldreader,'GimpVectors','legacy'))
            if label=='PATHS.path':
                ev += [proof(root,LOAD,reader,'else if (version == 3)'),proof(root,LOAD,oldreader,'else if (version >= 3)','legacy')]
            if label=='parasite': ev.append(proof(root,SAVE,writer,'if (gimp_parasite_is_persistent (parasite))'))
            if label=='image-header' and name=='(guint32 *) &p':
                ev += [proof(root,LOAD,reader,'GimpPrecision precision = GIMP_PRECISION_U8_NON_LINEAR;'),
                       proof(root,LOAD,reader,'if (! info->painter_legacy && info->file_version >= 4)')]
            if label=='effect-header' and name=='&string' and ordinal==3:
                ev += [proof(root,LOAD,reader,'if (info->file_version >= 22)'),proof(root,LOAD,reader,'filter = g_new0 (FilterData, 1);')]
            if label=='PATHS.path' and 'tattoo' in name:
                ev += [proof(root,LOAD,reader,'GimpTattoo tattoo = 0;'),proof(root,LOAD,reader,'else if (version == 3)')]
            if name=='coords':
                ev += [proof(root,LOAD,reader,'gfloat coords[13] = GIMP_COORDS_DEFAULT_VALUES;'),
                       proof(root,LOAD,reader,'if (num_axes < 2 || num_axes > 6)')]
                for symbol,value in [('PRESSURE','1.0'),('TILT','0.0'),('WHEEL','0.5')]:
                    ev.append(proof(root,'app/core/core-types.h','GIMP_COORDS_DEFAULT_'+symbol,'#define GIMP_COORDS_DEFAULT_'+symbol+' '+value))
            record_owner={'image-header':'GimpImage/XcfInfo','layer-header':'GimpLayer/GimpDrawable','channel-header':'GimpChannel','mask-header':'GimpLayerMask','effect-header':'GimpDrawableFilter/FilterData','path-header':'GimpPath/GimpStroke/GimpAnchor','hierarchy':'GeglBuffer owned by GimpDrawable','level':'GeglBuffer owned by GimpDrawable','parasite':'GimpParasite in image/item parasite list','PATHS':'GimpImage selected GimpPath','PATHS.path':'GimpPath compatibility points','VECTORS':'GimpImage selected GimpPath','VECTORS.path':'GimpPath/GimpStroke/GimpAnchor'}[label]
            result.append(row('record.'+key,'05' if label=='parasite' else '01',record_owner,label+'/'+saved,
                              transfer['type']+' count='+transfer['count'],
                              '; '.join(v['type']+' count='+v['count'] for v in matching) or ('no same-named legacy member; exact old read-layout attached' if oldreader else 'native-only layout'),
                              default,default,[LOAD+'#'+reader],[SAVE+'#'+writer],ev,
                              legacy_reader=[LOAD+'#'+oldreader] if oldreader else [], legacy_writer=[SAVE+'#'+oldwriter] if oldwriter else [],
                              read=transfer,writer_members=write_members,writer_assignment_rules=write_rules,
                              legacy_owner=({'PATHS':'GimpImage active GimpVectors','PATHS.path':'GimpVectors compatibility points','VECTORS':'GimpImage active GimpVectors','VECTORS.path':'GimpVectors/GimpStroke/GimpAnchor','hierarchy':'TileManager owned by GimpDrawable','level':'TileManager owned by GimpDrawable'}.get(label,record_owner) if oldreader else 'no legacy counterpart'),
                              legacy_layout=legacy,legacy_writer_layout=record_writes(oldwritebody) if oldwritebody else [],
                              version='PATHS versions1,2,3 admitted natively; old admits1,2,and>=3; canonical writer3' if label.startswith('PATHS') else 'native XCF18+ standalone path version1' if label=='path-header' else 'VECTORS version1' if label.startswith('VECTORS') else 'native XCF with explicit per-member reader and writer version branches',flags='GIMP_PARASITE_PERSISTENT gate; name/flags/length/data written verbatim' if label=='parasite' else 'XCF record/version branch',
                              disposition='dynamic-schema' if transfer['count']!='1' else 'saved-field'))
    return result

def capsule_fields(root):
    text=source(str(root),PRESERVE)
    puts={}
    for name,args,expr,_ in calls(text,r'put'):
        if len(args)==2 and args[0].startswith('"'):
            puts.setdefault(args[0].strip('"'),[]).append((args[1],expr))
    for name,args,expr,_ in calls(text,r'nullable_string'):
        if len(args)>2 and args[1].startswith('"'):
            puts.setdefault(args[1].strip('"'),[]).append(('nullable NUL-terminated byte array',expr))
    puts['external-reference-origins']=[('aa{sv}','dict.put (external_field, g_variant_builder_end (&array))')]
    proof(root,PRESERVE,'external_field','"external-reference-origins"')
    rules={
      'version':('image/item/origin','u','1','required uint32=1; unknown/missing version keeps namespace opaque'),
      'dialect':('image','s','gimp-painter-standard-v1','absent marker does not select standard Painter dialect; ordinary XCF still opens'),
      'id':('item','u','chosen unique nonzero file-local item identity (prior save ID, tattoo, or collision repair)','optional redundant identity; native tattoo remains file-local reference authority'),
      'kind':('item','s','ordinary|clone|filter according to current native owner','missing/unknown kind leaves capsule inert; no inferred custom owner'),
      'legacy-mode':('layer','u','present only for gimp_painter_layer_mode_to_legacy mapping','absent means native XCF mode remains; invalid mapping keeps capsule inert'),
      'source-id':('CloneLayer','u','same-image source save ID;0 for none/external source','required in Clone envelope; malformed/missing remains inert; unmatched nonzero target becomes expired'),
      'source-state':('CloneLayer','u','snapshot state NONE|PENDING|LIVE|EXPIRED','required and consistent with pending-name, source-id and expired flag'),
      'source-expired':('CloneLayer','b','snapshot source_expired; forced TRUE for external source','required Boolean; missing/wrong type makes Clone capsule inert'),
      'allow-name-lookup':('CloneLayer','b','snapshot allow_name_lookup; FALSE for external/missing expired source without pending name','required Boolean; missing/wrong type makes Clone capsule inert'),
      'pending-name':('CloneLayer','ay','optional snapshot pending_name including final NUL','absent=NULL; present empty byte string is pending empty name, not absence'),
      'source-name':('CloneLayer','ay','optional snapshot source_name including final NUL','absent=NULL; no arbitrary name fallback to missing file identity'),
      'original-source-name':('item','ay','optional preserved imported source name including final NUL','absent=no imported lookup-name provenance'),
      'unresolved-source-id':('CloneLayer','u','old nonzero source-id copied when source is now expired and absent','absent=no saved diagnostic ID; never an active lookup candidate'),
      'procedure':('FilterLayer','ay','optional current procedure including final NUL','absent=NULL procedure; no executable procedure invented'),
      'has-definition':('FilterLayer','b','definition != nullptr','required Boolean; missing/wrong type leaves Filter capsule inert'),
      'definition':('FilterLayer','ay','raw definition bytes, empty array when no definition','required byte array if has-definition TRUE; ignored as definition when FALSE'),
      'has-arguments':('FilterLayer','b','args != nullptr, or TRUE for opaque current model','required Boolean; FALSE means no converted model; never equated to empty arguments'),
      'arguments':('FilterLayer','a(sbv) or preserved opaque v','typed current snapshot; original opaque value retained unchanged when present','has-arguments FALSE=no model; TRUE+absent is explicitly retained opaque missing model'),
      'generation':('FilterLayer','t','snapshot generation','required uint64<=G_MAXINT64; missing/wrong type leaves capsule inert'),
      'cache-generation':('FilterLayer','t','snapshot cache_generation','required uint64<=generation; missing/wrong type/inconsistent relation leaves capsule inert'),
      'cache-complete':('FilterLayer','b','snapshot cache_complete','required Boolean; missing/wrong type leaves capsule inert'),
      'saved-state':('FilterLayer','u','snapshot diagnostic execution state','required uint32<=GIMP_FILTER_LAYER_CLOSED; restored lineage enters fresh runtime epoch'),
      'original-owner':('image/item/origin archive','s','optional imported owner type text','absent=no imported owner-type evidence; never current owner authority'),
      'original-header':('image/item/origin archive','ay','optional exact imported header bytes','absent=no original header retained'),
      'original-properties':('image/item/origin archive','ay','optional exact imported property bytes','absent=no original property sequence retained'),
      'original-extension':('item archive','ay','optional exact imported custom extension bytes','absent=no original extension retained'),
      'opaque-record-sequences':('image/item/origin archive','aay','ordered distinct unknown property byte sequences','absent=empty unknown-record history'),
      'original-argument-model':('FilterLayer archive','v','first replaced imported argument value, any original type','absent=no replaced present original model; current model is authoritative'),
      'original-argument-procedure':('FilterLayer archive','v','first replaced imported procedure value, any original type','absent=no replaced present original procedure; current procedure is authoritative'),
      'external-reference-origins':('item archive','aa{sv}','deduplicated diagnostic origin dictionaries; seven fixed keys plus opaque future members','absent=empty diagnostic provenance; never opens/resolves another image')}
    exact(set(puts),set(rules),'native capsule writer keys')
    result=[]
    for key,expressions in sorted(puts.items()):
        owner,typ,default,absence=rules[key]
        # Validate the type from writer construction, including the optional
        # byte-string and raw/opaque exceptional paths.
        joined=' '.join(v for v,_ in expressions)
        factory={'u':'g_variant_new_uint32','t':'g_variant_new_uint64','b':'g_variant_new_boolean','s':'g_variant_new_string'}
        if typ in factory: assert factory[typ] in joined,(key,joined)
        if typ=='ay': assert 'byte_array' in joined or 'nullable' in joined,(key,joined)
        ev=[]; producer_anchors=[]
        for _,expr in expressions:
            candidates=['item_record','preserve_records','image_record','fallback_origin','merge_external_origins']
            anchor=next(fn for fn in candidates if norm(expr) in norm(function(text,fn)))
            ev.append(proof(root,PRESERVE,anchor,expr))
            producer_anchors.append(PRESERVE+'#'+anchor)
        required={
          'generation':('filter_cache_state','t','&state.generation'),
          'cache-generation':('filter_cache_state','t','&state.cache_generation'),
          'cache-complete':('filter_cache_state','b','&state.cache_complete'),
          'saved-state':('filter_cache_state','u','&saved_state'),
          'has-definition':('validate_filter_record','b','&has_definition'),
          'has-arguments':('validate_filter_record','b','&has_arguments'),
          'source-id':('validate_clone_record','u','&id'),
          'source-state':('validate_clone_record','u','&state'),
          'source-expired':('validate_clone_record','b','&expired'),
          'allow-name-lookup':('validate_clone_record','b','&allow_lookup')}
        if key in required:
            check,format_,destination=required[key]
            ev.append(proof(root,PRESERVE,check,f'!g_variant_lookup (dict, "{key}", "{format_}", {destination})'))
            ev.append(proof(root,PRESERVE,check,'fail ("Invalid'))
        if key=='version': ev.append(proof(root,PRESERVE,'decode','if (!g_variant_lookup (value.get (), "version", "u", &version) || version != 1) return {};'))
        if key=='source-id': ev.append(proof(root,PRESERVE,'item_record','const guint32 source_id = ref->source && !external ? GPOINTER_TO_UINT (g_hash_table_lookup (ids, ref->source)) : 0;'))
        if key in ('pending-name','source-name','original-source-name','procedure'):
            ev += [proof(root,PRESERVE,'validate_optional_name','if (!name) return;'),
                   proof(root,PRESERVE,'validate_optional_name','if (!size || data[size - 1] || std::memchr (data, 0, size - 1))')]
        if key=='definition':
            ev += [proof(root,PRESERVE,'validate_filter_record','if (has_definition)'),proof(root,PRESERVE,'validate_filter_record','if (!definition) fail ("Invalid Filter definition bytes; original capsule remains inert");')]
        if key=='arguments':
            ev += [proof(root,PRESERVE,'item_record','if (value) dict.put ("arguments", value.get ()); else g_variant_dict_remove (&dict.dict, "arguments");')]
        if key in ('generation','cache-generation'):
            ev.append(proof(root,PRESERVE,'filter_cache_state','state.generation > G_MAXINT64 || state.cache_generation > state.generation'))
        if key=='kind': ev.append(proof(root,PRESERVE,'known_item_kind','g_variant_lookup (value, "kind", "&s", &kind)'))
        reader='validate_filter_record' if owner=='FilterLayer' else 'validate_clone_record' if owner=='CloneLayer' else 'decode'
        route='02' if 'FilterLayer' in owner else '03' if 'CloneLayer' in owner else '04' if key=='legacy-mode' else '05'
        result.append(row('capsule.'+key,route,owner,'gimp-painter-v1/'+key,'GVariant '+typ,'no legacy capsule; custom source records archived separately',default,absence,
                          [PRESERVE+'#'+reader],sorted(set(producer_anchors)),ev,
                          version='GPXCF logical capsule v1; canonical little-endian a{sv}',flags='GIMP_PARASITE_PERSISTENT; namespace/owner validation; optional keys omitted explicitly',
                          writer_expressions=[x[0] for x in expressions],disposition='dynamic-schema' if typ.startswith('a(') or typ in ('v','aa{sv}','aay') or 'opaque' in typ else 'saved-field'))
    for name,typ,default,expr in [
      ('magic','12 bytes','GPXCF\\0\\0\\0 + uint32le1',"const guint8 magic[12] = {'G','P','X','C','F',0,0,0, 1,0,0,0};"),
      ('unknown-key[*]','GVariant key:string/value:variant','no unknown fields unless imported','g_variant_dict_init (&dict, base)'),
      ('dictionary-key','unique UTF8 GVariant string','required per entry; duplicates keep whole namespace inert','if (!keys.emplace (g_variant_get_string (key.get (), nullptr)).second)')]:
        result.append(row('capsule.'+name,'05','image/item/origin capsule',name,typ,'not in legacy format',default,
                          'invalid framing retains opaque namespace; no synthetic semantic defaults',[PRESERVE+'#decode'],[PRESERVE+'#encode'],[proof(root,PRESERVE,'magic' if name=='magic' else 'decode' if name=='dictionary-key' else 'Dictionary',expr)],
                          version='GPXCF v1',flags='persistent namespace',disposition='dynamic-schema' if '*' in name else 'saved-field'))
    return result

def transport_fields(root):
    specs=[
      ('magic','12 bytes','GPXCF v2 manifest / GPXCC v1 chunk','std::vector<guint8> bytes (magic, magic + 12);'),
      ('owner.type','u32le','image|layer|channel|layer_mask|path','word (bytes, guint32 (manifest.owner.type), 4)'),
      ('owner.id','u32le','image=0; other owner has nonzero file-local ID','word (bytes, manifest.owner.id, 4)'),
      ('namespace','u32le','image=0,item=1,origin=2; invalid owner/namespace pair rejected','word (bytes, guint32 (manifest.space), 4)'),
      ('block-size','u32le','65536','word (bytes, multipart_block_size, 4)'),
      ('logical-size','u64le','required capsule bytes,12..65536*1048576 inclusive','word (bytes, manifest.size, 8)'),
      ('chunk-count','u32le','ceil(logical-size/65536)','word (bytes, manifest.count, 4)'),
      ('reserved','u32le','0 required','word (bytes, 0, 4)'),
      ('sha256','32 bytes','SHA256 of complete logical capsule','bytes.insert (bytes.end (), manifest.digest.begin (), manifest.digest.end ())'),
      ('chunk.index','u32le','required index in[0,count)','word (bytes, index, 4)'),
      ('chunk.reserved','u32le','0 required','word (bytes, 0, 4)'),
      ('chunk.payload','byte[count]','min(65536,logical-size-index*65536) exact capsule slice','bytes.insert (bytes.end (), data, data + count)'),
      ('chunk.name','XCF string','gimp-painter-chunk-v1/{namespace}/{SHA256hex}/{index:08x}','g_strdup_printf ("%u/%s/%08x", guint32 (manifest.space), hex (manifest.digest).c_str (), index)'),
      ('inert.magic','12 bytes','GPXCI v1',"const guint8 inert_magic[12] = {'G','P','X','C','I',0,0,0,1,0,0,0};"),
      ('inert.digest','32 bytes','SHA256 of exact original inner parasite record','data.insert (data.end (), identity.begin (), identity.end ())'),
      ('inert.name','XCF string','gimp-painter-inert-v1/{original-record-SHA256hex}','frame ((std::string (inert_prefix) + hex (identity)).c_str (), data)')]
    result=[]
    proof(root,'app/xcf/painter-xcf-multipart.hpp','multipart_block_size','constexpr guint32 multipart_block_size = 65536;')
    proof(root,'app/xcf/painter-xcf-multipart.hpp','multipart_max_chunks','constexpr guint32 multipart_max_chunks = 1048576;')
    for name,typ,value,expr in specs:
        anchor='chunk_name' if name=='chunk.name' else 'multipart_chunk_record' if name.startswith('chunk.') else 'inert_magic' if name=='inert.magic' else 'multipart_inert_markers' if name.startswith('inert.') else 'header'
        result.append(row('transport.'+name,'05','MultipartManifest/owner-local parasite set',name,typ,'no legacy transport',value,
                          'transport absent means inline namespace; required field absent/malformed means inert exact records, not a default manifest',
                          [MULTI+'#multipart_restore'],[MULTI+'#multipart_manifest_record',MULTI+'#multipart_chunk_record',MULTI+'#multipart_inert_markers'],
                          [proof(root,MULTI,anchor,expr),
                           proof(root,MULTI,'valid_identity','if (guint32 (owner.type) > guint32 (OwnerClass::path) || guint32 (space) > 2) return false;'),
                           proof(root,MULTI,'valid_identity','if (owner.type == OwnerClass::image) return !owner.id && space != Namespace::item;'),
                           proof(root,MULTI,'parse_header','result.count == (result.size + multipart_block_size - 1) / multipart_block_size')],
                          version='manifest2/chunk1/inert1 over logical capsule1',flags='persistent=1 exact; owner-specific transport validation',
                          disposition='dynamic-schema' if name=='chunk.payload' else 'saved-field'))
    return result

def argument_fields(root):
    result=[]
    def add(name,typ,default,reader,writer,expr,legacy='legacy tags are separately preserved; no corresponding native tuple'):
        result.append(row('arguments.'+name,'02','GimpFilterArgumentsSnapshot',name,typ,legacy,default,default,[ARGS+'#'+reader],[ARGS+'#'+writer],
                          [proof(root,ARGS,writer,expr)],version='Painter capsule1 typed argument model',flags='has-arguments plus validated type/depth/work schema',disposition='dynamic-schema'))
    for name,typ,expr in [('type-name','GType name UTF8 string','g_type_name (type)'),('is-null','Boolean','gimp_filter_arguments_snapshot_is_null (args, i)'),('payload','typed GVariant','g_variant_builder_add (&b.value, "(sbv)", g_type_name (type), is_null, owned_payload.get ())')]:
        add('slot[*].'+name,typ,'required tuple member; empty array means zero slots; absent model distinct from empty', 'decode_snapshot_inner','encode_snapshot_inner',expr)
    for name,typ in [('kind','u32'),('file-local-id','u32'),('diagnostic-runtime-id','i64'),('declared-type','GType name string'),('was-set','Boolean'),('expired','Boolean')]:
        add('reference[*].'+name,typ,'required tuple member; kind0 unset,1 self image,2 same-image item,3 expired/external; unresolved IDs never guessed',
            'decode_snapshot_inner','encode_reference','g_variant_new ("(uuxsbb)", kind, tattoo, ref.id, g_type_name (ref.object_type), ref.was_set, ref.expired)')
    body=function(source(str(root),ARGS),'encode_value')
    types=sorted(set(re.findall(r'\bG_(?:TYPE_[A-Z0-9_]+|VALUE_HOLDS_ENUM|VALUE_HOLDS_FLAGS)\b|\bGIMP_TYPE_(?:CORE_OBJECT_ARRAY|VALUE_ARRAY|ARRAY|INT32_ARRAY|DOUBLE_ARRAY)\b',body)))
    rules={
      'G_TYPE_BOOLEAN':('b','g_variant_new_boolean'),
      'G_TYPE_CHAR':('x','g_variant_new_int64'),'G_TYPE_INT':('x','g_variant_new_int64'),'G_TYPE_LONG':('x','g_variant_new_int64'),'G_TYPE_INT64':('x','g_variant_new_int64'),
      'G_TYPE_UCHAR':('t','g_variant_new_uint64'),'G_TYPE_UINT':('t','g_variant_new_uint64'),'G_TYPE_ULONG':('t','g_variant_new_uint64'),'G_TYPE_UINT64':('t','g_variant_new_uint64'),
      'G_VALUE_HOLDS_ENUM':('x','g_variant_new_int64'),'G_VALUE_HOLDS_FLAGS':('t','g_variant_new_uint64'),
      'G_TYPE_FLOAT':('u exact IEEE32 bits','g_variant_new_uint32'),'G_TYPE_DOUBLE':('t exact IEEE64 bits','g_variant_new_uint64'),
      'G_TYPE_VARIANT':('(bv) present + typed child','g_variant_new ("(bv)"'),
      'G_TYPE_STRING':('(bay) present + bytes including NUL','string_bytes'),
      'GIMP_TYPE_CORE_OBJECT_ARRAY':('snapshot:a(uuxsbb); compatibility:(ba(uu))','a(uu)'),
      'GIMP_TYPE_VALUE_ARRAY':('snapshot:a(sbv); compatibility:(ba(sv))','(b@a(sv))'),
      'G_TYPE_STRV':('(ba(bay)) present + nullable byte strings','a(bay)'),
      'G_TYPE_BYTES':('(bay) present + raw bytes','GimpPainterXcf::byte_variant'),
      'GIMP_TYPE_ARRAY':('(bay) present + bytes','array_bytes'),
      'GIMP_TYPE_INT32_ARRAY':('(bay) present + canonical little-endian4-byte components','array_bytes'),
      'GIMP_TYPE_DOUBLE_ARRAY':('(bay) present + canonical little-endian8-byte IEEE components','array_bytes')}
    exact(set(types),set(rules),'argument scalar/array type family')
    for typ,(wire,expr) in rules.items():
        add('payload['+typ+']',wire,'required payload for selected declared type; outer a(sbv) slot is-null carries snapshot nullness'+('; inner wrapper presence bit FALSE preserves null distinctly from empty' if wire.startswith('(b') else '; this scalar payload has no inner presence flag'), 'decode_value','encode_value',expr)
    # Finite members inside nullable wrappers; opaque variant contents remain
    # parameterized rather than pretending every future key is known.
    for name,typ,expr in [('nullable.present','b','"(bv)"'),('nullable.bytes','ay','"(b@ay)"'),('nullable.variant','v','"(bv)"'),
                          ('nested.slots','a(sbv)','Builder b ("a(sbv)")'),('compatibility.type-name','s','"(sv)"'),('compatibility.payload','v','"(sv)"')]:
        add(name,typ,'required wrapper member; explicit presence bit governs null; nested depth<=32 and aggregate slots/references<=65536',
            'decode_snapshot_inner' if name.startswith('nested') else 'ArgumentEntry' if name.startswith('compatibility') else 'decode_value','encode_snapshot_inner' if name.startswith('nested') else 'encode_value' if name.startswith('nullable') else 'encode',expr)
    origin=function(source(str(root),ARGS),'origin_record')
    found={}
    for _,args,expr,_ in calls(origin,'g_variant_builder_add'):
        if len(args)>=4 and args[2].startswith('"'): found[args[2].strip('"')]=(args[3],expr)
    defaults={'role':'clone-source|filter-argument','declared-type':'declared GType name','object-type':'actual object GType name',
              'object-runtime-id':'diagnostic runtime ID only','image-runtime-id':'diagnostic owner image runtime ID;0 if absent',
              'object-name':'nullable already-visible object name; image uses safe basename only','image-name':'nullable safe visible basename; omitted for nonlocal file handles or path/URI/userinfo spelling'}
    exact(set(found),set(defaults),'external origin member domain')
    for key,(expression,expr) in found.items():
        typ='(bay)' if 'string_bytes' in expression else 'x' if 'int64' in expression else 's'
        add('external-origin[*].'+key,typ,defaults[key]+'; no origin entry when no external identity; individual key values remain diagnostic and uninterpreted', 'decode_snapshot_inner','origin_record',expr)
        result[-1]['owner']='diagnostic external-origin dictionary; not active argument reference authority'
        result[-1]['reader']=[PRESERVE+'#merge_external_origins']
        result[-1]['evidence'].append(proof(root,PRESERVE,'merge_external_origins','external_field'))
    return result

def legacy_extension_fields(root):
    result=[]
    legacy=source(str(root),LOAD,'legacy'); writer=source(str(root),SAVE,'legacy')
    for kind,fn,wfn,tag in [('filter','xcf_load_filter_specs','xcf_save_filter_spec_value',32),('clone','xcf_load_clone_specs','xcf_save_prop',33)]:
        body=function(legacy,fn)
        source_name='filter_name' if kind=='filter' else 'target_name'
        prop='PROP_FILTER_SPEC' if kind=='filter' else 'PROP_CLONE_SPEC'
        name_expr=next(expr for _,a,expr,_ in calls(function(legacy,'xcf_load_layer_props'),'xcf_read_string') if a[1]=='&'+source_name)
        result.append(row('legacy.'+kind+'.name','02' if kind=='filter' else '03','legacy '+kind+' layer',prop+'/name',
                          'WireString exact byte range; safe imported nullable native string','u32 BE byte length plus bytes including NUL',
                          'required name slot; zero-length XCF string yields NULL; no procedure/source inferred',
                          'whole property absent leaves ordinary layer; truncated property remains retained opaque evidence',
                          ['app/xcf/painter-xcf-compat.cpp#extension'],[PRESERVE+'#item_record'],
                          [proof(root,LOAD,'xcf_load_layer_props',name_expr,'legacy'),proof(root,'app/xcf/painter-xcf-compat.cpp','extension','result.name = c.string ();')],
                          version='legacy property'+str(tag)+'; native capsule1 archive',legacy_reader=[LOAD+'#xcf_load_layer_props'],legacy_writer=[SAVE+'#xcf_save_prop']))
        for i,t in enumerate(transfers(body)):
            name=f'{kind}.primitive{i}.{t["destination"]}'
            result.append(row('legacy.'+re.sub(r'[^a-zA-Z0-9_.-]','_',name),'02' if kind=='filter' else '03',
                              'legacy '+kind+' record',name,t['type']+' count='+t['count'],t['type']+' count='+t['count'],
                              'required within selected old tag branch; unavailable omitted values remain opaque, never fabricated',
                              'no custom property leaves ordinary layer; within record truncation/unsupported data retains raw source',
                              ['app/xcf/painter-xcf-compat.cpp#decode_'+kind], [PRESERVE+'#item_record'],
                              [proof(root,LOAD,fn,t['expression'],'legacy')],version='legacy Painter property'+str(tag)+'; native archive in capsule1',
                              legacy_reader=[LOAD+'#'+fn],legacy_writer=[SAVE+'#'+wfn],read=t,disposition='dynamic-schema'))
    # Tagged slot family has real asymmetric old readers/writers.  Keep exact
    # source-derived read and write branches instead of normalizing tag quirks.
    oldbody=function(legacy,'xcf_load_filter_specs'); writebody=function(writer,'xcf_save_filter_spec_value')
    tags=sorted(set(re.findall(r'case\s+(FILTER_[A-Z0-9_]+)\s*:',oldbody)))
    if not tags: tags=sorted(set(re.findall(r'case\s+([A-Z][A-Z0-9_]+)\s*:',oldbody)))
    types={
      'END':('0','terminator','tag0 size0; no value','required terminator; no argument slot'),
      'UNSUPPORTED':('1','unsupported','tag1 size0; original value omitted','no value can be recovered'),
      'INT32':('2','int32_bits','tag2 size4 + u32 bits -> legacy G_TYPE_INT','required four payload bytes; no implicit zero'),
      'INT16':('3','int16_writer_32_bits','tag3 declared size2 but four u32 payload bytes; legacy G_TYPE_UINT then invalid signed setter','retain actual32 bits, do not emulate invalid legacy setter'),
      'INT8':('4','int8_bits','tag4 declared size1 + byte; legacy G_TYPE_UCHAR then invalid signed setter','retain byte bits, do not emulate invalid legacy setter'),
      'FLOAT':('5','float32_bits','tag5 size4 IEEE32 converted from/to legacy double','original double precision unavailable'),
      'INT8ARRAY':('6','omitted_array','tag6 size0; no elements written','cannot recover omitted array elements'),
      'FLOATARRAY':('7','omitted_array','tag7 size0 omitted array OR old writer string length+bytes','zero-size null string vs array ambiguous; nonempty string only under explicit writer-recovery policy'),
      'STRING':('8','string','tag8 size word doubles as XCF string length; old writer instead emits tag7','reader-compatible byte string; no new string assumed'),
      'RGB':('9','omitted_rgb','tag9 has no payload in reader; old writer has no RGB-specific branch','cannot recover omitted color'),
      'DRAWABLE':('10','omitted_drawable','tag10 size0; drawable ID omitted','cannot recover omitted object reference')}
    exact({t.removeprefix('XCF_FILTER_') for t in tags},set(types),'legacy filter tag family')
    for tag in tags:
        wire,kind,legacy_type,absence=types[tag.removeprefix('XCF_FILTER_')]
        native=source(str(root),'app/xcf/painter-xcf-compat.cpp')
        assert 'ArgumentKind::'+kind in function(native,'extension')
        result.append(row('legacy.filter-tag.'+tag,'02','legacy FilterLayer argument slot',tag,
                          'ArgumentKind::'+kind+'; original encoded/payload Range retained',legacy_type,
                          absence,absence,
                          ['app/xcf/painter-xcf-compat.cpp#decode_filter'],[PRESERVE+'#item_record'],
                          [proof(root,LOAD,'xcf_load_filter_specs','case '+tag+':','legacy'),proof(root,'app/xcf/painter-xcf-compat.cpp','extension','ArgumentKind::'+kind)],version='legacy Painter property32 nested tag'+wire+' schema',
                          legacy_reader=[LOAD+'#xcf_load_filter_specs'],legacy_writer=[SAVE+'#xcf_save_filter_spec_value'],disposition='dynamic-schema'))
    return result

def expected_fields(root):
    return property_fields(root)+config_fields(root)+structured_fields(root)+capsule_fields(root)+transport_fields(root)+argument_fields(root)+legacy_extension_fields(root)+extra_native_fields(root)+effect_config_fields(root)

OWNED_CANDIDATE_FILES={LOAD,SAVE,'app/xcf/xcf.c','app/core/gimpimage.c','app/core/gimpimage.h',
                       'app/core/gimpitem.c','app/core/gimpitem.h','libgimpconfig/gimpconfig-params.h'}

def expected_candidates(root, fields=None):
    fields=fields or expected_fields(root); result=[]; cache={}
    census=list(csv.DictReader((Path(root)/'migration/inventory/persistence-sites.tsv').open(),delimiter='\t'))
    parasite=[r['id'] for r in fields if r['id'].startswith('xcf.record.parasite.')]
    for c in census:
        path,line=c['legacy_site'].rsplit(':',1); line=int(line)
        if path not in OWNED_CANDIDATE_FILES: continue
        if path not in cache:
            text=source(str(root),path,'legacy'); cache[path]=(text,function_ranges(text))
        text,ranges=cache[path]
        exact(text.splitlines()[line-1].strip(),c['source_line'],c['legacy_site']+' pinned source line')
        matches=[x for x in ranges if x[0]<=line<=x[1]]
        fn=matches[0][2] if matches else None
        record=dict(legacy_site=c['legacy_site'],route='none',disposition='non-persisted',fields=[],reason='')
        if path.endswith('.h') or fn is None or (matches and line==matches[0][0]):
            record['reason']='Declaration/macro definition/function signature; no concrete owner instance or serialized field at this syntax site'
        elif path in ('app/core/gimpimage.c','app/core/gimpitem.c'):
            record.update(route='05',disposition='dynamic-schema',fields=parasite,
                          reason=fn+' accesses the in-memory named parasite schema; XCF persistence is gated separately by GIMP_PARASITE_PERSISTENT')
        elif path=='app/xcf/xcf.c':
            if fn in ('xcf_load_invoker','xcf_save_invoker','xcf_load_stream','xcf_save_stream'):
                record['reason']=fn+' is a whole-document entry/dispatch call; it declares no additional saved member'
            else:
                record.update(route='01',disposition='dynamic-schema',fields=[r['id'] for r in fields if r['id'].startswith('xcf.record.image-header.')],
                              reason=fn+' routes the required XCF document header; member schema is separately enumerated')
        else:
            body='\n'.join(text.splitlines()[matches[0][0]-1:line])
            cases=re.findall(r'case\s+(PROP_[A-Z0-9_]+)\s*:',body)
            prop=cases[-1] if cases and fn.endswith('_props') or cases and fn=='xcf_save_prop' else None
            if 'filter_spec' in fn or prop=='PROP_FILTER_SPEC': route='02'
            elif 'clone_spec' in fn or prop=='PROP_CLONE_SPEC': route='03'
            elif prop=='PROP_MODE': route='04'
            elif 'parasite' in fn or prop=='PROP_PARASITES': route='05'
            else: route='01'
            if prop in ('PROP_FILTER_SPEC','PROP_CLONE_SPEC') or route in ('02','03'):
                ids=[r['id'] for r in fields if r['route']==route and r['id'].startswith('xcf.legacy.')]
            elif prop:
                native_prop='PROP_OLD_SAMPLE_POINTS' if prop=='PROP_SAMPLE_POINTS' else prop
                ids=[r['id'] for r in fields if r['route']==route and (r['saved_name']==native_prop or r['saved_name'].startswith(native_prop+'/'))]
            else:
                anchor=path+'#'+fn
                ids=[r['id'] for r in fields if r['route']==route and anchor in r['legacy_reader']+r['legacy_writer']]
            if not ids and route=='05': ids=parasite
            if not ids:
                # Tile codecs and offset/string helpers implement the explicit
                # parameterized byte/framing schemas; they are not new keys.
                ids=[r['id'] for r in fields if r['route']==route and (r['id'].startswith('xcf.pixels.') if 'tile' in fn or 'level' in fn or 'hierarchy' in fn else r['id'].startswith('xcf.framing.'))]
            if not ids: raise AssertionError('unmapped syntax site '+c['legacy_site']+' '+str(fn)+' '+str(prop))
            record.update(route=route,disposition='dynamic-schema',fields=sorted(ids),
                          reason=fn+(' '+prop if prop else '')+' is a decoder/writer/framing use of the enumerated members; syntax count is not field count')
        result.append(record)
    return result

def validate_family(data, root=ROOT):
    source.cache_clear()
    fields=data['fields']; expected=expected_fields(root)
    actual={x['id']:x for x in fields}; wanted={x['id']:x for x in expected}
    exact(len(actual),len(fields),'duplicate XCF field IDs')
    exact(set(actual),set(wanted),'XCF field population (source-discovered)')
    for key, value in wanted.items():
        for member in value:
            exact(actual[key].get(member),value[member],key+'/'+member)
    exact(data.get('candidates'),expected_candidates(root,expected),'XCF/core syntax dispositions')
    return [r['assertion'] for r in expected]

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--contract',type=Path,default=ROOT/'migration/inventory/persistence-xcf-fields.json')
    args=parser.parse_args(); data=json.loads(args.contract.read_text())
    assertions=validate_family(data)
    print(f'PASS: {len(assertions)} exact XCF metadata/schema assertions; no runtime equivalence claim')

if __name__=='__main__': main()
