/****************************************************************************
 * vendor/allwinnertech/boards/a733/cubie-a7z/src/a7z_storage.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/* The vendor GPT uses the same textual name for several partitions, so the
 * generic name-based registrar would collide at /dev/primary.  Keep the GPT
 * itself compatible with U-Boot and assign stable board-specific node names
 * from the GPT entry index instead.  Only entry 4 is writable and mounted.
 */

#include <nuttx/config.h>

#ifdef CONFIG_BOARD_A7Z_STORAGE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <syslog.h>

#include <nuttx/drivers/drivers.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/partition.h>

#define A7Z_DATA_INDEX       3
#define A7Z_DATA_FIRST_LBA   917504
#define A7Z_DATA_MIN_BLOCKS  131000

struct a7z_partition_ctx_s
{
  int  result;
  bool data_found;
};

static const char *const g_a7z_partition_nodes[] =
{
  "/dev/a7z-firmware",
  "/dev/a7z-efi",
  "/dev/a7z-openvela",
  "/dev/a7z-data"
};

static void a7z_partition_handler(struct partition_s *part, void *arg)
{
  struct a7z_partition_ctx_s *ctx = arg;
  const char *path;
  mode_t mode;
  int ret;

  if (part->index >= sizeof(g_a7z_partition_nodes) /
                     sizeof(g_a7z_partition_nodes[0]))
    {
      syslog(LOG_WARNING,
             "A733 GPT: ignoring unexpected entry %zu name=%s\n",
             part->index + 1, part->name);
      return;
    }

  path = g_a7z_partition_nodes[part->index];
  mode = part->index == A7Z_DATA_INDEX ? 0660 : 0440;
  ret = register_blockpartition(path, mode, "/dev/mmcsd0",
                                part->firstblock, part->nblocks);
  if (ret < 0 && ret != -EEXIST)
    {
      syslog(LOG_ERR, "A733 GPT: failed to register %s: %d\n", path, ret);
      if (ctx->result >= 0)
        {
          ctx->result = ret;
        }

      return;
    }

  syslog(LOG_INFO,
         "A733 GPT: entry=%zu node=%s first=%zu blocks=%zu name=%s\n",
         part->index + 1, path, part->firstblock, part->nblocks, part->name);

  if (part->index == A7Z_DATA_INDEX)
    {
      if (part->blocksize != 512 ||
          part->firstblock != A7Z_DATA_FIRST_LBA ||
          part->nblocks < A7Z_DATA_MIN_BLOCKS)
        {
          syslog(LOG_ERR,
                 "A733 GPT: refusing unexpected data layout "
                 "block=%zu first=%zu count=%zu\n",
                 part->blocksize, part->firstblock, part->nblocks);
          ctx->result = -EINVAL;
          return;
        }

      ctx->data_found = true;
    }
}

int a7z_storage_initialize(void)
{
  struct a7z_partition_ctx_s ctx =
  {
    .result = OK,
    .data_found = false
  };
  int ret;

  ret = parse_block_partition("/dev/mmcsd0", a7z_partition_handler, &ctx);
  if (ret < 0)
    {
      syslog(LOG_ERR, "A733 GPT: parse failed: %d\n", ret);
      return ret;
    }

  if (ctx.result < 0)
    {
      return ctx.result;
    }

  if (!ctx.data_found)
    {
      syslog(LOG_ERR, "A733 GPT: dedicated data partition not found\n");
      return -ENODEV;
    }

  ret = bchdev_register("/dev/mmcsd0", "/dev/sdcard", O_RDONLY);
  if (ret < 0 && ret != -EEXIST)
    {
      syslog(LOG_WARNING, "A733 GPT: /dev/sdcard failed: %d\n", ret);
    }

  ret = mkdir("/data", 0777);
  if (ret < 0 && errno != EEXIST)
    {
      syslog(LOG_ERR, "A733 FAT: mkdir /data failed: %d\n", errno);
      return -errno;
    }

  ret = nx_mount("/dev/a7z-data", "/data", "vfat", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "A733 FAT: mount /dev/a7z-data failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO,
         "A733 FAT: /dev/a7z-data mounted read/write at /data\n");
  return OK;
}

#endif /* CONFIG_BOARD_A7Z_STORAGE */
