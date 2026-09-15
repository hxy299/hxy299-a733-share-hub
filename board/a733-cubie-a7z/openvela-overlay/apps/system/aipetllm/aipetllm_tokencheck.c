/****************************************************************************
 * apps/system/aipetllm/aipetllm_tokencheck.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#define TOKEN_KEY_MAX 256
#define TOKEN_VALUE_MAX 64
#define TOKEN_LENGTH_LIMIT (1024 * 1024)

struct tokenizer_info_s
{
  char model[TOKEN_VALUE_MAX];
  char pre[TOKEN_VALUE_MAX];
  uint64_t tokens;
  uint64_t merges;
  uint64_t token_types;
  uint64_t scores;
  uint64_t token_bytes;
  uint64_t merge_bytes;
  uint64_t max_token;
  uint64_t max_merge;
  uint64_t bos;
  uint64_t eos;
  uint64_t eot;
  uint64_t pad;
  int add_bos;
  int add_eos;
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
      int ret = skip_value(stream, element_type, depth + 1);

      if (ret < 0)
        {
          return ret;
        }
    }

  return 0;
}

static int scan_string_array(FILE *stream, uint64_t *count,
                             uint64_t *total, uint64_t *maximum)
{
  uint32_t element_type;
  uint64_t entries;
  uint64_t index;

  if (read_exact(stream, &element_type, sizeof(element_type)) < 0 ||
      read_exact(stream, &entries, sizeof(entries)) < 0 ||
      element_type != GGUF_TYPE_STRING)
    {
      return -EINVAL;
    }

  for (index = 0; index < entries; index++)
    {
      uint64_t length;

      if (read_exact(stream, &length, sizeof(length)) < 0 ||
          length > TOKEN_LENGTH_LIMIT ||
          *total > UINT64_MAX - length ||
          skip_exact(stream, length) < 0)
        {
          return -EINVAL;
        }

      *total += length;
      if (length > *maximum)
        {
          *maximum = length;
        }
    }

  *count = entries;
  return 0;
}

static int scan_scalar_array(FILE *stream, uint32_t expected,
                             uint64_t *count)
{
  uint32_t element_type;
  uint64_t entries;
  size_t size;

  if (read_exact(stream, &element_type, sizeof(element_type)) < 0 ||
      read_exact(stream, &entries, sizeof(entries)) < 0 ||
      element_type != expected)
    {
      return -EINVAL;
    }

  size = scalar_size(element_type);
  if (size == 0 || entries > UINT64_MAX / size ||
      skip_exact(stream, entries * size) < 0)
    {
      return -EINVAL;
    }

  *count = entries;
  return 0;
}

static int read_token_id(FILE *stream, uint32_t type, uint64_t *value)
{
  uint64_t u64;
  int64_t i64;
  uint32_t u32;
  int32_t i32;

  switch (type)
    {
      case GGUF_TYPE_UINT32:
        if (read_exact(stream, &u32, sizeof(u32)) < 0) return -EIO;
        *value = u32;
        return 0;
      case GGUF_TYPE_INT32:
        if (read_exact(stream, &i32, sizeof(i32)) < 0 || i32 < 0)
          {
            return -EINVAL;
          }

        *value = (uint32_t)i32;
        return 0;
      case GGUF_TYPE_UINT64:
        if (read_exact(stream, &u64, sizeof(u64)) < 0) return -EIO;
        *value = u64;
        return 0;
      case GGUF_TYPE_INT64:
        if (read_exact(stream, &i64, sizeof(i64)) < 0 || i64 < 0)
          {
            return -EINVAL;
          }

        *value = (uint64_t)i64;
        return 0;
      default:
        return -EINVAL;
    }
}

static int read_bool(FILE *stream, uint32_t type, int *value)
{
  uint8_t flag;

  if (type != GGUF_TYPE_BOOL || read_exact(stream, &flag, sizeof(flag)) < 0 ||
      flag > 1)
    {
      return -EINVAL;
    }

  *value = flag;
  return 0;
}

static int valid_token_id(uint64_t id, uint64_t count)
{
  return id == UINT64_MAX || id < count;
}

int aipetllm_tokenizer_checkpoint(const char *path)
{
  struct tokenizer_info_s info;
  FILE *stream;
  uint64_t tensor_count;
  uint64_t metadata_count;
  uint32_t version;
  uint32_t magic;
  uint64_t index;
  int ret = 1;

  memset(&info, 0, sizeof(info));
  info.bos = UINT64_MAX;
  info.eos = UINT64_MAX;
  info.eot = UINT64_MAX;
  info.pad = UINT64_MAX;
  info.add_bos = -1;
  info.add_eos = -1;

  stream = fopen(path, "rb");
  if (stream == NULL)
    {
      fprintf(stderr, "aipetllm: tokencheck cannot open %s: %d (%s)\n",
              path, errno, strerror(errno));
      return 1;
    }

  if (read_exact(stream, &magic, sizeof(magic)) < 0 ||
      read_exact(stream, &version, sizeof(version)) < 0 ||
      read_exact(stream, &tensor_count, sizeof(tensor_count)) < 0 ||
      read_exact(stream, &metadata_count, sizeof(metadata_count)) < 0 ||
      magic != GGUF_MAGIC || version < 2 || version > 3)
    {
      fputs("aipetllm: tokencheck invalid GGUF header\n", stderr);
      goto out;
    }

  for (index = 0; index < metadata_count; index++)
    {
      char key[TOKEN_KEY_MAX];
      uint64_t key_length;
      uint32_t type;
      int result;

      if (read_string(stream, key, sizeof(key), &key_length) < 0 ||
          key_length >= sizeof(key) ||
          read_exact(stream, &type, sizeof(type)) < 0)
        {
          fputs("aipetllm: tokencheck malformed metadata\n", stderr);
          goto out;
        }

      if (strcmp(key, "tokenizer.ggml.model") == 0 &&
          type == GGUF_TYPE_STRING)
        {
          result = read_string(stream, info.model, sizeof(info.model), NULL);
        }
      else if (strcmp(key, "tokenizer.ggml.pre") == 0 &&
               type == GGUF_TYPE_STRING)
        {
          result = read_string(stream, info.pre, sizeof(info.pre), NULL);
        }
      else if (strcmp(key, "tokenizer.ggml.tokens") == 0 &&
               type == GGUF_TYPE_ARRAY)
        {
          result = scan_string_array(stream, &info.tokens, &info.token_bytes,
                                     &info.max_token);
        }
      else if (strcmp(key, "tokenizer.ggml.merges") == 0 &&
               type == GGUF_TYPE_ARRAY)
        {
          result = scan_string_array(stream, &info.merges, &info.merge_bytes,
                                     &info.max_merge);
        }
      else if (strcmp(key, "tokenizer.ggml.token_type") == 0 &&
               type == GGUF_TYPE_ARRAY)
        {
          result = scan_scalar_array(stream, GGUF_TYPE_INT32,
                                     &info.token_types);
        }
      else if (strcmp(key, "tokenizer.ggml.scores") == 0 &&
               type == GGUF_TYPE_ARRAY)
        {
          result = scan_scalar_array(stream, GGUF_TYPE_FLOAT32,
                                     &info.scores);
        }
      else if (strcmp(key, "tokenizer.ggml.bos_token_id") == 0)
        {
          result = read_token_id(stream, type, &info.bos);
        }
      else if (strcmp(key, "tokenizer.ggml.eos_token_id") == 0)
        {
          result = read_token_id(stream, type, &info.eos);
        }
      else if (strcmp(key, "tokenizer.ggml.eot_token_id") == 0)
        {
          result = read_token_id(stream, type, &info.eot);
        }
      else if (strcmp(key, "tokenizer.ggml.padding_token_id") == 0)
        {
          result = read_token_id(stream, type, &info.pad);
        }
      else if (strcmp(key, "tokenizer.ggml.add_bos_token") == 0)
        {
          result = read_bool(stream, type, &info.add_bos);
        }
      else if (strcmp(key, "tokenizer.ggml.add_eos_token") == 0)
        {
          result = read_bool(stream, type, &info.add_eos);
        }
      else
        {
          result = skip_value(stream, type, 0);
        }

      if (result < 0)
        {
          fprintf(stderr, "aipetllm: tokencheck invalid key=%s type=%" PRIu32
                          " (%d)\n", key, type, result);
          goto out;
        }
    }

  printf("tokenizer-check path=%s version=%" PRIu32
         " tensors=%" PRIu64 " metadata=%" PRIu64 "\n",
         path, version, tensor_count, metadata_count);
  printf("tokenizer model='%s' pre='%s' vocab=%" PRIu64
         " merges=%" PRIu64 " token-types=%" PRIu64
         " scores=%" PRIu64 "\n",
         info.model, info.pre, info.tokens, info.merges,
         info.token_types, info.scores);
  printf("token-bytes=%" PRIu64 " max-token=%" PRIu64
         " merge-bytes=%" PRIu64 " max-merge=%" PRIu64 "\n",
         info.token_bytes, info.max_token, info.merge_bytes, info.max_merge);
  printf("special bos=%" PRIu64 " eos=%" PRIu64 " eot=%" PRIu64
         " pad=%" PRIu64 " add-bos=%d add-eos=%d\n",
         info.bos, info.eos, info.eot, info.pad,
         info.add_bos, info.add_eos);

  if (strcmp(info.model, "gpt2") != 0 || strcmp(info.pre, "qwen2") != 0 ||
      info.tokens == 0 || info.merges == 0 || info.eos == UINT64_MAX ||
      (info.token_types != 0 && info.token_types != info.tokens) ||
      (info.scores != 0 && info.scores != info.tokens) ||
      !valid_token_id(info.bos, info.tokens) ||
      !valid_token_id(info.eos, info.tokens) ||
      !valid_token_id(info.eot, info.tokens) ||
      !valid_token_id(info.pad, info.tokens))
    {
      fputs("aipetllm: Qwen2 BPE tokenizer assets are incomplete/unsafe\n",
            stderr);
      goto out;
    }

  puts("Qwen2 BPE tokenizer asset checkpoint passed; tokenization is pending.");
  ret = 0;

out:
  fclose(stream);
  return ret;
}

#ifdef AIPETLLM_TOKENCHECK_STANDALONE
int main(int argc, char **argv)
{
  if (argc != 2)
    {
      fprintf(stderr, "usage: %s MODEL.gguf\n", argv[0]);
      return 2;
    }

  return aipetllm_tokenizer_checkpoint(argv[1]);
}
#endif
