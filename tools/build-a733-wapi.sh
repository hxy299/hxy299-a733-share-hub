#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
team_dir="$(cd "$script_dir/.." && pwd)"
workspace_root="$(cd "$team_dir/../.." && pwd)"
official="$workspace_root/quickly-openvela"
overlay="$team_dir/board/a733-cubie-a7z/openvela-overlay"
build="$official/cmake_out/cubie-a7z_nsh_v69_aipet_modelcheck"
backup="$(mktemp -d)"

bash "$script_dir/check-openvela-first.sh"

files=(
  vendor/allwinnertech/boards/a733/cubie-a7z/scripts/dramboot.ld
  nuttx/arch/arm64/src/common/arm64_fatal.c
  vendor/allwinnertech/chips/a733/a733_boot.c
  vendor/allwinnertech/chips/a733/a733_hwdiag.c
  vendor/allwinnertech/chips/a733/a733_wifi_usb.c
  vendor/allwinnertech/chips/a733/CMakeLists.txt
  vendor/allwinnertech/chips/a733/Kconfig
  vendor/allwinnertech/chips/a733/include/chip.h
  vendor/allwinnertech/chips/a733/include/irq.h
  vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh/defconfig
  apps/system/a733wifi/a733wifi_main.c
  apps/system/a733wifi/Kconfig
)
patched_files=(
  nuttx/arch/arm64/include/arch.h
  packages/ai_agent/src/agent_main.c
  packages/ai_agent/CMakeLists.txt
  packages/ai_agent/src/infra/vela_tls.c
  packages/ai_agent/src/tools/tool_registry.c
  packages/ai_agent/src/core/agent_loop.c
  packages/ai_agent/src/core/session_mgr.c
)
staged_files=()
aipet_saved=false
pet_saved=false
aipet_relative=apps/system/aipetllm
aipet_official="$official/$aipet_relative"
aipet_overlay="$overlay/$aipet_relative"

case "$aipet_official" in
  "$official"/apps/system/aipetllm) ;;
  *) echo "Refusing unsafe AI Pet staging path: $aipet_official" >&2; exit 1 ;;
esac

restore_official()
{
  local path
  for path in "${staged_files[@]}"; do
    if [[ -f "$backup/$path" ]]; then
      cp -f "$backup/$path" "$official/$path"
    else
      rm -f "$official/$path"
    fi
  done

  for path in "${patched_files[@]}"; do
    if [[ -f "$backup/$path" ]]; then
      cp -f "$backup/$path" "$official/$path"
    fi
  done

  if [[ "$aipet_saved" == true ]]; then
    rm -rf "$aipet_official"
    if [[ -d "$backup/$aipet_relative" ]]; then
      cp -a "$backup/$aipet_relative" "$aipet_official"
    fi
  fi

  if [[ "$pet_saved" == true ]]; then
    rm -rf "$official/apps/system/aipet"
    if [[ -d "$backup/apps/system/aipet" ]]; then
      cp -a "$backup/apps/system/aipet" "$official/apps/system/aipet"
    fi
  fi

  rm -rf "$backup"
}

trap restore_official EXIT
for path in "${files[@]}"; do
  mkdir -p "$backup/$(dirname "$path")"
  if [[ -f "$official/$path" ]]; then
    cp "$official/$path" "$backup/$path"
  fi

  staged_files+=("$path")
  mkdir -p "$official/$(dirname "$path")"
  cp -f "$overlay/$path" "$official/$path"
done


for path in "${patched_files[@]}"; do
  mkdir -p "$backup/$(dirname "$path")"
  cp "$official/$path" "$backup/$path"
done

patch --forward --batch --no-backup-if-mismatch -p1 -d "$official" \
  < "$team_dir/patches/nuttx-arm64-a733-aff1-cpuid.patch"

if [[ -d "$aipet_official" ]]; then
  mkdir -p "$backup/$(dirname "$aipet_relative")"
  cp -a "$aipet_official" "$backup/$aipet_relative"
fi
aipet_saved=true

rm -rf "$aipet_official"
cp -a "$aipet_overlay" "$aipet_official"

if [[ -d "$official/apps/system/aipet" ]]; then
  cp -a "$official/apps/system/aipet" "$backup/apps/system/aipet"
fi
pet_saved=true
rm -rf "$official/apps/system/aipet"
cp -a "$overlay/apps/system/aipet" "$official/apps/system/aipet"
patch --forward --batch --no-backup-if-mismatch -p1 -d "$official" \
  < "$team_dir/patches/ai-agent-a733-pet-channel.patch"

cd "$official"
export PATH="$official/prebuilts/build-tools/linux-x86_64/bin:$official/prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin:$official/prebuilts/tools/linux-x86_64:$official/prebuilts/tools/cmake/bin:$official/prebuilts/tools/ninja:/usr/bin:/bin:${PATH:-}"

if [[ "${1:-}" == "--incremental" ]]; then
  if [[ "${BUILD_KEEP_GOING:-0}" == 1 ]]; then
    cmake --build "$build" -j"${JOBS:-8}" -- -k 0
  else
    cmake --build "$build" -j"${JOBS:-8}"
  fi
else
  # build.sh preserves an existing CMake cache.  Remove only this board's
  # generated directory so defconfig changes (notably CONFIG_SMP) cannot be
  # silently ignored by a stale .config.
  case "$build" in
    "$official"/cmake_out/cubie-a7z_nsh_v69_aipet_modelcheck)
      rm -rf "$build"
      ;;
    *)
      echo "Refusing unsafe build cleanup path: $build" >&2
      exit 1
      ;;
  esac

  ./nuttx/tools/build.sh \
    vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh \
    --cmake -b "$build" -j"${JOBS:-8}"
fi

grep -E 'CONFIG_(NETDEV_WIRELESS_IOCTL|WIRELESS_WAPI|WIRELESS_WAPI_CMDTOOL)' \
  "$build/.config"

grep -qx 'CONFIG_SMP=y' "$build/.config"
expected_ncpus=$(sed -n 's/^CONFIG_SMP_NCPUS=//p' \
  "$overlay/vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh/defconfig")
[[ "$expected_ncpus" =~ ^[2-8]$ ]]
grep -qx "CONFIG_SMP_NCPUS=$expected_ncpus" "$build/.config"
grep -qx 'CONFIG_ARCH_HAVE_MULTICPU=y' "$build/.config"
sha256sum "$build/nuttx.bin"
