/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AIPETLLM_SEQUENCE_H
#define AIPETLLM_SEQUENCE_H
#include <stdint.h>
#define AIPETLLM_SEQUENCE_LIMIT 64
struct aipetllm_sequence_s
{
  uint32_t input[64];
  uint32_t input_count;
  uint32_t generate_limit;
  uint32_t output[64];
  uint32_t output_count;
  uint32_t eos;
};
int aipetllm_sequence_ram_checkpoint(const char *path,
                                    struct aipetllm_sequence_s *sequence,
                                    unsigned int mask);
int aipetllm_sequence_fast_checkpoint(const char *path,
                                     struct aipetllm_sequence_s *sequence,
                                     unsigned int mask);
#endif
