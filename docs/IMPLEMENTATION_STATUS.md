# A733 / Cubie A7Z implementation status

Status date: 2026-09-10. “Passed” means observed on a real 4 GiB Cubie A7Z,
not merely compiled.

## Verified system baseline

- Boot0/SCP/BL31/U-Boot hands off to openvela at EL1 through `booti`.
- AArch64 runtime, MMU, exceptions, GICv3 and virtual timer work.
- UART0 provides early output and an interactive NSH console at 115200 8N1.
- 4 GiB DRAM is exposed through split heap regions while preserving firmware
  reservations.
- procfs, `free`, `ps`, `uptime`, `dmesg`, signals and Ctrl+C work.
- `poweroff` and `reboot` use the board/PSCI path.

## Verified storage and peripherals

- SDMMC0: PIO, 400 kHz initialization, 12 MHz transfer, 4-bit bus.
- GPT partitions: firmware, EFI, openvela and data.
- FAT data partition mounted read/write at `/data`, UTF-8 long file names.
- User LED, watchdog, THS temperature sensor, I2C2/I2C7, SPI1 and fan PWM.
- UFS is absent on the test board and is deliberately non-blocking.

## Verified FCU760K Wi-Fi and services

- Module: FCU760K / AIC8800D80-U02 over dedicated USB1 high-speed link.
- BootROM `a69c:8d80`, runtime `a69c:8d81`.
- Firmware upload, patch configuration, re-enumeration, LMAC/RF/ME/regulatory
  setup and station VIF.
- 2.4 GHz and 5 GHz active scan, WPA2-PSK/CCMP, DHCP, ARP, DNS, TCP/UDP.
- Native `wifi` command, remembered-network reconnect, optional autostart.
- SSH/SCP, FTP, curl/wget/HTTPS, NTP and iperf.
- Board TRNG backs `/dev/random` and `/dev/urandom` for TLS/SSH.
- Observed throughput baseline: TCP 4.13 Mbit/s; UDP 10.8 Mbit/s with 5.7%
  loss in one 30-second test. This is a functional baseline, not a peak claim.

## Verified VIP2 NPU

- Power/clock/reset, IRQ, DMA pool, MMU page tables, cache maintenance and
  synchronous task ABI.
- Hardware CID `0x1000003b`; runtime device `/dev/npu0`.
- LeNet output CRC32 `d50f5d79`, byte prefix
  `003c000000000000000000000000000000000000`, matching Linux VIPLite golden.
- YOLOv5s three-output golden regression passed.
- YOLOv8n PCQ six-output golden regression and dynamic 640×640 RGB input
  passed. Dog input produces detections and black input does not, proving the
  result is generated from the supplied input rather than replayed.
- Current interchange path is authorized Linux VIPLite prepare/trace → A7PM
  checked package → openvela executor. Generic proprietary NBG linking is not
  implemented in openvela.

## In progress: USB UVC camera

The current source contains the v65 diagnostic implementation. Type-C state,
USB2 PHY and xHCI MMIO are visible, but the camera is not enumerated. Evidence
shows that resetting xHCI/DWC3 destroys the vendor firmware’s Cadence Combo
PHY/PIPE handoff, after which HCRST/CNR recovery times out.

Next implementation checkpoint:

1. preserve the working firmware handoff;
2. do not reset DWC3 or xHCI;
3. construct DMA32 DCBAA, command ring, event ring and interrupter state;
4. run controller and issue Enable Slot / Address Device;
5. enumerate descriptors, implement UVC PROBE/COMMIT and acquire frames;
6. convert/resize to RGB640, feed existing YOLOv8 dynamic input and emit
   boxes/classes/scores.

## Not complete

- Bluetooth HCI and profiles.
- Imagination BXM GPU runtime.
- UFS host/storage and UFS-first boot.
- MIPI CSI IMX214 driver.
- SMP/DVFS/power-management production hardening.
- Generic on-openvela NBG compiler/linker.

These items are intentionally excluded from completed-feature claims.
