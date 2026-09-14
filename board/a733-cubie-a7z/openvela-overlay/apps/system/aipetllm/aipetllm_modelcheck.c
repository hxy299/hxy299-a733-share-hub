/****************************************************************************
 * apps/system/aipetllm/aipetllm_modelcheck.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ggml.h"
#define GGML_COMMON_DECL_C
#include "ggml-common.h"

#define GGUF_MAGIC UINT32_C(0x46554747)
#define GGUF_TYPE_UINT8 0
#define GGUF_TYPE_INT8 1
#define GGUF_TYPE_UINT16 2
#define GGUF_TYPE_INT16 3
#define GGUF_TYPE_UINT32 4
#define GGUF_TYPE_INT32 5
#define GGUF_TYPE_FLOAT32 6
#define GGUF_TYPE_BOOL 7
#define GGUF_TYPE_STRING 8
#define GGUF_TYPE_ARRAY 9
#define GGUF_TYPE_UINT64 10
#define GGUF_TYPE_INT64 11
#define GGUF_TYPE_FLOAT64 12

#define GGUF_KEY_MAX 256
#define GGUF_NAME_MAX 128
#define GGUF_DIMS_MAX 4
#define GGUF_TENSOR_LIMIT 4096
#define QWEN_LAYER_LIMIT 128

struct tensor_info_s
{
  char name[GGUF_NAME_MAX];
  uint64_t dimensions[GGUF_DIMS_MAX];
  uint64_t offset;
  uint64_t bytes;
  uint32_t dimension_count;
  uint32_t type;
};

struct qwen_layer_s
{
  uint16_t mask;
};

static int read_exact(FILE *stream, void *buffer, size_t length)
{
  return fread(buffer, 1, length, stream) == length ? 0 : -EIO;
}

static int skip_exact(FILE *stream, uint64_t length)
{
  while (length != 0)
    {
      long step = length > (uint64_t)LONG_MAX ? LONG_MAX : (long)length;
      if (fseek(stream, step, SEEK_CUR) != 0)
        {
          return -errno;
        }

      length -= (uint64_t)step;
    }

  return 0;
}

static int read_string(FILE *stream, char *value, size_t capacity,
                       uint64_t *full_length)
{
  uint64_t length;
  size_t keep;

  if (read_exact(stream, &length, sizeof(length)) < 0)
    {
      return -EIO;
    }

  if (full_length != NULL)
    {
      *full_length = length;
    }

  keep = capacity == 0 ? 0 :
         length < capacity - 1 ? (size_t)length : capacity - 1;
  if (keep != 0 && read_exact(stream, value, keep) < 0)
    {
      return -EIO;
    }

  if (capacity != 0)
    {
      value[keep] = '\0';
    }

  return skip_exact(stream, length - keep);
}

static size_t scalar_size(uint32_t type)
{
  switch (type)
    {
      case GGUF_TYPE_UINT8:
      case GGUF_TYPE_INT8:
      case GGUF_TYPE_BOOL:
        return 1;
      case GGUF_TYPE_UINT16:
      case GGUF_TYPE_INT16:
        return 2;
      case GGUF_TYPE_UINT32:
      case GGUF_TYPE_INT32:
      case GGUF_TYPE_FLOAT32:
        return 4;
      case GGUF_TYPE_UINT64:
      case GGUF_TYPE_INT64:
      case GGUF_TYPE_FLOAT64:
        return 8;
      default:
        return 0;
    }
}

static int skip_value(FILE *stream, uint32_t type, unsigned int depth)
{
  uint32_t element_type;
  uint64_t count;
  uint64_t index;
  uint64_t length;
  size_t size;

  if (depth > 4)
    {
      return -EOVERFLOW;
    }

  size = scalar_size(type);
  if (size != 0)
    {
      return skip_exact(stream, size);
    }

  if (type == GGUF_TYPE_STRING)
    {
      if (read_exact(stream, &length, sizeof(length)) < 0)
        {
          return -EIO;
        }

      return skip_exact(stream, length);
    }

  if (type != GGUF_TYPE_ARRAY ||
      read_exact(stream, &element_type, sizeof(element_type)) < 0 ||
      read_exact(stream, &count, sizeof(count)) < 0)
    {
      return -EINVAL;
    }

  size = scalar_size(element_type);
  if (size != 0)
    {
      if (count > UINT64_MAX / size)
        {
          return -EOVERFLOW;
        }

      return skip_exact(stream, count * size);
    }

  for (index = 0; index < count; index++)
    {
      int result = skip_value(stream, element_type, depth + 1);
      if (result < 0)
        {
          return result;
        }
    }

  return 0;
}

static const char *tensor_type_name(uint32_t type)
{
  switch (type)
    {
      case GGML_TYPE_F32: return "F32";
      case GGML_TYPE_F16: return "F16";
      case GGML_TYPE_Q4_0: return "Q4_0";
      case GGML_TYPE_Q4_1: return "Q4_1";
      case GGML_TYPE_Q5_0: return "Q5_0";
      case GGML_TYPE_Q5_1: return "Q5_1";
      case GGML_TYPE_Q8_0: return "Q8_0";
      case GGML_TYPE_Q8_1: return "Q8_1";
      case GGML_TYPE_Q2_K: return "Q2_K";
      case GGML_TYPE_Q3_K: return "Q3_K";
      case GGML_TYPE_Q4_K: return "Q4_K";
      case GGML_TYPE_Q5_K: return "Q5_K";
      case GGML_TYPE_Q6_K: return "Q6_K";
      case GGML_TYPE_Q8_K: return "Q8_K";
      case GGML_TYPE_I8: return "I8";
      case GGML_TYPE_I16: return "I16";
      case GGML_TYPE_I32: return "I32";
      case GGML_TYPE_I64: return "I64";
      case GGML_TYPE_F64: return "F64";
      case GGML_TYPE_BF16: return "BF16";
      default: return "unsupported";
    }
}

static int tensor_layout(uint32_t type, uint64_t *block, uint64_t *size)
{
  *block = 1;
  switch (type)
    {
      case GGML_TYPE_F32: *size = 4; return 0;
      case GGML_TYPE_F16:
      case GGML_TYPE_BF16: *size = 2; return 0;
      case GGML_TYPE_I8: *size = 1; return 0;
      case GGML_TYPE_I16: *size = 2; return 0;
      case GGML_TYPE_I32: *size = 4; return 0;
      case GGML_TYPE_I64:
      case GGML_TYPE_F64: *size = 8; return 0;
      case GGML_TYPE_Q4_0: *block = QK4_0; *size = sizeof(block_q4_0); return 0;
      case GGML_TYPE_Q4_1: *block = QK4_1; *size = sizeof(block_q4_1); return 0;
      case GGML_TYPE_Q5_0: *block = QK5_0; *size = sizeof(block_q5_0); return 0;
      case GGML_TYPE_Q5_1: *block = QK5_1; *size = sizeof(block_q5_1); return 0;
      case GGML_TYPE_Q8_0: *block = QK8_0; *size = sizeof(block_q8_0); return 0;
      case GGML_TYPE_Q8_1: *block = QK8_1; *size = sizeof(block_q8_1); return 0;
      case GGML_TYPE_Q2_K: *block = QK_K; *size = sizeof(block_q2_K); return 0;
      case GGML_TYPE_Q3_K: *block = QK_K; *size = sizeof(block_q3_K); return 0;
      case GGML_TYPE_Q4_K: *block = QK_K; *size = sizeof(block_q4_K); return 0;
      case GGML_TYPE_Q5_K: *block = QK_K; *size = sizeof(block_q5_K); return 0;
      case GGML_TYPE_Q6_K: *block = QK_K; *size = sizeof(block_q6_K); return 0;
      case GGML_TYPE_Q8_K: *block = QK_K; *size = sizeof(block_q8_K); return 0;
      default: return -ENOTSUP;
    }
}

static int tensor_bytes(const uint64_t *dimensions, uint32_t count,
                        uint32_t type, uint64_t *result)
{
  uint64_t block = 0;
  uint64_t size = 0;
  uint64_t elements = 1;
  uint32_t index;
  int ret;

  ret = tensor_layout(type, &block, &size);
  if (ret < 0 || count == 0 || count > GGUF_DIMS_MAX ||
      dimensions[0] == 0 || dimensions[0] % block != 0)
    {
      return ret < 0 ? ret : -EINVAL;
    }

  for (index = 1; index < count; index++)
    {
      if (dimensions[index] == 0 || elements > UINT64_MAX / dimensions[index])
        {
          return -EOVERFLOW;
        }

      elements *= dimensions[index];
    }

  if (dimensions[0] / block > UINT64_MAX / elements ||
      dimensions[0] / block * elements > UINT64_MAX / size)
    {
      return -EOVERFLOW;
    }

  *result = dimensions[0] / block * elements * size;
  return 0;
}

static uint16_t qwen_tensor_bit(const char *suffix)
{
  static const char *const names[] =
  {
    "attn_norm.weight", "attn_q.weight", "attn_k.weight",
    "attn_v.weight", "attn_output.weight", "ffn_norm.weight",
    "ffn_gate.weight", "ffn_up.weight", "ffn_down.weight"
  };
  unsigned int index;

  for (index = 0; index < sizeof(names) / sizeof(names[0]); index++)
    {
      if (strcmp(suffix, names[index]) == 0)
        {
          return (uint16_t)(1u << index);
        }
    }

  return 0;
}

static void record_qwen_tensor(const char *name, struct qwen_layer_s *layers,
                               int *token_embedding, int *output_norm,
                               int *output)
{
  unsigned int layer;
  int consumed;

  if (strcmp(name, "token_embd.weight") == 0)
    {
      *token_embedding = 1;
    }
  else if (strcmp(name, "output_norm.weight") == 0)
    {
      *output_norm = 1;
    }
  else if (strcmp(name, "output.weight") == 0)
    {
      *output = 1;
    }
  else if (sscanf(name, "blk.%u.%n", &layer, &consumed) == 1 &&
           layer < QWEN_LAYER_LIMIT && consumed > 0)
    {
      layers[layer].mask |= qwen_tensor_bit(name + consumed);
    }
}

int aipetllm_model_checkpoint(const char *path)
{
  struct qwen_layer_s layers[QWEN_LAYER_LIMIT];
  struct tensor_info_s *tensors = NULL;
  struct stat file_info;
  FILE *stream = NULL;
  char architecture[32] = "unknown";
  uint64_t metadata_count;
  uint64_t tensor_count;
  uint64_t file_size;
  uint64_t data_start;
  uint64_t total_bytes = 0;
  uint64_t type_bytes[GGML_TYPE_COUNT];
  uint32_t type_counts[GGML_TYPE_COUNT];
  uint32_t version;
  uint32_t alignment = 32;
  uint32_t block_count = 0;
  uint32_t magic;
  uint64_t index;
  int token_embedding = 0;
  int output_norm = 0;
  int output = 0;
  int ret = 1;

  memset(layers, 0, sizeof(layers));
  memset(type_bytes, 0, sizeof(type_bytes));
  memset(type_counts, 0, sizeof(type_counts));

  if (stat(path, &file_info) < 0 || !S_ISREG(file_info.st_mode))
    {
      fprintf(stderr, "aipetllm: modelcheck cannot stat %s: %d (%s)\n",
              path, errno, strerror(errno));
      return 1;
    }

  file_size = (uint64_t)file_info.st_size;
  stream = fopen(path, "rb");
  if (stream == NULL)
    {
      fprintf(stderr, "aipetllm: modelcheck cannot open %s: %d (%s)\n",
              path, errno, strerror(errno));
      return 1;
    }

  if (read_exact(stream, &magic, sizeof(magic)) < 0 ||
      read_exact(stream, &version, sizeof(version)) < 0 ||
      read_exact(stream, &tensor_count, sizeof(tensor_count)) < 0 ||
      read_exact(stream, &metadata_count, sizeof(metadata_count)) < 0 ||
      magic != GGUF_MAGIC || version < 2 || version > 3 ||
      tensor_count == 0 || tensor_count > GGUF_TENSOR_LIMIT)
    {
      fputs("aipetllm: invalid GGUF header/tensor count\n", stderr);
      goto out;
    }

  for (index = 0; index < metadata_count; index++)
    {
      char key[GGUF_KEY_MAX];
      uint64_t key_length;
      uint32_t type;

      if (read_string(stream, key, sizeof(key), &key_length) < 0 ||
          key_length >= sizeof(key) ||
          read_exact(stream, &type, sizeof(type)) < 0)
        {
          fputs("aipetllm: malformed GGUF metadata\n", stderr);
          goto out;
        }

      if (strcmp(key, "general.architecture") == 0 &&
          type == GGUF_TYPE_STRING)
        {
          if (read_string(stream, architecture, sizeof(architecture), NULL) < 0)
            {
              goto out;
            }
        }
      else if (strcmp(key, "general.alignment") == 0 &&
               type == GGUF_TYPE_UINT32)
        {
          if (read_exact(stream, &alignment, sizeof(alignment)) < 0)
            {
              goto out;
            }
        }
      else if (strstr(key, ".block_count") != NULL &&
               type == GGUF_TYPE_UINT32)
        {
          if (read_exact(stream, &block_count, sizeof(block_count)) < 0)
            {
              goto out;
            }
        }
      else if (skip_value(stream, type, 0) < 0)
        {
          fprintf(stderr, "aipetllm: unsupported metadata key=%s type=%" PRIu32 "\n",
                  key, type);
          goto out;
        }
    }

  if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
      alignment > 4096 || block_count > QWEN_LAYER_LIMIT)
    {
      fprintf(stderr, "aipetllm: unsafe alignment/layer count %" PRIu32
                      "/%" PRIu32 "\n", alignment, block_count);
      goto out;
    }

  tensors = calloc((size_t)tensor_count, sizeof(*tensors));
  if (tensors == NULL)
    {
      fputs("aipetllm: tensor directory allocation failed\n", stderr);
      goto out;
    }

  for (index = 0; index < tensor_count; index++)
    {
      struct tensor_info_s *tensor = &tensors[index];
      uint64_t name_length;
      uint32_t dimension;

      if (read_string(stream, tensor->name, sizeof(tensor->name),
                      &name_length) < 0 ||
          name_length >= sizeof(tensor->name) ||
          read_exact(stream, &tensor->dimension_count,
                     sizeof(tensor->dimension_count)) < 0 ||
          tensor->dimension_count == 0 ||
          tensor->dimension_count > GGUF_DIMS_MAX)
        {
          fprintf(stderr, "aipetllm: malformed tensor directory at %" PRIu64 "\n",
                  index);
          goto out;
        }

      for (dimension = 0; dimension < tensor->dimension_count; dimension++)
        {
          if (read_exact(stream, &tensor->dimensions[dimension],
                         sizeof(uint64_t)) < 0)
            {
              goto out;
            }
        }

      if (read_exact(stream, &tensor->type, sizeof(tensor->type)) < 0 ||
          read_exact(stream, &tensor->offset, sizeof(tensor->offset)) < 0 ||
          tensor_bytes(tensor->dimensions, tensor->dimension_count,
                       tensor->type, &tensor->bytes) < 0)
        {
          fprintf(stderr, "aipetllm: unsupported/invalid tensor %s type=%" PRIu32
                          " (%s)\n", tensor->name, tensor->type,
                  tensor_type_name(tensor->type));
          goto out;
        }

      if (tensor->type < GGML_TYPE_COUNT)
        {
          type_counts[tensor->type]++;
          type_bytes[tensor->type] += tensor->bytes;
        }

      total_bytes += tensor->bytes;
      record_qwen_tensor(tensor->name, layers, &token_embedding,
                         &output_norm, &output);
    }

  {
    long position = ftell(stream);
    if (position < 0)
      {
        goto out;
      }

    data_start = ((uint64_t)position + alignment - 1) &
                 ~(uint64_t)(alignment - 1);
  }

  for (index = 0; index < tensor_count; index++)
    {
      struct tensor_info_s *tensor = &tensors[index];
      if (tensor->offset % alignment != 0 ||
          tensor->offset > UINT64_MAX - data_start ||
          data_start + tensor->offset > file_size ||
          tensor->bytes > file_size - (data_start + tensor->offset))
        {
          fprintf(stderr, "aipetllm: tensor outside file/alignment name=%s "
                          "offset=%" PRIu64 " bytes=%" PRIu64 "\n",
                  tensor->name, tensor->offset, tensor->bytes);
          goto out;
        }
    }

  printf("model-check path=%s architecture=%s version=%" PRIu32 "\n",
         path, architecture, version);
  printf("file=%" PRIu64 " tensors=%" PRIu64 " metadata=%" PRIu64
         " alignment=%" PRIu32 " data-offset=%" PRIu64 "\n",
         file_size, tensor_count, metadata_count, alignment, data_start);
  printf("tensor-payload=%" PRIu64 " MiB (sum, shared/padding excluded)\n",
         total_bytes / (1024 * 1024));

  for (index = 0; index < GGML_TYPE_COUNT; index++)
    {
      if (type_counts[index] != 0)
        {
          printf("type[%s]=%" PRIu32 " tensors, %" PRIu64 " MiB\n",
                 tensor_type_name((uint32_t)index), type_counts[index],
                 type_bytes[index] / (1024 * 1024));
        }
    }

  if (strcmp(architecture, "qwen2") != 0 || block_count == 0 ||
      !token_embedding || !output_norm)
    {
      fprintf(stderr, "aipetllm: expected qwen2 roots missing "
                      "layers=%" PRIu32 " token=%d norm=%d output=%d\n",
              block_count, token_embedding, output_norm, output);
      goto out;
    }

  for (index = 0; index < block_count; index++)
    {
      if (layers[index].mask != UINT16_C(0x01ff))
        {
          fprintf(stderr, "aipetllm: qwen2 layer %" PRIu64
                          " incomplete mask=%04x expected=01ff\n",
                  index, layers[index].mask);
          goto out;
        }
    }

  printf("qwen2-check passed layers=%" PRIu32
         " roots=token+norm+%s layer-mask=01ff\n",
         block_count, output ? "output" : "tied-output");
  puts("model directory is safe for staged tensor loading; inference is pending.");
  ret = 0;

out:
  free(tensors);
  if (stream != NULL)
    {
      fclose(stream);
    }

  return ret;
}
