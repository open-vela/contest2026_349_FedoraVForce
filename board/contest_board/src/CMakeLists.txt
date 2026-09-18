# ##############################################################################
# vendor/openvela/boards/contest2026_349_board/src/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# ##############################################################################

set(SRCS vsai_logicpi_a1_boardinit.c)

if(CONFIG_VSAI_LOGICPI_A1_TOUCH)
  list(APPEND SRCS vsai_logicpi_a1_touch.c)
endif()

target_sources(board PRIVATE ${SRCS})
