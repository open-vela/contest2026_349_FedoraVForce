/****************************************************************************
 * app/mihome/mihome_mock.c
 *
 * Mock AC partner for demos without network.  It answers get_prop,
 * set_power and set_tar_temp with the JSON the real one sends, so the
 * same parsing and apps run on it.  The load follows what the real one
 * did on 10-29 (docs/miio/README.md 4.5):
 *
 *   off               96 W, after 15 s 24 W standby
 *   on                the target becomes 20 degree C (quick cooling,
 *                     set in the Mi Home app), 30 W while the
 *                     compressor waits for 20 s, then
 *                     120 + (30 - target) * 30 W
 *
 * all +-3 W, and every call takes 50 to 150 ms like a round trip.  One
 * device for all tasks, the launcher apps and NSH see the same one.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mihome.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MOCK_OFF_LOAD         96
#define MOCK_STANDBY_LOAD     24
#define MOCK_STANDBY_AFTER    15000   /* ms */
#define MOCK_WAIT_LOAD        30
#define MOCK_WAIT             20000   /* ms */
#define MOCK_ON_TEMP          20
#define MOCK_TEMP_MIN         16
#define MOCK_TEMP_MAX         30

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* Runs at 25 degree C since long before boot */

static bool g_on = true;
static int g_temp = 25;
static int64_t g_since = -3600 * 1000;    /* ms of the last on or off */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int64_t mock_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int mock_load(void)
{
  int64_t t = mock_ms() - g_since;
  int load;

  if (!g_on)
    {
      load = t < MOCK_STANDBY_AFTER ? MOCK_OFF_LOAD : MOCK_STANDBY_LOAD;
    }
  else if (t < MOCK_WAIT)
    {
      load = MOCK_WAIT_LOAD;
    }
  else
    {
      load = 120 + (30 - g_temp) * 30;
    }

  return load + rand() % 7 - 3;
}

static void mock_power(bool on)
{
  if (on != g_on)
    {
      g_since = mock_ms();
    }

  /* Quick cooling, also when it was on */

  if (on)
    {
      g_temp = MOCK_ON_TEMP;
    }

  g_on = on;
}

/* ["power","tar_temp"] -> "on",25 */

static void mock_get(FAR const char *params, FAR char *out, size_t len)
{
  FAR char *names;
  FAR char *name;
  FAR char *p;
  char value[16];
  size_t n = 0;

  out[0] = '\0';
  names = strdup(params);
  if (names == NULL)
    {
      return;
    }

  p = names + strspn(names, "[");
  while ((name = strsep(&p, ",")) != NULL && n < len)
    {
      name += strspn(name, "\"");
      name[strcspn(name, "\"]")] = '\0';

      if (strcmp(name, "power") == 0)
        {
          strlcpy(value, g_on ? "\"on\"" : "\"off\"", sizeof(value));
        }
      else if (strcmp(name, "mode") == 0)
        {
          strlcpy(value, "\"cool\"", sizeof(value));
        }
      else if (strcmp(name, "tar_temp") == 0)
        {
          snprintf(value, sizeof(value), "%d", g_temp);
        }
      else if (strcmp(name, "load_power") == 0)
        {
          snprintf(value, sizeof(value), "%d.00", mock_load());
        }
      else
        {
          strlcpy(value, "\"\"", sizeof(value));
        }

      n += snprintf(out + n, len - n, "%s%s", n > 0 ? "," : "", value);
    }

  free(names);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int mihome_mock_call(FAR const char *method, FAR const char *params,
                     FAR char *buf, size_t len)
{
  char result[128];
  int temp;

  usleep((50 + rand() % 101) * 1000);

  pthread_mutex_lock(&g_lock);
  if (strcmp(method, "get_prop") == 0)
    {
      mock_get(params, result, sizeof(result));
    }
  else if (strcmp(method, "set_power") == 0 &&
           (strcmp(params, "[\"on\"]") == 0 ||
            strcmp(params, "[\"off\"]") == 0))
    {
      mock_power(strcmp(params, "[\"on\"]") == 0);
      strlcpy(result, "\"ok\"", sizeof(result));
    }
  else if (strcmp(method, "set_tar_temp") == 0 &&
           sscanf(params, "[%d]", &temp) == 1 &&
           temp >= MOCK_TEMP_MIN && temp <= MOCK_TEMP_MAX)
    {
      g_temp = temp;
      strlcpy(result, "\"ok\"", sizeof(result));
    }
  else
    {
      result[0] = '\0';
    }

  pthread_mutex_unlock(&g_lock);

  /* Like the device: an error object for what it does not know */

  if (result[0] == '\0')
    {
      return snprintf(buf, len, "{\"id\":0,\"error\":{\"code\":-5001,"
                      "\"message\":\"invalid params\"}}");
    }

  return snprintf(buf, len, "{\"id\":0,\"result\":[%s]}", result);
}
