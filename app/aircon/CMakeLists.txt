# app/aircon/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_AIRCON)
  nuttx_add_application(
    NAME aircon
    SRCS aircon_main.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_ui
                        ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_miio
                        ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_mihome
    STACKSIZE 16384)
endif()
