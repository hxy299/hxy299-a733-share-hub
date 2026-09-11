# ##############################################################################
# apps/system/aipetllm/CMakeLists.txt
# ##############################################################################

if(CONFIG_SYSTEM_AIPETLLM)
  nuttx_add_application(
    NAME
    ${CONFIG_AIPETLLM_PROGNAME}
    SRCS
    aipetllm_main.c
    aipetllm_cxx.cxx
    STACKSIZE
    ${CONFIG_AIPETLLM_STACKSIZE}
    PRIORITY
    ${CONFIG_AIPETLLM_PRIORITY})
endif()
