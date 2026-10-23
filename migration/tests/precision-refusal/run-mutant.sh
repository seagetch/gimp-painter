set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
python3 <evidence>/native-mutant.py ><evidence>/native-mutation-build.log 2>&1
