if(CONFIG_EXAMPLES_AI_AGENT_VELA)
  nuttx_add_application(
    NAME aipet
    STACKSIZE 16384
    PRIORITY 100
    SRCS aipet_main.cxx pet_core.cxx agent_startup.c
    INCLUDE_DIRECTORIES
      ${CMAKE_SOURCE_DIR}/../packages/ai_agent/src
      ${CMAKE_SOURCE_DIR}/../packages/ai_agent/include)
endif()
