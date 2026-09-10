# A733 / Radxa Cubie A7Z board port

This directory is the contest-owned source of the A733 board port.
`contest2026_274_Dogking.xml` links its `openvela-overlay` contents into the
normal openvela source tree.

## Hardware contract

- SoC: Allwinner A733, ARM64, 2× Cortex-A76 + 6× Cortex-A55
- DRAM: 4 GiB LPDDR4/4X at physical `0x40000000`
- openvela load/entry: `0x40200000`
- BL31 reserved window: `0x48000000..0x48ffffff`
- console: UART0 at `0x02500000`, GIC IRQ 34, 115200 8N1
- GICv3: distributor `0x03400000`, redistributor `0x03460000`
- boot storage: SDMMC0 at `0x04020000`, 4-bit microSD
- wireless: FCU760K/AIC8800D80-U02 on dedicated USB1
- NPU: Allwinner VIP2, hardware CID `0x1000003b`
- current test board has no UFS device; UFS absence is non-blocking

## Overlay layout

- `vendor/allwinnertech/chips/a733`: chip boot, UART, SDMMC, Wi-Fi, NPU,
  TRNG, THS, watchdog and USB-camera diagnostics.
- `vendor/allwinnertech/boards/a733/cubie-a7z`: defconfig, linker script,
  board bring-up, storage, GPIO and power control.
- `apps/system/a733wifi`: interactive dual-band Wi-Fi manager.
- `apps/system/a733services`: persistent optional service autostart.
- `apps/system/a733ftpd`: FTP service wrapper.
- other linked files: explicit compatibility deltas required by this port.

The authoritative feature set is:

```text
openvela-overlay/vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh/defconfig
```

## Boot boundary

The port reuses the known-good vendor Boot0/SCP/BL31/U-Boot chain. The
generated `nuttx.bin` starts with an ARM64 Image header and is entered through
`booti`. This repository does not redistribute vendor boot firmware.

## Completion boundary

UART/NSH, memory, SD/GPT/FAT, basic peripherals, Wi-Fi/network services and
VIP2 model execution are verified on real hardware. USB UVC, Bluetooth HCI,
GPU and UFS are not complete and must not be represented as complete.
