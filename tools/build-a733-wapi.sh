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
  vendor/allwinnertech/chips/a733/a733_wifi_usb.c
  vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh/defconfig
  apps/system/a733wifi/a733wifi_main.c
  apps/system/a733wifi/Kconfig
)
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
  for path in "${files[@]}"; do
    if [[ -f "$backup/$path" ]]; then
      cp -f "$backup/$path" "$official/$path"
    else
      rm -f "$official/$path"
    fi
  done

  rm -rf "$aipet_official"
  if [[ -d "$backup/$aipet_relative" ]]; then
    cp -a "$backup/$aipet_relative" "$aipet_official"
  fi

  rm -rf "$backup"
}

trap restore_official EXIT
for path in "${files[@]}"; do
  mkdir -p "$backup/$(dirname "$path")"
  if [[ -f "$official/$path" ]]; then
    cp "$official/$path" "$backup/$path"
  fi

  mkdir -p "$official/$(dirname "$path")"
  cp -f "$overlay/$path" "$official/$path"
done

if [[ -d "$aipet_official" ]]; then
  mkdir -p "$backup/$(dirname "$aipet_relative")"
  cp -a "$aipet_official" "$backup/$aipet_relative"
fi

rm -rf "$aipet_official"
cp -a "$aipet_overlay" "$aipet_official"

cd "$official"
export PATH="$official/prebuilts/build-tools/linux-x86_64/bin:$official/prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin:$official/prebuilts/tools/linux-x86_64:$official/prebuilts/tools/cmake/bin:$official/prebuilts/tools/ninja:/usr/bin:/bin:${PATH:-}"

if [[ "${1:-}" == "--incremental" ]]; then
  cmake --build "$build" -j"${JOBS:-8}"
else
  ./nuttx/tools/build.sh \
    vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh \
    --cmake -b "$build" -j"${JOBS:-8}"
fi

grep -E 'CONFIG_(NETDEV_WIRELESS_IOCTL|WIRELESS_WAPI|WIRELESS_WAPI_CMDTOOL)' \
  "$build/.config"
sha256sum "$build/nuttx.bin"
