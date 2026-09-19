# Contest 2026 team 349 launcher
if(CONFIG_LVX_USE_DEMO_CONTEST2026_349_LAUNCHER)
  nuttx_add_application(
    NAME launcher
    SRCS launcher_main.c
    STACKSIZE 32768)
endif()
