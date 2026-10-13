# app/bench/CMakeLists.txt
if(CONFIG_LVX_USE_DEMO_BENCH)
  nuttx_add_application(
    NAME bench
    SRCS bench_main.c
    STACKSIZE 32768)
endif()
