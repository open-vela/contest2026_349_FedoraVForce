/****************************************************************************
 * vendor/openvela/boards/contest2026_349_board/src/vsai_logicpi_a1_touch.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>

#include <debug.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/signal.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/gt9xx.h>

#include "s6_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Goodix GT911 on the ATK-MD0550 panel, wired per the J6 LCD connector:
 *   GPIOA_0  I2C0 SDA (3.3V pull-up)
 *   GPIOA_1  I2C0 SCL (3.3V pull-up)
 *   GPIOA_9  touch reset, active low
 *   GPIOA_10 touch interrupt
 */

#define CTP_I2C_ADDR         0x5d

#define CTP_RST_PIN          9
#define CTP_INT_PIN          10

#define CTP_INT_GPIO         S6_GPIOA(CTP_INT_PIN)
#define CTP_INT_CHANNEL      0

#define CTP_IRQ              S6_GPIOINT_IRQ(CTP_INT_CHANNEL)

/* Reset timing from the Goodix GT911 datasheet / mainline goodix.c */

#define CTP_RST_HOLD_MS      20     /* T2: > 10ms with reset asserted     */
#define CTP_ADDR_SETUP_US    100    /* T3: > 100us INT level before reset */
#define CTP_RST_RELEASE_MS   10     /* T4: > 5ms after reset deasserted   */
#define CTP_INT_SYNC_MS      50     /* T5: INT low pulse after reset      */

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gt9xx_irq_attach(const struct gt9xx_board_s *state,
                            xcpt_t isr, void *arg);
static void gt9xx_irq_enable(const struct gt9xx_board_s *state, bool enable);
static int gt9xx_set_power(const struct gt9xx_board_s *state, bool on);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Callback for Board-Specific Operations */

static const struct gt9xx_board_s g_vsai_logicpi_a1_gt9xx =
{
  .irq_attach = gt9xx_irq_attach,
  .irq_enable = gt9xx_irq_enable,
  .set_power  = gt9xx_set_power
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gt911_reset
 *
 * Description:
 *   Run the GT911 power-on reset sequence.  INT held low during reset
 *   selects the 0x5d I2C address.
 *
 ****************************************************************************/

static int gt911_reset(void)
{
  int ret;

  ret = s6_gpioa_config_output(CTP_RST_PIN, false);
  if (ret < 0)
    {
      return ret;
    }

  nxsig_usleep(CTP_RST_HOLD_MS * 1000);

  ret = s6_gpioa_config_output(CTP_INT_PIN, false);
  if (ret < 0)
    {
      return ret;
    }

  nxsig_usleep(CTP_ADDR_SETUP_US);

  s6_gpioa_write(CTP_RST_PIN, true);
  nxsig_usleep(CTP_RST_RELEASE_MS * 1000);

  nxsig_usleep(CTP_INT_SYNC_MS * 1000);

  return s6_gpioa_config_input(CTP_INT_PIN);
}

/****************************************************************************
 * Name: gt9xx_irq_attach
 *
 * Description:
 *   Attach the Touch Panel Interrupt Handler and arm the gpio_intc
 *   channel for a falling edge on GPIOA_10.
 *
 ****************************************************************************/

static int gt9xx_irq_attach(const struct gt9xx_board_s *state,
                            xcpt_t isr, void *arg)
{
  int ret;

  iinfo("\n");

  ret = irq_attach(CTP_IRQ, isr, arg);
  if (ret < 0)
    {
      ierr("irq_attach failed: %d\n", ret);
      return ret;
    }

  /* A GPIO edge is converted by gpio_intc into a rising-edge pulse for the
   * GIC.  This matches meson_gpio_irq_type_output() in the vendor SDK.
   */

  up_set_irq_type(CTP_IRQ, IRQ_RISING_EDGE);

  ret = s6_gpioint_config(CTP_INT_CHANNEL, CTP_INT_GPIO,
                          IRQ_FALLING_EDGE);
  if (ret < 0)
    {
      irq_detach(CTP_IRQ);
    }

  return ret;
}

/****************************************************************************
 * Name: gt9xx_irq_enable
 *
 * Description:
 *   Enable or disable Touch Panel Interrupts at the GIC.
 *
 ****************************************************************************/

static void gt9xx_irq_enable(const struct gt9xx_board_s *state, bool enable)
{
  iinfo("enable=%d\n", enable);

  if (enable)
    {
      up_enable_irq(CTP_IRQ);
    }
  else
    {
      up_disable_irq(CTP_IRQ);
    }
}

/****************************************************************************
 * Name: gt9xx_set_power
 *
 * Description:
 *   Power the Touch Panel up with the GT911 reset sequence, or leave it
 *   running on close (holding reset would stop touch scanning).
 *
 ****************************************************************************/

static int gt9xx_set_power(const struct gt9xx_board_s *state, bool on)
{
  iinfo("on=%d\n", on);

  return on ? gt911_reset() : OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vsai_logicpi_a1_touch_register
 *
 * Description:
 *   Register the GT911 Touch Panel on I2C0 as devpath (e.g. "/dev/input0").
 *
 * Input Parameters:
 *   devpath - Device Path (e.g. "/dev/input0")
 *   i2c     - I2C bus (as returned by s6_i2cbus_initialize())
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value is returned on any failure.
 *
 ****************************************************************************/

int vsai_logicpi_a1_touch_register(FAR const char *devpath,
                                   FAR struct i2c_master_s *i2c)
{
  int ret;

  iinfo("devpath=%s\n", devpath);

  /* Route I2C0 to GPIOA_0 (SDA) / GPIOA_1 (SCL), function 3 */

  ret = s6_gpioa_set_mux(0, 3);
  if (ret < 0)
    {
      ierr("SDA mux failed: %d\n", ret);
      return ret;
    }

  ret = s6_gpioa_set_mux(1, 3);
  if (ret < 0)
    {
      ierr("SCL mux failed: %d\n", ret);
      return ret;
    }

  ret = gt9xx_register(devpath, i2c, CTP_I2C_ADDR, &g_vsai_logicpi_a1_gt9xx);
  if (ret < 0)
    {
      ierr("gt9xx_register failed: %d\n", ret);
    }

  return ret;
}
