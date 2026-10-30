############################################################################
# app/aircon/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = aircon
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_AIRCON)

MAINSRC = aircon_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_ui
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_miio
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_349_mihome

include $(APPDIR)/Application.mk
