# app/miio/CMakeLists.txt
if(CONFIG_LVX_MIIO)
  nuttx_add_library(lvx_miio)
  target_sources(lvx_miio PRIVATE miio.c)
  target_link_libraries(lvx_miio PRIVATE mbedtls)
endif()
