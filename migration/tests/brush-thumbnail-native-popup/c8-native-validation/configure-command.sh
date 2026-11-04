#!/bin/bash
# Executed once against this isolated source/build; no historical build regeneration.
source /workspace/shared/gimp-common-layer-env/environment.sh
export HOME=/tmp/c8-native-validation/home TMPDIR=/tmp/c8-native-validation/tmp
unset DISPLAY
meson setup /tmp/c8-native-validation/build /tmp/c8-native-validation/source --wrap-mode=nofallback -Dauto_features=disabled -Dheadless-tests=disabled -Denable-console-bin=false -Dgi-docgen=disabled -Dappdata-test=disabled -Djavascript=disabled -Dvala=disabled -Dprint=false -Dpainter-http=disabled -Dcheck-update=no -Dlibunwind=false -Dlibbacktrace=false -Dprefix=/tmp/c8-native-validation/prefix -Dbuildtype=debugoptimized
