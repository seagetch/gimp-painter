#!/bin/bash
set -eu
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
cd /workspace/shared/gimp-native-baseline
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
for file in app/display/{gimpimagewindow.c,gimppaintercanvasui.h} app/widgets/{gimpaction.c,gimpaction.h,gimpactiongroup.c,gimpactionimpl.c,gimptoggleaction.c,gimpradioaction.c,gimpuimanager.c,gimpmenu.c} app/tests/{test-painter-toolbar-window.c,test-painter-toolbar-transfer.inc}; do
 cp "$repo/$file" "source/$file"
done
cp "$repo/migration/inventory/additional-c-header-routes.json" source/migration/inventory/additional-c-header-routes.json
meson compile -C build-port painter-toolbar-window painter-public-headers-c painter-public-headers-cpp >toolbar-transfer-evidence/green-build.log 2>&1
for name in transfer-radio-command-supersession transfer-global-actions transfer-radio-notification transfer-action-publication; do
 GIMP_TEST_TOOLBAR_TRANSFER_SHOTS=/workspace/shared/gimp-native-baseline/toolbar-transfer-evidence/portrait python3 source/tools/run-painter-xvfb.py meson test -C build-port painter-toolbar-window --no-rebuild --print-errorlogs --logbase "toolbar-green-$name" --test-args="-p /painter-toolbar-window/$name" >"toolbar-transfer-evidence/green-$name.log" 2>&1
done
python3 source/tools/run-painter-xvfb.py meson test -C build-port painter-toolbar-window --no-rebuild --print-errorlogs --logbase toolbar-green-full >toolbar-transfer-evidence/green-full.log 2>&1
python3 - <<'PY'
import pathlib,hashlib,json
b=pathlib.Path('/workspace/shared/gimp-native-baseline'); r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter')
files=['migration/inventory/additional-c-header-routes.json','app/display/gimpimagewindow.c','app/display/gimppaintercanvasui.h']+['app/widgets/'+n for n in ['gimpaction.c','gimpaction.h','gimpactiongroup.c','gimpactionimpl.c','gimptoggleaction.c','gimpradioaction.c','gimpuimanager.c','gimpmenu.c']]+['app/tests/test-painter-toolbar-window.c','app/tests/test-painter-toolbar-transfer.inc']
receipt={}
for f in files:
 a=(r/f).read_bytes(); c=(b/'source'/f).read_bytes(); assert a==c,f
 receipt[f]=hashlib.sha256(a).hexdigest()
receipt['binary_sha256']=hashlib.sha256((b/'build-port/app/tests/painter-toolbar-window').read_bytes()).hexdigest()
(b/'toolbar-transfer-evidence/green-source-binary.json').write_text(json.dumps(receipt,indent=2)+'\n')
PY
