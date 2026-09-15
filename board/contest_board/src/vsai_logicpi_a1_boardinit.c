/****************************************************************************
 * vendor/openvela/boards/contest2026_349_board/src/vsai_logicpi_a1_boardinit.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/fs/fs.h>

#ifdef CONFIG_S6_MIPI_DSI
#  include <nuttx/video/fb.h>
#endif

void s6_board_initialize(void)
{
  /* DDR, clocks, pin mux and UART are initialized by BL31/U-Boot. */
}

#ifdef CONFIG_BOARDCTL
int board_app_initialize(uintptr_t arg)
{
  UNUSED(arg);
  return OK;
}
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret;

  #ifdef CONFIG_FS_PROCFS
    ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
    if (ret < 0)
      {
        syslog(LOG_ERR, "ERROR: Failed to mount procfs at /proc: %d\n", ret);
      }
  #endif

  #ifdef CONFIG_S6_MIPI_DSI
    ret = fb_register(0, 0);
    if (ret < 0)
      {
        syslog(LOG_ERR, "ERROR: fb_register() failed: %d\n", ret);
      }
  #endif
}
#endif
