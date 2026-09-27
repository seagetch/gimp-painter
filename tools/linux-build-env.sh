#!/usr/bin/env bash
# Source this file before configuring, compiling, or launching the local build.
linux_build_repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
linux_build_prefix="$(dirname "$linux_build_repo_dir")/.build-prefix"
export PKG_CONFIG_PATH="$linux_build_prefix/lib/x86_64-linux-gnu/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$linux_build_prefix/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export GI_TYPELIB_PATH="$linux_build_prefix/lib/x86_64-linux-gnu/girepository-1.0${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"
# Plug-in shebangs must resolve system Python, which owns python3-gi here.
export PATH="$linux_build_prefix/bin:/usr/bin:$PATH"
unset linux_build_repo_dir linux_build_prefix
