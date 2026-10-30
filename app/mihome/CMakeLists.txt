# app/mihome/CMakeLists.txt
if(CONFIG_LVX_MIHOME)
  nuttx_add_library(lvx_mihome)
  target_sources(lvx_mihome PRIVATE mihome.c mihome_mock.c)
  target_include_directories(
    lvx_mihome PUBLIC ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_miio)

  nuttx_add_application(
    NAME miio
    SRCS mihome_main.c
    INCLUDE_DIRECTORIES ${NUTTX_APPS_DIR}/packages/demos/contest2026_349_miio
    STACKSIZE 8192)
endif()
