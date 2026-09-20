/****************************************************************************
 * apps/system/a733display/a733display_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

static volatile sig_atomic_t g_stop;

static void display_signal(int signo)
{
  (void)signo;
  g_stop = 1;
}

static void display_block(lv_obj_t *parent, int x, int y, int w, int h,
                          uint32_t rgb, int radius)
{
  lv_obj_t *block = lv_obj_create(parent);

  lv_obj_remove_style_all(block);
  lv_obj_set_pos(block, x, y);
  lv_obj_set_size(block, w, h);
  lv_obj_set_style_bg_opa(block, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(block, lv_color_hex(rgb), 0);
  lv_obj_set_style_radius(block, radius, 0);
}

static void display_test_scene(lv_display_t *disp)
{
  lv_obj_t *screen = lv_screen_active();
  int width = lv_display_get_horizontal_resolution(disp);
  int height = lv_display_get_vertical_resolution(disp);
  int band = width / 3;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  /* RGB bars prove color order.  Their unequal widths plus the face below
   * make rotation and clipping errors immediately visible.
   */

  display_block(screen, 0, 0, band, 20, 0xff0000, 0);
  display_block(screen, band, 0, band, 20, 0x00ff00, 0);
  display_block(screen, band * 2, 0, width - band * 2, 20, 0x0000ff, 0);
  display_block(screen, width / 4 - 8, height / 2 - 20,
                16, 24, 0x202020, 8);
  display_block(screen, width * 3 / 4 - 8, height / 2 - 20,
                16, 24, 0x202020, 8);
  display_block(screen, width / 4, height * 3 / 4,
                width / 2, 8, 0x202020, 4);
}

static int display_run(unsigned int seconds)
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  struct sigaction action;
  struct sigaction previous;
  unsigned int elapsed = 0;

  if (lv_is_initialized())
    {
      fprintf(stderr, "display: LVGL is already owned by another task\n");
      return -EBUSY;
    }

  memset(&action, 0, sizeof(action));
  action.sa_handler = display_signal;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, &previous);
  g_stop = 0;

  lv_init();
  lv_nuttx_dsc_init(&info);
  info.fb_path = "/dev/lcd0";
  memset(&result, 0, sizeof(result));
  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      lv_deinit();
      sigaction(SIGINT, &previous, NULL);
      fprintf(stderr, "display: cannot initialize /dev/lcd0\n");
      return -ENODEV;
    }

  display_test_scene(result.disp);
  puts("ST7735/LVGL test active; press Ctrl+C to stop.");
  while (!g_stop && (seconds == 0 || elapsed < seconds * 1000))
    {
      uint32_t idle = lv_timer_handler();
      unsigned int delay = idle == 0 ? 1 : idle > 20 ? 20 : idle;
      usleep(delay * 1000);
      elapsed += delay;
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  sigaction(SIGINT, &previous, NULL);
  return OK;
}

static void display_usage(const char *progname)
{
  fprintf(stderr,
          "Usage:\n"
          "  %s status\n"
          "  %s test [seconds]\n"
          "  %s run\n",
          progname, progname, progname);
}

int main(int argc, char *argv[])
{
  struct stat info;
  unsigned int seconds = 10;
  int ret;

  if (argc == 2 && strcmp(argv[1], "status") == 0)
    {
      ret = stat("/dev/lcd0", &info);
      printf("display: lcd0=%s controller=ST7735 resolution=128x160 "
             "format=RGB565 spi=1 mode=0 frequency=12000000\n",
             ret == 0 ? "ready" : "missing");
      return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

  if ((argc == 2 || argc == 3) && strcmp(argv[1], "test") == 0)
    {
      if (argc == 3)
        {
          char *end;
          unsigned long value = strtoul(argv[2], &end, 10);
          if (*argv[2] == '\0' || *end != '\0' || value > 3600)
            {
              display_usage(argv[0]);
              return EXIT_FAILURE;
            }

          seconds = value;
        }

      return display_run(seconds) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

  if (argc == 2 && strcmp(argv[1], "run") == 0)
    {
      return display_run(0) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

  display_usage(argv[0]);
  return EXIT_FAILURE;
}
