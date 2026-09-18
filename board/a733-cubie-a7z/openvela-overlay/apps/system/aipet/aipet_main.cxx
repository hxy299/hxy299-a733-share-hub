#include "agent_bridge.h"
#include "pet_core.hxx"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

extern "C" int aipet_main(int argc, char **argv)
{
  if (argc == 2 && !std::strcmp(argv[1], "status"))
    { aipet_startup_status(); return 0; }
  if (argc == 2 && !std::strcmp(argv[1], "start"))
    return aipet_startup(0);
  if (argc == 2 && !std::strcmp(argv[1], "--boot"))
    return aipet_startup(1);
  if (argc == 2 && !std::strcmp(argv[1], "cancel"))
    { aipet_agent_cancel(); return 0; }
  if (argc != 3 || std::strcmp(argv[1], "ask"))
    {
      std::printf("aipet ask \"text\" | aipet cancel\n"
                  "aipet start | status; service agent on|off|status\n"
                  "Boot initialization waits for network/time/config.\n"
                  "Online text integration; microphone/UART output pending.\n");
      return 1;
    }
  int status = aipet_agent_submit(argv[2], 90000);
  if (status)
    { std::printf("aipet: submit failed %d\n", status); return 1; }
  char raw[4097];
  uint64_t elapsed = 0;
  do
    {
      status = aipet_agent_poll(raw, sizeof(raw), &elapsed);
      if (status == -EAGAIN) usleep(10000);
    }
  while (status == -EAGAIN);
  if (status)
    { std::printf("aipet: reply failed %d\n", status); return 1; }
  aipet::Reply reply;
  if (!aipet::parse_reply(raw, reply))
    { std::printf("aipet: invalid reply\n"); return 1; }
  std::printf("%s\n[emotion=%s actions=%zu elapsed=%llu ms]\n",
              reply.text.c_str(), reply.emotion.c_str(), reply.actions.size(),
              (unsigned long long)elapsed);
  /* Never execute parsed actions before numeric validation/hardware binding. */
  return 0;
}
