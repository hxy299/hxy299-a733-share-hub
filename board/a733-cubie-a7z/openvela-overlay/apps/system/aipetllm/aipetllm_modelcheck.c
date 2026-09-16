/****************************************************************************
 * apps/system/aipetllm/aipetllm_modelcheck.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ggml.h"
#define GGML_COMMON_DECL_C
#include "ggml-common.h"
#include "ggml-quants.h"
#include "quants.h"

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
    long file_position = ftell(stream);
    if (file_position < 0)
      {
        goto out;
      }

    data_start = ((uint64_t)file_position + alignment - 1) &
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

static uint32_t tensor_crc32(uint32_t crc, const uint8_t *data, size_t length)
{
  size_t index;

  crc = ~crc;
  for (index = 0; index < length; index++)
    {
      unsigned int bit;

      crc ^= data[index];
      for (bit = 0; bit < 8; bit++)
        {
          crc = (crc >> 1) ^ (UINT32_C(0xedb88320) &
                              (uint32_t)-(int32_t)(crc & 1));
        }
    }

  return ~crc;
}

static int read_dequantized_row(FILE *stream, uint64_t absolute_offset,
                                const struct tensor_info_s *tensor,
                                uint64_t row, float *output)
{
  uint64_t row_bytes;
  uint64_t row_dimensions[1];
  void *packed;
  int result = -EINVAL;

  row_dimensions[0] = tensor->dimensions[0];
  if (tensor->dimension_count < 1 ||
      tensor_bytes(row_dimensions, 1, tensor->type, &row_bytes) < 0 ||
      row > UINT64_MAX / row_bytes ||
      absolute_offset > UINT64_MAX - row * row_bytes ||
      absolute_offset + row * row_bytes > (uint64_t)LONG_MAX)
    {
      return -EOVERFLOW;
    }

  if (fseek(stream, (long)(absolute_offset + row * row_bytes), SEEK_SET) != 0)
    {
      return -errno;
    }

  packed = malloc((size_t)row_bytes);
  if (packed == NULL)
    {
      return -ENOMEM;
    }

  if (read_exact(stream, packed, (size_t)row_bytes) < 0)
    {
      result = -EIO;
      goto out;
    }

  switch (tensor->type)
    {
      case GGML_TYPE_F32:
        memcpy(output, packed, (size_t)tensor->dimensions[0] * sizeof(float));
        result = 0;
        break;
      case GGML_TYPE_Q4_K:
        dequantize_row_q4_K((const block_q4_K *)packed, output,
                            (int64_t)tensor->dimensions[0]);
        result = 0;
        break;
      case GGML_TYPE_Q6_K:
        dequantize_row_q6_K((const block_q6_K *)packed, output,
                            (int64_t)tensor->dimensions[0]);
        result = 0;
        break;
      default:
        result = -ENOTSUP;
        break;
    }

out:
  free(packed);
  return result;
}

static int read_quantized_matvec(FILE *stream, uint64_t absolute_offset,
                                 const struct tensor_info_s *tensor,
                                 const float *input, float *output)
{
  uint64_t row_dimensions[1];
  uint64_t row_bytes;
  uint64_t rows;
  void *packed = NULL;
  block_q8_K *input_q8 = NULL;
  uint64_t row;
  int result = -EINVAL;

  if (tensor->dimension_count != 2 || tensor->dimensions[0] == 0 ||
      tensor->dimensions[0] > INT_MAX || tensor->dimensions[1] == 0 ||
      tensor->dimensions[1] > 65536)
    {
      return -EINVAL;
    }

  row_dimensions[0] = tensor->dimensions[0];
  rows = tensor->dimensions[1];
  if (tensor_bytes(row_dimensions, 1, tensor->type, &row_bytes) < 0 ||
      row_bytes == 0 || row_bytes > SIZE_MAX ||
      rows > UINT64_MAX / row_bytes ||
      absolute_offset > UINT64_MAX - rows * row_bytes ||
      absolute_offset > (uint64_t)LONG_MAX)
    {
      return -EOVERFLOW;
    }

  packed = malloc((size_t)row_bytes);
  if (packed == NULL)
    {
      return -ENOMEM;
    }

  if (tensor->type == GGML_TYPE_Q4_K ||
      tensor->type == GGML_TYPE_Q6_K)
    {
      size_t blocks;

      if (tensor->dimensions[0] % QK_K != 0)
        {
          result = -EINVAL;
          goto out;
        }

      blocks = (size_t)(tensor->dimensions[0] / QK_K);
      if (blocks > SIZE_MAX / sizeof(block_q8_K))
        {
          result = -EOVERFLOW;
          goto out;
        }

      input_q8 = malloc(blocks * sizeof(block_q8_K));
      if (input_q8 == NULL)
        {
          result = -ENOMEM;
          goto out;
        }

      quantize_row_q8_K(input, input_q8,
                        (int64_t)tensor->dimensions[0]);
    }
  else if (tensor->type != GGML_TYPE_F32)
    {
      result = -ENOTSUP;
      goto out;
    }

  if (fseek(stream, (long)absolute_offset, SEEK_SET) != 0)
    {
      result = -errno;
      goto out;
    }

  for (row = 0; row < rows; row++)
    {
      if (read_exact(stream, packed, (size_t)row_bytes) < 0)
        {
          result = -EIO;
          goto out;
        }

      if (tensor->type == GGML_TYPE_Q4_K)
        {
          ggml_vec_dot_q4_K_q8_K((int)tensor->dimensions[0],
                                 &output[row], 0, packed, 0,
                                 input_q8, 0, 1);
        }
      else if (tensor->type == GGML_TYPE_Q6_K)
        {
          ggml_vec_dot_q6_K_q8_K((int)tensor->dimensions[0],
                                 &output[row], 0, packed, 0,
                                 input_q8, 0, 1);
        }
      else
        {
          const float *weights = packed;
          double sum = 0.0;
          uint64_t column;

          for (column = 0; column < tensor->dimensions[0]; column++)
            {
              sum += (double)weights[column] * input[column];
            }

          output[row] = (float)sum;
        }
    }

  result = 0;

out:
  free(input_q8);
  free(packed);
  return result;
}

static int add_f32_bias(FILE *stream, uint64_t data_start,
                        const struct tensor_info_s *bias,
                        float *values, uint64_t count)
{
  float *bias_values;
  uint64_t index;
  int result;

  if (bias == NULL || bias->dimension_count != 1 ||
      bias->dimensions[0] != count || bias->type != GGML_TYPE_F32 ||
      data_start > UINT64_MAX - bias->offset)
    {
      return -EINVAL;
    }

  bias_values = malloc((size_t)count * sizeof(float));
  if (bias_values == NULL)
    {
      return -ENOMEM;
    }

  result = read_dequantized_row(stream, data_start + bias->offset,
                                bias, 0, bias_values);
  if (result == 0)
    {
      for (index = 0; index < count; index++)
        {
          values[index] += bias_values[index];
        }
    }

  free(bias_values);
  return result;
}

static int apply_normal_rope(float *values, uint64_t heads,
                             uint64_t head_dimension,
                             uint32_t rotary_dimension,
                             uint32_t position, float frequency_base)
{
  uint64_t head;
  uint32_t pair;

  if (heads == 0 || head_dimension == 0 ||
      heads > UINT64_MAX / head_dimension ||
      rotary_dimension == 0 || (rotary_dimension & 1) != 0 ||
      rotary_dimension > head_dimension || !isfinite(frequency_base) ||
      frequency_base <= 1.0f)
    {
      return -EINVAL;
    }

  for (head = 0; head < heads; head++)
    {
      float *head_values = values + head * head_dimension;

      for (pair = 0; pair < rotary_dimension / 2; pair++)
        {
          uint32_t component = pair * 2;
          float exponent = -(float)component / rotary_dimension;
          float angle = (float)position * powf(frequency_base, exponent);
          float cosine = cosf(angle);
          float sine = sinf(angle);
          float first = head_values[component];
          float second = head_values[component + 1];

          head_values[component] = first * cosine - second * sine;
          head_values[component + 1] = first * sine + second * cosine;
        }
    }

  return 0;
}

static int normalized_token(FILE *stream, uint64_t data_start,
                            const struct tensor_info_s *embedding,
                            const float *norm_weight, float epsilon,
                            uint32_t token_id, float *input,
                            float *normalized)
{
  uint64_t index;
  double square_sum = 0.0;
  float inverse_rms;

  if (token_id >= embedding->dimensions[1] ||
      data_start > UINT64_MAX - embedding->offset ||
      read_dequantized_row(stream, data_start + embedding->offset,
                           embedding, token_id, input) < 0)
    {
      return -EINVAL;
    }

  for (index = 0; index < embedding->dimensions[0]; index++)
    {
      square_sum += (double)input[index] * input[index];
    }

  inverse_rms = 1.0f /
                sqrtf((float)(square_sum / embedding->dimensions[0]) +
                      epsilon);
  for (index = 0; index < embedding->dimensions[0]; index++)
    {
      normalized[index] = input[index] * inverse_rms * norm_weight[index];
    }

  return 0;
}

static int embedding_pipeline_checkpoint(const char *path,
                                         uint32_t token_id,
                                         int projection_mode,
                                         uint32_t position,
                                         uint32_t second_token_id)
{
  struct tensor_info_s *embedding = NULL;
  struct tensor_info_s *attention_norm = NULL;
  struct tensor_info_s *attention_q = NULL;
  struct tensor_info_s *attention_k = NULL;
  struct tensor_info_s *attention_v = NULL;
  struct tensor_info_s *attention_q_bias = NULL;
  struct tensor_info_s *attention_k_bias = NULL;
  struct tensor_info_s *attention_v_bias = NULL;
  struct tensor_info_s *tensors = NULL;
  FILE *stream = NULL;
  uint64_t metadata_count;
  uint64_t tensor_count;
  uint64_t data_start;
  uint32_t alignment = 32;
  uint32_t version;
  uint32_t magic;
  uint32_t head_count = 0;
  uint32_t head_count_kv = 0;
  uint32_t rotary_dimension = 0;
  float epsilon = 1.0e-6f;
  float frequency_base = 1000000.0f;
  float *input = NULL;
  float *weight = NULL;
  float *normalized = NULL;
  float *q_output = NULL;
  float *k_output = NULL;
  float *v_output = NULL;
  float *second_input = NULL;
  float *second_normalized = NULL;
  float *second_q = NULL;
  float *second_k = NULL;
  float *second_v = NULL;
  float *attention_output = NULL;
  uint64_t index;
  double square_sum = 0.0;
  double output_sum = 0.0;
  double output_square_sum = 0.0;
  float inverse_rms;
  float minimum = 0.0f;
  float maximum = 0.0f;
  int ret = 1;

  stream = fopen(path, "rb");
  if (stream == NULL)
    {
      fprintf(stderr, "aipetllm: embed cannot open %s: %d (%s)\n",
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
      fputs("aipetllm: embed invalid GGUF header\n", stderr);
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
          fputs("aipetllm: embed malformed metadata\n", stderr);
          goto out;
        }

      if (strcmp(key, "general.alignment") == 0 &&
          type == GGUF_TYPE_UINT32)
        {
          if (read_exact(stream, &alignment, sizeof(alignment)) < 0)
            {
              goto out;
            }
        }
      else if (strcmp(key,
                      "qwen2.attention.layer_norm_rms_epsilon") == 0 &&
               type == GGUF_TYPE_FLOAT32)
        {
          if (read_exact(stream, &epsilon, sizeof(epsilon)) < 0)
            {
              goto out;
            }
        }
      else if (strcmp(key, "qwen2.attention.head_count") == 0 &&
               type == GGUF_TYPE_UINT32)
        {
          if (read_exact(stream, &head_count, sizeof(head_count)) < 0)
            {
              goto out;
            }
        }
      else if (strcmp(key, "qwen2.attention.head_count_kv") == 0 &&
               type == GGUF_TYPE_UINT32)
        {
          if (read_exact(stream, &head_count_kv, sizeof(head_count_kv)) < 0)
            {
              goto out;
            }
        }
      else if (strcmp(key, "qwen2.rope.dimension_count") == 0 &&
               type == GGUF_TYPE_UINT32)
        {
          if (read_exact(stream, &rotary_dimension,
                         sizeof(rotary_dimension)) < 0)
            {
              goto out;
            }
        }
      else if (strcmp(key, "qwen2.rope.freq_base") == 0 &&
               type == GGUF_TYPE_FLOAT32)
        {
          if (read_exact(stream, &frequency_base,
                         sizeof(frequency_base)) < 0)
            {
              goto out;
            }
        }
      else if (skip_value(stream, type, 0) < 0)
        {
          goto out;
        }
    }

  if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
      alignment > 4096 || !isfinite(epsilon) ||
      epsilon <= 0.0f || epsilon > 0.01f)
    {
      fprintf(stderr, "aipetllm: embed unsafe alignment/epsilon %" PRIu32
                      "/%.9g\n", alignment, (double)epsilon);
      goto out;
    }

  tensors = calloc((size_t)tensor_count, sizeof(*tensors));
  if (tensors == NULL)
    {
      fputs("aipetllm: embed tensor directory allocation failed\n", stderr);
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
          goto out;
        }

      if (strcmp(tensor->name, "token_embd.weight") == 0)
        {
          embedding = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_norm.weight") == 0)
        {
          attention_norm = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_q.weight") == 0)
        {
          attention_q = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_k.weight") == 0)
        {
          attention_k = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_v.weight") == 0)
        {
          attention_v = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_q.bias") == 0)
        {
          attention_q_bias = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_k.bias") == 0)
        {
          attention_k_bias = tensor;
        }
      else if (strcmp(tensor->name, "blk.0.attn_v.bias") == 0)
        {
          attention_v_bias = tensor;
        }
    }

  {
    long directory_end = ftell(stream);
    if (directory_end < 0)
      {
        goto out;
      }

    data_start = ((uint64_t)directory_end + alignment - 1) &
                 ~(uint64_t)(alignment - 1);
  }

  if (embedding == NULL || attention_norm == NULL ||
      embedding->dimension_count != 2 ||
      attention_norm->dimension_count != 1 ||
      embedding->dimensions[0] == 0 ||
      embedding->dimensions[0] != attention_norm->dimensions[0] ||
      embedding->dimensions[0] > 65536 ||
      token_id >= embedding->dimensions[1] ||
      attention_norm->type != GGML_TYPE_F32 ||
      data_start > (uint64_t)LONG_MAX)
    {
      fputs("aipetllm: embed incompatible embedding/norm tensors\n", stderr);
      goto out;
    }

  if (projection_mode >= 1 &&
      (attention_q == NULL || attention_q->dimension_count != 2 ||
       attention_q->dimensions[0] != embedding->dimensions[0] ||
       attention_q->dimensions[1] == 0 ||
       attention_q->dimensions[1] > 65536 ||
       (attention_q->type != GGML_TYPE_F32 &&
        attention_q->type != GGML_TYPE_Q4_K &&
        attention_q->type != GGML_TYPE_Q6_K) ||
       data_start > UINT64_MAX - attention_q->offset))
    {
      fputs("aipetllm: qproj incompatible blk.0.attn_q.weight\n", stderr);
      goto out;
    }

  if (projection_mode >= 2 &&
      (attention_k == NULL || attention_v == NULL ||
       attention_k->dimension_count != 2 ||
       attention_v->dimension_count != 2 ||
       attention_k->dimensions[0] != embedding->dimensions[0] ||
       attention_v->dimensions[0] != embedding->dimensions[0] ||
       attention_k->dimensions[1] == 0 ||
       attention_k->dimensions[1] != attention_v->dimensions[1] ||
       attention_k->dimensions[1] > 65536 ||
       (attention_k->type != GGML_TYPE_F32 &&
        attention_k->type != GGML_TYPE_Q4_K &&
        attention_k->type != GGML_TYPE_Q6_K) ||
       (attention_v->type != GGML_TYPE_F32 &&
        attention_v->type != GGML_TYPE_Q4_K &&
        attention_v->type != GGML_TYPE_Q6_K) ||
       data_start > UINT64_MAX - attention_k->offset ||
       data_start > UINT64_MAX - attention_v->offset ||
       head_count == 0 || head_count_kv == 0 ||
       attention_q->dimensions[1] % head_count != 0 ||
       attention_k->dimensions[1] % head_count_kv != 0 ||
       attention_q->dimensions[1] / head_count !=
       attention_k->dimensions[1] / head_count_kv))
    {
      fputs("aipetllm: qkv incompatible K/V/GQA metadata\n", stderr);
      goto out;
    }

  if (projection_mode >= 2 && rotary_dimension == 0)
    {
      rotary_dimension = (uint32_t)(attention_q->dimensions[1] /
                                    head_count);
    }

  input = malloc((size_t)embedding->dimensions[0] * sizeof(float));
  weight = malloc((size_t)embedding->dimensions[0] * sizeof(float));
  normalized = malloc((size_t)embedding->dimensions[0] * sizeof(float));
  if (input == NULL || weight == NULL || normalized == NULL)
    {
      fputs("aipetllm: embed vector allocation failed\n", stderr);
      goto out;
    }

  if (read_dequantized_row(stream, data_start + embedding->offset,
                           embedding, token_id, input) < 0 ||
      read_dequantized_row(stream, data_start + attention_norm->offset,
                           attention_norm, 0, weight) < 0)
    {
      fputs("aipetllm: embed tensor row read/dequantize failed\n", stderr);
      goto out;
    }

  for (index = 0; index < embedding->dimensions[0]; index++)
    {
      square_sum += (double)input[index] * input[index];
    }

  inverse_rms = 1.0f /
                sqrtf((float)(square_sum / embedding->dimensions[0]) +
                      epsilon);
  for (index = 0; index < embedding->dimensions[0]; index++)
    {
      float value = input[index] * inverse_rms * weight[index];

      normalized[index] = value;
      output_sum += value;
      output_square_sum += (double)value * value;
      if (index == 0 || value < minimum)
        {
          minimum = value;
        }

      if (index == 0 || value > maximum)
        {
          maximum = value;
        }
    }

  printf("embedding token=%" PRIu32 " hidden=%" PRIu64
         " vocab=%" PRIu64 " type=%s row-bytes=%" PRIu64 "\n",
         token_id, embedding->dimensions[0], embedding->dimensions[1],
         tensor_type_name(embedding->type),
         embedding->bytes / embedding->dimensions[1]);
  printf("rmsnorm tensor=blk.0.attn_norm.weight epsilon=%.9g "
         "input-rms=%.9g inverse-rms=%.9g\n",
         (double)epsilon,
         sqrt(square_sum / embedding->dimensions[0]),
         (double)inverse_rms);
  printf("embedding-crc=%08" PRIx32 " normalized-crc=%08" PRIx32
         " sum=%.9g l2=%.9g min=%.9g max=%.9g\n",
         tensor_crc32(0, (const uint8_t *)input,
                      (size_t)embedding->dimensions[0] * sizeof(float)),
         tensor_crc32(0, (const uint8_t *)normalized,
                      (size_t)embedding->dimensions[0] * sizeof(float)),
         output_sum, sqrt(output_square_sum),
         (double)minimum, (double)maximum);
  printf("normalized[0..7]=");
  for (index = 0; index < embedding->dimensions[0] && index < 8; index++)
    {
      printf("%s%.7g", index == 0 ? "" : ",", (double)normalized[index]);
    }

  putchar('\n');

  if (projection_mode == 0)
    {
      puts("Qwen2 token embedding and layer-0 RMSNorm checkpoint passed; "
           "Q projection pending.");
      ret = 0;
      goto out;
    }

  q_output = malloc((size_t)attention_q->dimensions[1] * sizeof(float));
  if (q_output == NULL)
    {
      fputs("aipetllm: qproj output allocation failed\n", stderr);
      goto out;
    }

  if (read_quantized_matvec(stream, data_start + attention_q->offset,
                            attention_q, normalized, q_output) < 0)
    {
      fputs("aipetllm: qproj matrix-vector execution failed\n", stderr);
      goto out;
    }

  output_sum = 0.0;
  output_square_sum = 0.0;
  for (index = 0; index < attention_q->dimensions[1]; index++)
    {
      float value = q_output[index];

      output_sum += value;
      output_square_sum += (double)value * value;
      if (index == 0 || value < minimum)
        {
          minimum = value;
        }

      if (index == 0 || value > maximum)
        {
          maximum = value;
        }
    }

  printf("qproj tensor=%s type=%s input=%" PRIu64 " output=%" PRIu64
         " row-bytes=%" PRIu64 "\n",
         attention_q->name, tensor_type_name(attention_q->type),
         attention_q->dimensions[0], attention_q->dimensions[1],
         attention_q->bytes / attention_q->dimensions[1]);
  printf("q-crc=%08" PRIx32 " sum=%.9g l2=%.9g min=%.9g max=%.9g\n",
         tensor_crc32(0, (const uint8_t *)q_output,
                      (size_t)attention_q->dimensions[1] * sizeof(float)),
         output_sum, sqrt(output_square_sum),
         (double)minimum, (double)maximum);
  printf("q[0..7]=");
  for (index = 0; index < attention_q->dimensions[1] && index < 8; index++)
    {
      printf("%s%.7g", index == 0 ? "" : ",", (double)q_output[index]);
    }

  putchar('\n');

  if (projection_mode == 1)
    {
      puts("Qwen2 layer-0 Q projection checkpoint passed; "
           "K/V and RoPE pending.");
      ret = 0;
      goto out;
    }

  k_output = malloc((size_t)attention_k->dimensions[1] * sizeof(float));
  v_output = malloc((size_t)attention_v->dimensions[1] * sizeof(float));
  if (k_output == NULL || v_output == NULL)
    {
      fputs("aipetllm: qkv K/V allocation failed\n", stderr);
      goto out;
    }

  if (read_quantized_matvec(stream, data_start + attention_k->offset,
                            attention_k, normalized, k_output) < 0 ||
      read_quantized_matvec(stream, data_start + attention_v->offset,
                            attention_v, normalized, v_output) < 0 ||
      add_f32_bias(stream, data_start, attention_q_bias, q_output,
                   attention_q->dimensions[1]) < 0 ||
      add_f32_bias(stream, data_start, attention_k_bias, k_output,
                   attention_k->dimensions[1]) < 0 ||
      add_f32_bias(stream, data_start, attention_v_bias, v_output,
                   attention_v->dimensions[1]) < 0)
    {
      fputs("aipetllm: qkv projection/bias execution failed\n", stderr);
      goto out;
    }

  printf("qkv heads=%" PRIu32 "/%" PRIu32 " head-dim=%" PRIu64
         " position=%" PRIu32 " rope-dim=%" PRIu32 " base=%.9g\n",
         head_count, head_count_kv,
         attention_q->dimensions[1] / head_count, position,
         rotary_dimension, (double)frequency_base);
  printf("biased-crc q=%08" PRIx32 " k=%08" PRIx32 " v=%08" PRIx32
         " dimensions=%" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
         tensor_crc32(0, (const uint8_t *)q_output,
                      (size_t)attention_q->dimensions[1] * sizeof(float)),
         tensor_crc32(0, (const uint8_t *)k_output,
                      (size_t)attention_k->dimensions[1] * sizeof(float)),
         tensor_crc32(0, (const uint8_t *)v_output,
                      (size_t)attention_v->dimensions[1] * sizeof(float)),
         attention_q->dimensions[1], attention_k->dimensions[1],
         attention_v->dimensions[1]);

  if (apply_normal_rope(q_output, head_count,
                        attention_q->dimensions[1] / head_count,
                        rotary_dimension, position, frequency_base) < 0 ||
      apply_normal_rope(k_output, head_count_kv,
                        attention_k->dimensions[1] / head_count_kv,
                        rotary_dimension, position, frequency_base) < 0)
    {
      fputs("aipetllm: qkv RoPE configuration/execution failed\n", stderr);
      goto out;
    }

  printf("rope-crc q=%08" PRIx32 " k=%08" PRIx32
         " v-unchanged=%08" PRIx32 "\n",
         tensor_crc32(0, (const uint8_t *)q_output,
                      (size_t)attention_q->dimensions[1] * sizeof(float)),
         tensor_crc32(0, (const uint8_t *)k_output,
                      (size_t)attention_k->dimensions[1] * sizeof(float)),
         tensor_crc32(0, (const uint8_t *)v_output,
                      (size_t)attention_v->dimensions[1] * sizeof(float)));
  puts("Qwen2 layer-0 biased Q/K/V and RoPE checkpoint passed; "
       "attention scores and KV cache pending.");

  if (projection_mode == 2)
    {
      ret = 0;
      goto out;
    }

  if (second_token_id >= embedding->dimensions[1])
    {
      fputs("aipetllm: attn2 second token is outside vocabulary\n", stderr);
      goto out;
    }

  second_input = malloc((size_t)embedding->dimensions[0] * sizeof(float));
  second_normalized = malloc((size_t)embedding->dimensions[0] *
                             sizeof(float));
  second_q = malloc((size_t)attention_q->dimensions[1] * sizeof(float));
  second_k = malloc((size_t)attention_k->dimensions[1] * sizeof(float));
  second_v = malloc((size_t)attention_v->dimensions[1] * sizeof(float));
  attention_output = malloc((size_t)attention_q->dimensions[1] *
                            sizeof(float));
  if (second_input == NULL || second_normalized == NULL ||
      second_q == NULL || second_k == NULL || second_v == NULL ||
      attention_output == NULL)
    {
      fputs("aipetllm: attn2 vector allocation failed\n", stderr);
      goto out;
    }

  if (normalized_token(stream, data_start, embedding, weight, epsilon,
                       second_token_id, second_input,
                       second_normalized) < 0 ||
      read_quantized_matvec(stream, data_start + attention_q->offset,
                            attention_q, second_normalized, second_q) < 0 ||
      read_quantized_matvec(stream, data_start + attention_k->offset,
                            attention_k, second_normalized, second_k) < 0 ||
      read_quantized_matvec(stream, data_start + attention_v->offset,
                            attention_v, second_normalized, second_v) < 0 ||
      add_f32_bias(stream, data_start, attention_q_bias, second_q,
                   attention_q->dimensions[1]) < 0 ||
      add_f32_bias(stream, data_start, attention_k_bias, second_k,
                   attention_k->dimensions[1]) < 0 ||
      add_f32_bias(stream, data_start, attention_v_bias, second_v,
                   attention_v->dimensions[1]) < 0 ||
      apply_normal_rope(second_q, head_count,
                        attention_q->dimensions[1] / head_count,
                        rotary_dimension, 1, frequency_base) < 0 ||
      apply_normal_rope(second_k, head_count_kv,
                        attention_k->dimensions[1] / head_count_kv,
                        rotary_dimension, 1, frequency_base) < 0)
    {
      fputs("aipetllm: attn2 second-token QKV execution failed\n", stderr);
      goto out;
    }

  {
    uint64_t head_dimension = attention_q->dimensions[1] / head_count;
    uint32_t query_per_kv = head_count / head_count_kv;
    float score_values[64];
    float probability_values[64];
    float scale = 1.0f / sqrtf((float)head_dimension);
    double context_sum = 0.0;
    double context_square_sum = 0.0;
    float context_minimum = 0.0f;
    float context_maximum = 0.0f;
    uint32_t head;

    if (head_count > 32 || head_count % head_count_kv != 0)
      {
        fputs("aipetllm: attn2 unsupported GQA head mapping\n", stderr);
        goto out;
      }

    for (head = 0; head < head_count; head++)
      {
        uint32_t kv_head = head / query_per_kv;
        uint64_t query_base = (uint64_t)head * head_dimension;
        uint64_t kv_base = (uint64_t)kv_head * head_dimension;
        double score0 = 0.0;
        double score1 = 0.0;
        float maximum_score;
        float exponent0;
        float exponent1;
        float denominator;
        uint64_t component;

        for (component = 0; component < head_dimension; component++)
          {
            float query = second_q[query_base + component];

            score0 += (double)query * k_output[kv_base + component];
            score1 += (double)query * second_k[kv_base + component];
          }

        score_values[head * 2] = (float)score0 * scale;
        score_values[head * 2 + 1] = (float)score1 * scale;
        maximum_score = fmaxf(score_values[head * 2],
                              score_values[head * 2 + 1]);
        exponent0 = expf(score_values[head * 2] - maximum_score);
        exponent1 = expf(score_values[head * 2 + 1] - maximum_score);
        denominator = exponent0 + exponent1;
        probability_values[head * 2] = exponent0 / denominator;
        probability_values[head * 2 + 1] = exponent1 / denominator;

        for (component = 0; component < head_dimension; component++)
          {
            uint64_t output_index = query_base + component;
            float value = probability_values[head * 2] *
                          v_output[kv_base + component] +
                          probability_values[head * 2 + 1] *
                          second_v[kv_base + component];

            attention_output[output_index] = value;
            context_sum += value;
            context_square_sum += (double)value * value;
            if (output_index == 0 || value < context_minimum)
              {
                context_minimum = value;
              }

            if (output_index == 0 || value > context_maximum)
              {
                context_maximum = value;
              }
          }
      }

    printf("kv-cache tokens=2 kv-heads=%" PRIu32 " head-dim=%" PRIu64
           " k-crc=%08" PRIx32 "/%08" PRIx32
           " v-crc=%08" PRIx32 "/%08" PRIx32 "\n",
           head_count_kv, head_dimension,
           tensor_crc32(0, (const uint8_t *)k_output,
                        (size_t)attention_k->dimensions[1] * sizeof(float)),
           tensor_crc32(0, (const uint8_t *)second_k,
                        (size_t)attention_k->dimensions[1] * sizeof(float)),
           tensor_crc32(0, (const uint8_t *)v_output,
                        (size_t)attention_v->dimensions[1] * sizeof(float)),
           tensor_crc32(0, (const uint8_t *)second_v,
                        (size_t)attention_v->dimensions[1] * sizeof(float)));
    printf("attention query-position=1 causal-keys=2 group=%" PRIu32
           " scale=%.9g score-crc=%08" PRIx32
           " probability-crc=%08" PRIx32 "\n",
           query_per_kv, (double)scale,
           tensor_crc32(0, (const uint8_t *)score_values,
                        (size_t)head_count * 2 * sizeof(float)),
           tensor_crc32(0, (const uint8_t *)probability_values,
                        (size_t)head_count * 2 * sizeof(float)));
    for (head = 0; head < head_count && head < 3; head++)
      {
        printf("head[%" PRIu32 "] scores=%.7g,%.7g probs=%.7g,%.7g\n",
               head, (double)score_values[head * 2],
               (double)score_values[head * 2 + 1],
               (double)probability_values[head * 2],
               (double)probability_values[head * 2 + 1]);
      }

    printf("context-crc=%08" PRIx32
           " sum=%.9g l2=%.9g min=%.9g max=%.9g\n",
           tensor_crc32(0, (const uint8_t *)attention_output,
                        (size_t)attention_q->dimensions[1] * sizeof(float)),
           context_sum, sqrt(context_square_sum),
           (double)context_minimum, (double)context_maximum);
  }

  puts("Qwen2 two-token causal GQA/KV-cache checkpoint passed; "
       "attention output projection pending.");
  ret = 0;

out:
  free(attention_output);
  free(second_v);
  free(second_k);
  free(second_q);
  free(second_normalized);
  free(second_input);
  free(v_output);
  free(k_output);
  free(q_output);
  free(normalized);
  free(weight);
  free(input);
  free(tensors);
  if (stream != NULL)
    {
      fclose(stream);
    }

  return ret;
}

int aipetllm_embedding_checkpoint(const char *path, uint32_t token_id)
{
  return embedding_pipeline_checkpoint(path, token_id, 0, 0, 0);
}

int aipetllm_q_projection_checkpoint(const char *path, uint32_t token_id)
{
  return embedding_pipeline_checkpoint(path, token_id, 1, 0, 0);
}

int aipetllm_qkv_checkpoint(const char *path, uint32_t token_id,
                            uint32_t position)
{
  return embedding_pipeline_checkpoint(path, token_id, 2, position, 0);
}

int aipetllm_attention2_checkpoint(const char *path, uint32_t first_token,
                                   uint32_t second_token)
{
  return embedding_pipeline_checkpoint(path, first_token, 3, 0,
                                       second_token);
}
