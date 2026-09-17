#include "agent_bridge.h"
#include "core/message_bus.h"
#include "core/message_bus_tap.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static agent_msg_t captured;
static int reject;
int message_bus_push_inbound(const agent_msg_t *msg)
{
  if (reject) return -1;
  captured = *msg;
  free(captured.content);
  captured.content = NULL;
  return 0;
}
int main(void)
{
  char text[4097];
  agent_msg_t old;
  uint64_t elapsed;
  assert(aipet_agent_submit("hello", 100) == -ENODEV);
  assert(aipet_agent_attach() == 0);
  assert(aipet_agent_submit("hello", 1000) == 0);
  assert(aipet_agent_submit("second", 1000) == -EBUSY);
  assert(aipet_agent_poll(text, sizeof(text), NULL) == -EAGAIN);
  captured.content = "[开心]Hello";
  assert(mbus_tap_try_deliver(&captured));
  assert(aipet_agent_poll(text, 2, NULL) == -ENOSPC);
  assert(aipet_agent_poll(text, sizeof(text), &elapsed) == 0);
  assert(!strcmp(text, "[开心]Hello"));
  assert(aipet_agent_submit("backend failure", 1000) == 0);
  captured.content = "[AIPET_ERROR]backend";
  assert(mbus_tap_try_deliver(&captured));
  assert(aipet_agent_poll(text, sizeof(text), NULL) == -EIO);
  old = captured;
  assert(aipet_agent_submit("next", 1000) == 0);
  assert(mbus_tap_try_deliver(&old));
  assert(aipet_agent_poll(text, sizeof(text), NULL) == -EAGAIN);
  aipet_agent_cancel();
  assert(aipet_agent_poll(text, sizeof(text), NULL) == -ECANCELED);
  reject = 1;
  assert(aipet_agent_submit("rejected", 1000) == -EAGAIN);
  reject = 0;
  assert(aipet_agent_submit("timeout", 100) == 0);
  usleep(120000);
  assert(aipet_agent_poll(text, sizeof(text), NULL) == -ETIMEDOUT);
  aipet_agent_detach();
  assert(!mbus_tap_try_deliver(&captured));
  return 0;
}
