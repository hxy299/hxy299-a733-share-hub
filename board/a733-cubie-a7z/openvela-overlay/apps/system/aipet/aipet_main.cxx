#include "agent_bridge.h"
#include "agent_secret.h"
#include "pet_core.hxx"
#include "pet_routes.hxx"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

extern "C" int aipet_main(int argc, char **argv)
{
  if (argc == 2 && (!std::strcmp(argv[1], "init") || !std::strcmp(argv[1], "setup")))
    return aipet_setup();
  if (argc == 2 && !std::strcmp(argv[1], "status"))
    { aipet_startup_status(); return 0; }
  if (argc == 2 && !std::strcmp(argv[1], "start"))
    return aipet_startup(0);
  if (argc == 2 && !std::strcmp(argv[1], "--boot"))
    return aipet_startup(1);
  if (argc == 2 && !std::strcmp(argv[1], "cancel"))
    { aipet_agent_cancel(); return 0; }
  if (argc == 3 && !std::strcmp(argv[1], "route"))
    return aipet::routed_ask(argv[2], true, false);
  if (argc == 3 && !std::strcmp(argv[1], "cloud"))
    return aipet::routed_ask(argv[2], false, true);
  if (argc == 3 && !std::strcmp(argv[1], "tts"))
    return aipet::uart_tts_control(argv[2]);
  if (argc == 4 && !std::strcmp(argv[1], "tts") &&
      !std::strcmp(argv[2], "test"))
    return aipet::uart_tts_control(argv[2], argv[3]);
  if (argc != 3 || std::strcmp(argv[1], "ask"))
    {
      std::printf("aipet init (one-time online setup via serial/SSH)\n"
                  "aipet ask \"text\" | aipet cancel\n"
                  "aipet route \"text\" (inspect only) | cloud \"text\" (bypass rules)\n"
                  "aipet start | status; service agent on|off|status\n"
                  "aipet tts status|on|off|test \"text\"\n"
                  "Boot initialization waits for network/time/config.\n"
                  "Online Agent with optional UART4 TW-TTS output.\n");
      return 1;
    }
  return aipet::routed_ask(argv[2], false, false);
}
