#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
team_dir="$(cd "$script_dir/.." && pwd)"
workspace="$(cd "$team_dir/.." && pwd)"
build_dir="$workspace/cmake_out/cubie-a7z_nsh"

export PATH="$workspace/prebuilts/build-tools/linux-x86_64/bin:$workspace/prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin:$workspace/prebuilts/tools/linux-x86_64:$workspace/prebuilts/tools/cmake/bin:$workspace/prebuilts/tools/ninja:/usr/bin:/bin:${PATH:-}"

if [[ "${1:-}" == "--incremental" ]]; then
  cmake --build "$build_dir" -j"${JOBS:-8}"
else
  "$workspace/nuttx/tools/build.sh" \
    vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh \
    --cmake \
    -b "$build_dir" \
    -j"${JOBS:-8}"
fi

sha256sum "$build_dir/nuttx.bin"
