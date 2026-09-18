/****************************************************************************
 * vendor/openvela/boards/contest2026_349_board/include/board.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __BOARDS_CONTEST2026_349_BOARD_INCLUDE_BOARD_H
#define __BOARDS_CONTEST2026_349_BOARD_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_VSAI_LOGICPI_A1_TOUCH
#  include <nuttx/i2c/i2c_master.h>
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

void s6_board_initialize(void);

#ifdef CONFIG_VSAI_LOGICPI_A1_TOUCH
int vsai_logicpi_a1_touch_register(FAR const char *devpath,
                                   FAR struct i2c_master_s *i2c);
#endif

#endif /* __ASSEMBLY__ */

#endif /* __BOARDS_CONTEST2026_349_BOARD_INCLUDE_BOARD_H */
