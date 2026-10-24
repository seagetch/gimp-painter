import pathlib,json,subprocess,os,hashlib
O=pathlib.Path('/workspace/shared/named-editor-entry');B=pathlib.Path('/workspace/shared/gimp-native-preset-gui/build');j=json.loads((O/'build.json').read_text());D=O/'mutants';D.mkdir(exist_ok=True)
cases=[('ignore-factory','app/widgets/gimppaintermybrusheditor.cpp','    if (factory) {\n      gimp_editor_create_menu','    if (false) {\n      gimp_editor_create_menu','constructor'),('save-as-is-save','app/actions/mypaint-brush-editor-actions.c','g_strcmp0 (gimp_action_get_name (action), "mypaint-brush-editor-save-as") == 0','FALSE','menu-save')]
cases += [('cache-not-retired','app/widgets/gimppaintermybrusheditor.cpp','      gimp_menu_factory_delete_manager (','      if (false) gimp_menu_factory_delete_manager (','menu-lifetime'),('retained-action-raw-callback','app/actions/mypaint-brush-editor-actions.c','g_signal_connect_object (gimp_action_group_get_action','g_signal_connect_data (gimp_action_group_get_action','menu-lifetime')]
cases += [('retained-group-data','app/widgets/gimppaintermybrusheditor.cpp','if (group && group->user_data == owner) group->user_data = nullptr;','if (false && group && group->user_data == owner) group->user_data = nullptr;','menu-lifetime')]
results=[]
for name,path,old,new,test in cases:
 source=O/'source'/path; text=source.read_text(); assert old in text
 mutant=source.with_name(name+source.suffix);changed=text.replace(old,new,1)
 if name=='retained-action-raw-callback':changed=changed.replace('G_OBJECT (group->user_data), 0);','G_OBJECT (group->user_data), NULL, 0);')
 mutant.write_text(changed); command=next(c[:] for c in j['compile'] if c[-1]==str(source));original_object=command[command.index('-o')+1]; obj=D/(name+'.o');command[command.index('-o')+1]=str(obj);command[command.index('-MF')+1]=str(obj)+'.d';command[-1]=str(mutant)
 subprocess.run(command,cwd=B,check=True)
 link=[str(obj) if x==original_object else x for x in j['link']];binary=D/name;link[link.index('-o')+1]=str(binary);subprocess.run(link,cwd=B,check=True)
 env=os.environ.copy();env['NAMED_EDITOR_BINARY']=str(binary)
 with (D/(name+'.log')).open('w') as log:r=subprocess.run(['bash',str(O/'run-private.sh'),'-p','/painter-editor/named-entry/'+test],env=env,stdout=log,stderr=subprocess.STDOUT)
 output=(D/(name+'.log')).read_text(); assert r.returncode!=0,(name,output)
 expected={'ignore-factory':'gimp_editor_get_menu_factory','save-as-is-save':"'dialog' should not be nullptr",'cache-not-retired':'g_hash_table_lookup','retained-action-raw-callback':'g_signal_handler_find','retained-group-data':'user_data'}[name]
 assert expected in output,(name,output[-1500:])
 results.append(dict(name=name,compile=command,link=link,returncode=r.returncode,executable_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),source_sha256=hashlib.sha256(mutant.read_bytes()).hexdigest(),expected=expected));binary.unlink()
(O/'mutants.json').write_text(json.dumps(results,indent=2)+'\n')
