# openvela on Allwinner A733 / Radxa Cubie A7Z

This is the first-stage, SD-card-first ARM64 port.  It intentionally reuses
the official Boot0, SCP, BL31 and U-Boot firmware from the known-good RadxaOS
image.  UFS is not probed by openvela and is reserved for a later board
configuration.

## Hardware contract

- DRAM: 4 GiB LPDDR4 at physical `0x40000000`
- openvela Image load/entry: `0x40200000`
- stage-1 RAM window: `0x40200000..0x47ffffff`
- BL31 reserved: `0x48000000..0x48ffffff`
- console: UART0, `0x02500000`, GIC IRQ 34, 115200 8N1
- interrupt controller: GICv3, GICD `0x03400000`, GICR `0x03460000`
- boot storage: SDMMC0, `0x04020000` (driver follows after the NSH baseline)

The produced `nuttx.bin` starts with a Linux ARM64 Image header.  This is
required because the board's U-Boot is AArch32 and must use `booti`/BL31 to
enter AArch64; `go 0x40200000` is not a valid boot method.

## Build in WSL

From the `quickly-openvela` directory:

```sh
export PATH="$PWD/prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin:\
$PWD/prebuilts/tools/linux/x86_64:\
$PWD/prebuilts/tools/cmake/bin:\
$PWD/prebuilts/tools/ninja:\
/usr/bin:/bin:$PATH"

./nuttx/tools/build.sh \
  vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh \
  --cmake -b cmake_out/cubie-a7z_nsh -j8
```

The SD boot file is (the verified local build used the `_py` directory while
working around a stale failed CMake directory):

```text
cmake_out/cubie-a7z_nsh_py/nuttx.bin
```

Copy it to `/boot/openvela/a733/Image` on the RadxaOS root filesystem and add
the entry from `tools/extlinux-openvela.conf` to the existing
`/boot/extlinux/extlinux.conf`.  Keep the Debian entry as the default recovery
path until the openvela NSH baseline has passed.

The expected first openvela-specific serial marker is:

```text
A7Z0
```

It is followed by the NuttShell banner when GICv3, the virtual timer, MMU,
heap and UART interrupt path are all functioning.
