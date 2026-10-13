# app/paint/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_PAINT)
  nuttx_add_application(
    NAME paint
    SRCS paint_main.c
    STACKSIZE 32768)
endif()
