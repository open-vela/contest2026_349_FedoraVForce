/****************************************************************************
 * app/mihome/mihome_main.c
 *
 * NSH command miio: state of the Mi Home device in MIIO_CONF, switch it
 * on and off, set its temperature, choose the backend.  Every line from
 * the mock starts with [mock].
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mihome.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void mihome_print(FAR const struct mihome_state_s *s)
{
  FAR const char *tag = s->mock ? "[mock] " : "";

  printf("%smodel:      %s\n", tag, s->model);
  if (s->fields & MIHOME_POWER)
    {
      printf("%spower:      %s\n", tag, s->on ? "on" : "off");
    }

  if (s->fields & MIHOME_MODE)
    {
      printf("%smode:       %s\n", tag, s->mode);
    }

  if (s->fields & MIHOME_TEMP)
    {
      printf("%star_temp:   %d\n", tag, s->temp);
    }

  if (s->fields & MIHOME_LOAD)
    {
      printf("%sload_power: %d W\n", tag, s->load);
    }
}

static void mihome_error(bool mock, int ret)
{
  FAR const char *tag = mock ? "[mock] " : "";

  switch (ret)
    {
      case -ENOENT:
        printf("%smiio: not configured, no %s\n", tag, MIIO_CONF);
        break;

      case -EINVAL:
        printf("%smiio: bad %s\n", tag, MIIO_CONF);
        break;

      case -ENOTSUP:
        printf("%smiio: not supported by the model\n", tag);
        break;

      case -ETIMEDOUT:
        printf("%smiio: no reply, device offline or wrong token\n", tag);
        break;

      case -EBADMSG:
        printf("%smiio: reply does not decrypt, wrong token?\n", tag);
        break;

      case -EPROTO:
        printf("%smiio: refused or unexpected reply\n", tag);
        break;

      default:
        printf("%smiio: failed: %d\n", tag, ret);
        break;
    }
}

static int mihome_mode(int argc, FAR char *argv[])
{
  static FAR const char * const names[] =
  {
    "auto", "real", "mock"
  };

  enum mihome_mode_e mode;
  bool mock;

  if (argc == 3)
    {
      for (mode = MIHOME_AUTO; mode <= MIHOME_MOCK; mode++)
        {
          if (strcmp(argv[2], names[mode]) == 0)
            {
              break;
            }
        }

      if (mode > MIHOME_MOCK)
        {
          return -EINVAL;
        }

      mihome_set_mode(mode);
    }

  mode = mihome_get_mode();
  mock = mihome_is_mock();
  printf("%smode: %s%s\n", mock ? "[mock] " : "", names[mode],
         mode != MIHOME_AUTO ? "" :
         mock ? ", no " MIIO_CONF : ", " MIIO_CONF " found");
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct mihome_state_s s;
  struct mihome_s h;
  int ret;

  if (argc >= 2 && argc <= 3 && strcmp(argv[1], "mode") == 0 &&
      mihome_mode(argc, argv) == 0)
    {
      return EXIT_SUCCESS;
    }
  else if (argc == 2 && strcmp(argv[1], "on") == 0)
    {
      mihome_init(&h);
      ret = mihome_power(&h, true);
    }
  else if (argc == 2 && strcmp(argv[1], "off") == 0)
    {
      mihome_init(&h);
      ret = mihome_power(&h, false);
    }
  else if (argc == 3 && strcmp(argv[1], "temp") == 0)
    {
      mihome_init(&h);
      ret = mihome_temp(&h, atoi(argv[2]));
    }
  else if (argc == 2 && strcmp(argv[1], "get") == 0)
    {
      mihome_init(&h);
      ret = 0;
    }
  else
    {
      printf("Usage: %s get | on | off | temp <degree C> |\n"
             "       mode [auto | real | mock]\n"
             "  auto: the device in %s if it is there, else a mock\n",
             argv[0], MIIO_CONF);
      return EXIT_FAILURE;
    }

  /* Every command ends with the state it left */

  if (ret == 0)
    {
      ret = mihome_get(&h, &s);
    }

  if (ret == 0)
    {
      mihome_print(&s);
    }
  else
    {
      mihome_error(h.mock, ret);
    }

  mihome_close(&h);
  return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
