# Additional executed legacy capture commands

These captures used the existing pinned, rebuilt reference executable, without
editing the compositor or XCF source. The shared legacy environment and patches
are documented under `migration/baseline/legacy`.

For each script listed below the exact application invocation was:

```sh
source /workspace/shared/gimp-legacy-build/env.sh
export HOME=/workspace/shared/gimp-legacy-build/runtime-home
export GIMP2_DIRECTORY=$HOME/profile
export GEGL_PATH=/workspace/shared/gimp-legacy-build/prefix/lib/gegl-0.3
export XDG_CACHE_HOME=/workspace/shared/gimp-legacy-build/runtime-cache
# TIMEOUT was 180 for seven modes, 120 for Normal/Multiply, 90 for image captures.
timeout "$TIMEOUT" /workspace/shared/gimp-legacy-build/prefix/bin/gimp-2.8 \
  -i -d -f --no-splash --no-shm --no-cpu-accel \
  -b "(load \"$SCRIPT\")" -b '(gimp-quit 0)'
```

The completed script locations (and package retaining each exact script) were:

- `/workspace/shared/legacy-mode-capture/capture.scm`: `legacy-modes`
- `/workspace/shared/legacy-normal-multiply-capture/capture.scm`: `legacy-normal-multiply`
- `/workspace/shared/legacy-ordinary-projection/capture.scm`: `legacy-ordinary-projection`
- `/workspace/shared/legacy-upstream-projection/capture.scm`: `legacy-upstream-projection`

Each package stores stdout/stderr together as capture.log and the actual shell
exit status. All final runs exited 0. The no-CPU-acceleration flag makes the
historical generic arithmetic the reference; accelerated/cross-platform paths
are not claimed as exercised. Existing installed diagnostic logging remains in
the executable and full logs are retained. PNG save used svtrans=1, compression9
and metadata options0. Pillow RGBA/tobytes only decoded the PNG representation.
