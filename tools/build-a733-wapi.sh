#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
team_dir="$(cd "$script_dir/.." && pwd)"
workspace_root="$(cd "$team_dir/../.." && pwd)"
official="$workspace_root/quickly-openvela"
overlay="$team_dir/board/a733-cubie-a7z/openvela-overlay"
build="$official/cmake_out/cubie-a7z_nsh_v67_openvela_wapi"
backup="$(mktemp -d)"

driver=vendor/allwinnertech/chips/a733/a733_wifi_usb.c
config=vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh/defconfig
wifi_app=apps/system/a733wifi/a733wifi_main.c
wifi_kconfig=apps/system/a733wifi/Kconfig

restore_official()
{
  cp -f "$backup/a733_wifi_usb.c" "$official/$driver"
  cp -f "$backup/defconfig" "$official/$config"
  cp -f "$backup/a733wifi_main.c" "$official/$wifi_app"
  cp -f "$backup/a733wifi_Kconfig" "$official/$wifi_kconfig"
  rm -rf "$backup"
}

trap restore_official EXIT
cp "$official/$driver" "$backup/a733_wifi_usb.c"
cp "$official/$config" "$backup/defconfig"
cp "$official/$wifi_app" "$backup/a733wifi_main.c"
cp "$official/$wifi_kconfig" "$backup/a733wifi_Kconfig"
cp -f "$overlay/$driver" "$official/$driver"
cp -f "$overlay/$config" "$official/$config"
cp -f "$overlay/$wifi_app" "$official/$wifi_app"
cp -f "$overlay/$wifi_kconfig" "$official/$wifi_kconfig"

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
