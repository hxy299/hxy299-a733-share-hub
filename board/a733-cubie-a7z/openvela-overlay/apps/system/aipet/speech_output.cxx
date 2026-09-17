/* SPDX-License-Identifier: Apache-2.0 */
#include "speech_output.hxx"
#include <cerrno>
#include <new>

namespace aipet
{
static bool valid_text(const std::string &text)
{
  return !text.empty() && text.size() <= 4096 &&
         text.find('\0') == std::string::npos;
}

int UartSpeechOutput::speak(const std::string &utf8)
{
  if (!valid_text(utf8)) return -EINVAL;
  if (!convert_) return -ENOSYS;
  std::string encoded;
  if (!convert_(utf8, encoded)) return -EILSEQ;
  if (port_.speak_gb2312(encoded, 5, 5, 5)) return 0;
  const int error = port_.status().last_errno;
  return -(error ? error : EIO);
}

int UartSpeechOutput::stop() { return -ENOSYS; }

int PcmSpeechOutput::speak(const std::string &text)
{
  if (!valid_text(text)) return -EINVAL;
  if (!ops_.synthesize || !ops_.play) return -ENODEV;
  /* Reserved bounded whole-utterance path. Long or streaming audio needs a
   * later chunked adapter, not silent truncation to this buffer. */
  try
    {
      std::vector<unsigned char> pcm(320000); /* 10 s at 16kHz/s16/mono */
      std::size_t length = 0;
      int result = ops_.synthesize(ops_.context, text.c_str(), pcm.data(),
                                   pcm.size(), &length);
      if (result != 0) return result;
      if (!length || length > pcm.size() || (length & 1)) return -EMSGSIZE;
      return ops_.play(ops_.context, pcm.data(), length, 16000, 1, 16);
    }
  catch (const std::bad_alloc &) { return -ENOMEM; }
}

int PcmSpeechOutput::stop()
{
  return ops_.stop ? ops_.stop(ops_.context) : -ENODEV;
}
}
