# app/launcher/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_LAUNCHER)
  nuttx_add_application(
    NAME launcher
    SRCS launcher_main.c
    STACKSIZE 32768)
endif()
