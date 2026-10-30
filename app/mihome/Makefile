############################################################################
# app/mihome/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = miio
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 8192
MODULE    = $(CONFIG_LVX_MIHOME)

CSRCS   = mihome.c mihome_mock.c
MAINSRC = mihome_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_miio

include $(APPDIR)/Application.mk
