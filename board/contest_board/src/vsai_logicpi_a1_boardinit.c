/****************************************************************************
 * vendor/openvela/boards/contest2026_349_board/src/vsai_logicpi_a1_boardinit.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <nuttx/board.h>

#include <syslog.h>

#include <arch/board/board.h>

#ifdef CONFIG_FS_PROCFS
#  include <nuttx/fs/fs.h>
#endif

#ifdef CONFIG_S6_MIPI_DSI
#  include <nuttx/video/fb.h>
#endif

#ifdef CONFIG_S6_I2C
#  include "s6_i2c.h"
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void s6_board_initialize(void)
{
  /* DDR, clocks, pin mux and UART are initialized by BL31/U-Boot. */
}

/* This tree's NSH enables CONFIG_BOARDCTL (mkrd), and boards/boardctl.c
 * references board_app_initialize() unconditionally.
 */

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
  int ret = OK;

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

#if defined(CONFIG_S6_I2C)
  struct i2c_master_s *i2c = s6_i2cbus_initialize();

#ifdef CONFIG_I2C_DRIVER
  ret = i2c_register(i2c, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: i2c_register(0) failed: %d\n", ret);
    }
#endif
#endif

  UNUSED(ret);
}
#endif
