############################################################################
# vendor/openvela/boards/contest2026_349_board/src/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(TOPDIR)/Make.defs

CSRCS = vsai_logicpi_a1_boardinit.c

ifeq ($(CONFIG_VSAI_LOGICPI_A1_TOUCH),y)
CSRCS += vsai_logicpi_a1_touch.c
endif

include $(TOPDIR)/boards/Board.mk
