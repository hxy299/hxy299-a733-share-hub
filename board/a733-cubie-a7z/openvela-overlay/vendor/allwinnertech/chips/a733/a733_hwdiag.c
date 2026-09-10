/****************************************************************************
 * vendor/allwinnertech/chips/a733/a733_hwdiag.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_A733_HARDWARE_DIAG

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nuttx/fs/fs.h>

#include "arm64_internal.h"

#define REG32(a) getreg32(UINT64_C(a))

static ssize_t a733_hwdiag_read(struct file *filep, char *buffer,
                                size_t buflen)
{
  char report[1536];
  char ths_status[64];
  uint32_t ths_ints;
  uint32_t ths_code;
  int32_t ths_temp;
  size_t length;
  size_t copy;

  ths_code = REG32(0x025220c0) & 0x0fffu;
  ths_ints = REG32(0x02522020);
  if (ths_code != 0 && ths_code != 0x0fffu && (ths_ints & 1u) != 0)
    {
      ths_temp = ths_code > 1769 ? ((int32_t)ths_code - 2822) * -62 :
                                   ((int32_t)ths_code - 2835) * -59;
      snprintf(ths_status, sizeof(ths_status), "%ld.%03ldC",
               (long)(ths_temp / 1000),
               (long)(ths_temp < 0 ? -ths_temp % 1000 : ths_temp % 1000));
    }
  else
    {
      snprintf(ths_status, sizeof(ths_status), "unavailable");
    }

  length = (size_t)snprintf(report, sizeof(report),
    "A733 Cubie A7Z merged bring-up\n"
    "pio: r_pm_cfg0=%08lx r_pm_data=%08lx user-led=PM2\n"
    "wdt: mode=%08lx cfg=%08lx node=/dev/watchdog0\n"
    "ths: ints=%08lx code0=%lu temp0=%s gpadc_clk=%08lx bgr=%08lx\n"
    "sdmmc0: gctrl=%08lx clkcr=%08lx status=%08lx irq=193 pins=PF0..PF6\n"
    "twi: /dev/i2c2 ctl=%08lx PD16/17; /dev/i2c7 ctl=%08lx PJ22/23\n"
    "spi: /dev/spi1 gcr=%08lx tcr=%08lx PD10..PD13 polling\n"
    "pwm: /dev/pwm0 cer=%08lx pcr9=%08lx ppr9=%08lx fan=PJ27\n"
    "pinmux: UART2_M0 on PJ22/23 reserved; conflicts with active TWI7\n"
    "network: no onboard Ethernet; FCU760K Wi-Fi/BT uses USB1 node=/dev/a733-wifi\n"
    "ufs: hcs=%08lx hce=%08lx state=%s (non-blocking)\n",
    (unsigned long)REG32(0x07025030),
    (unsigned long)REG32(0x07025040),
    (unsigned long)REG32(0x02050014),
    (unsigned long)REG32(0x02050010),
    (unsigned long)ths_ints,
    (unsigned long)ths_code, ths_status,
    (unsigned long)REG32(0x02002fc0),
    (unsigned long)REG32(0x02002fe4),
    (unsigned long)REG32(0x04020000),
    (unsigned long)REG32(0x04020004),
    (unsigned long)REG32(0x0402003c),
    (unsigned long)REG32(0x0251200c),
    (unsigned long)REG32(0x0251700c),
    (unsigned long)REG32(0x02541004),
    (unsigned long)REG32(0x02541008),
    (unsigned long)REG32(0x025280c0),
    (unsigned long)REG32(0x02528230),
    (unsigned long)REG32(0x02528238),
    (unsigned long)REG32(0x04520030),
    (unsigned long)REG32(0x04520034),
    (REG32(0x04520030) & 1u) != 0 ? "present/link-capable" :
                                    "absent/not-started");

  if ((size_t)filep->f_pos >= length)
    {
      return 0;
    }

  copy = length - (size_t)filep->f_pos;
  if (copy > buflen)
    {
      copy = buflen;
    }

  memcpy(buffer, report + filep->f_pos, copy);
  filep->f_pos += copy;
  return (ssize_t)copy;
}

static const struct file_operations g_a733_hwdiag_fops =
{
  .read = a733_hwdiag_read,
};

int a733_hwdiag_initialize(void)
{
  return register_driver("/dev/a733-hw", &g_a733_hwdiag_fops, 0444, NULL);
}

#endif
