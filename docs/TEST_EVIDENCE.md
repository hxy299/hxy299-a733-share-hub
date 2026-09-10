# Real-board test evidence

Board: Radxa Cubie A7Z, Allwinner A733, 4 GiB RAM, microSD boot, no UFS.

## Boot and operating system

Observed successful boot boundary:

```text
- Boot from EL1
- Boot to C runtime for OS Initialize
NuttShell (NSH)
nsh> uname -a
NuttX 0.0.0 ... arm64 cubie-a7z
```

`free` reported about 4.27 GB of managed memory, procfs mounted, and `ps`
showed the idle thread, high-priority work queue and NSH task.

## SD/GPT/FAT

The following reads completed without I/O error after the SDMMC multi-block
fix:

```text
dd if=/dev/mmcsd0 of=/dev/null bs=512 count=1
dd if=/dev/mmcsd0 of=/dev/null bs=16384 count=64
dd if=/dev/mmcsd0 of=/dev/null bs=65536 count=16
```

Board logs registered GPT entries and mounted `/dev/a7z-data` read/write at
`/data`.

## Temperature and board devices

```text
sensortest -n 3 temp0
temp0: ... value:34.47
temp0: ... value:34.47
temp0: ... value:34.47
```

Verified device nodes include `/dev/userled`, `/dev/watchdog0`,
`/dev/uorb/sensor_temp0`, `/dev/i2c2`, `/dev/i2c7`, `/dev/spi1`, `/dev/pwm0`,
`/dev/mmcsd0`, `/dev/a733-wifi` and `/dev/npu0`.

## Wi-Fi and IPv4

The FCU760K completed firmware transition from VID:PID `a69c:8d80` to
`a69c:8d81`. Both a 2.4 GHz BSS and a 5745 MHz 5 GHz BSS were scanned. The
board associated with WPA2 and obtained `192.168.31.27/24` by DHCP in the test
network. Network names and passwords are not part of this repository.

Functional tests passed for gateway ping, Internet IPv4 ping, DNS lookup,
TCP, UDP, wget/curl, SSH and FTP. A representative local iperf run measured:

- TCP: 14.9 MiB in 30.25 s, 4.13 Mbit/s.
- UDP: 38.9 MiB in 30.15 s, 10.8 Mbit/s, 5.7% loss.

FTP was exercised with small files, larger files and a PDF containing spaces
and Chinese characters in its local path. SSH was exercised from Windows to
remote NSH after host-key, ARP and socket-lifecycle fixes.

## VIP2 model execution

### LeNet

Linux VIPLite reference:

```text
hardware CID=1000003b
profile inference=164..178 us
output[0] raw-crc32=d50f5d79
```

openvela `/dev/npu0` produced the same CRC and 20-byte output:

```text
003c000000000000000000000000000000000000
```

### YOLOv5s

The official A733 model with deterministic zero input produced three output
CRCs on Linux and the same three CRCs on openvela:

```text
53127e9c  ddb5675e  6e01633e
```

### YOLOv8n PCQ

Linux VIPLite reference reported 12,604 us inference for six outputs. The
six golden CRC32 values were:

```text
55d0becd ead06611 ba209c57 2637657c 9e7f743a 2eb4d5ce
```

openvela matched all six for the embedded dog input. A dynamically supplied
copy of the dog RGB input matched again. A black 640×640 RGB input changed the
output CRCs and decoded zero detections while MMU/IRQ/guard checks remained
clean.

## Current UVC failure boundary

The latest diagnostic reads a valid xHCI capability block but xHCI HCRST does
not clear after the vendor Combo PHY handoff has been disturbed. DWC3/app
registers subsequently read zero. This is recorded as an in-progress driver
issue, not a camera success.
