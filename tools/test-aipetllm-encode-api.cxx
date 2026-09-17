/* SPDX-License-Identifier: Apache-2.0 */
/* Host-only regression for the bounded encode API; no board inference claims. */
#include <cstdint>
#include <cstdio>
extern "C" int aipetllm_encode_tokens(const char *, const char *,
                                      std::uint32_t *, std::uint32_t,
                                      std::uint32_t *);
int main(int argc, char **argv)
{
  std::uint32_t output[64] = {};
  std::uint32_t count = 0;
  if (argc != 2)
    {
      return 2;
    }

  if (aipetllm_encode_tokens(argv[1], "Hello world", output, 64, &count) != 0 ||
      count != 2 || output[0] != 9707 || output[1] != 1879)
    {
      return 1;
    }

  if (aipetllm_encode_tokens(argv[1], "Hello world", output, 1, &count) == 0 ||
      aipetllm_encode_tokens(argv[1], "", output, 64, &count) == 0 ||
      aipetllm_encode_tokens(argv[1], "x", nullptr, 64, &count) == 0)
    {
      return 1;
    }

  std::puts("bounded encode API regression passed");
  return 0;
}
