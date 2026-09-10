/****************************************************************************
 * include/nuttx/npu/a733_vip2.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __INCLUDE_NUTTX_NPU_A733_VIP2_H
#define __INCLUDE_NUTTX_NPU_A733_VIP2_H

#include <stdint.h>
#include <nuttx/fs/ioctl.h>

#define A733_NPU_ABI_VERSION       3u
#define A733_NPU_BUFFER_READ       (1u << 0)
#define A733_NPU_BUFFER_WRITE      (1u << 1)
#define A733_NPU_BUFFER_COMMAND    (1u << 2)
#define A733_NPU_SUBMIT_RESTORE    (1u << 0)
#define A733_NPU_MAX_TIMEOUT_MS    5000u
#define A733_NPU_MODEL_PATH_MAX    128u
#define A733_NPU_MODEL_NAME_MAX    32u
#define A733_NPU_NBG_MAGIC         UINT32_C(0x4e4d5056)
#define A733_NPU_NBG_V3_VERSION    UINT32_C(0x00010020)
#define A733_NPU_A733_CID          UINT32_C(0x1000003b)
#define A733_NPU_STAGE_MODEL       1u
#define A733_NPU_STAGE_INPUT       2u

struct a733_npu_info_s
{
  uint32_t abi_version;
  uint32_t cid_pid;
  uint32_t core_count;
  uint32_t page_size;
  uint32_t pool_size;
  uint32_t pool_free;
  uint32_t max_buffers;
  uint32_t max_timeout_ms;
};

struct a733_npu_alloc_s
{
  uint32_t size;
  uint32_t flags;
  uint32_t handle;
  uint32_t physical;
  uint32_t vip_address;
  uint32_t allocated;
  uint64_t cpu_address;
};

struct a733_npu_free_s
{
  uint32_t handle;
  uint32_t reserved;
};

struct a733_npu_cache_s
{
  uint32_t handle;
  uint32_t offset;
  uint32_t size;
  uint32_t reserved;
};

struct a733_npu_submit_s
{
  uint32_t command_handle;
  uint32_t command_offset;
  uint32_t command_size;
  uint32_t timeout_ms;
  uint32_t flags;
  uint32_t task_id;
  int32_t status;
  uint32_t irq_value;
  uint32_t idle;
  uint32_t polls;
};

struct a733_npu_sync_s
{
  uint32_t task_id;
  uint32_t timeout_ms;
  int32_t status;
  uint32_t irq_value;
  uint32_t idle;
};

/* Metadata-only NBG probe.  This deliberately does not claim that the
 * proprietary VIPLite prepare/link stage has run.  The file is streamed,
 * validated for the A733 target and checksummed without retaining it in RAM.
 */

struct a733_npu_model_probe_s
{
  char path[A733_NPU_MODEL_PATH_MAX];
  char name[A733_NPU_MODEL_NAME_MAX];
  uint64_t file_size;
  uint32_t crc32;
  uint32_t format_version;
  uint32_t target_cid;
  int32_t status;
  uint32_t reserved;
};

/* Copy a checked model or its input fixture into driver-owned, low-32-bit,
 * cache-coherent memory visible through the VIP2 page-descriptor MMU.  This
 * is a staging operation only: it does not imply that the proprietary NBG
 * prepare/link pass has produced executable commands.
 */

struct a733_npu_stage_file_s
{
  char path[A733_NPU_MODEL_PATH_MAX];
  uint64_t file_size;
  uint32_t crc32;
  uint32_t kind;
  uint32_t offset;
  uint32_t physical;
  uint32_t vip_address;
  int32_t status;
};

#define A733_NPUIOC_GET_INFO       _DIOC(0xe0)
#define A733_NPUIOC_ALLOC          _DIOC(0xe1)
#define A733_NPUIOC_FREE           _DIOC(0xe2)
#define A733_NPUIOC_CACHE_CLEAN    _DIOC(0xe3)
#define A733_NPUIOC_CACHE_INVALIDATE _DIOC(0xe4)
#define A733_NPUIOC_SUBMIT         _DIOC(0xe5)
#define A733_NPUIOC_SYNC           _DIOC(0xe6)
#define A733_NPUIOC_CANCEL         _DIOC(0xe7)
#define A733_NPUIOC_RESET          _DIOC(0xe8)
#define A733_NPUIOC_SELFTEST       _DIOC(0xe9)
#define A733_NPUIOC_MODEL_PROBE    _DIOC(0xea)
#define A733_NPUIOC_STAGE_FILE     _DIOC(0xeb)

#endif
