/****************************************************************************
 * app/mihome/mihome.c
 *
 * Model table and the calls built from it.  Older miio devices (the AC
 * partner) read properties by name with get_prop and have a method per
 * setting; MIoT devices read and write properties by service and property
 * id with get_properties / set_properties:
 *
 *   get_prop        ["power","tar_temp"]                -> ["on",25]
 *   set_power       ["on"]                              -> ["ok"]
 *   get_properties  [{"did":"power","siid":2,"piid":1}]
 *                   -> [{"did":"power","siid":2,"piid":1,"code":0,
 *                        "value":true}]
 *   set_properties  [{"did":"power","siid":2,"piid":1,"value":true}]
 *                   -> [{"did":"power","siid":2,"piid":1,"code":0}]
 *
 * The device sends "did" back as it got it, the property name is used.
 * No MIoT model is in the table yet, that part has not run on a device.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mihome.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Index in mihome_model_s.prop, also the bit of the MIHOME_* field */

#define PROP_POWER            0
#define PROP_MODE             1
#define PROP_TEMP             2
#define PROP_LOAD             3
#define PROP_COUNT            4

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mihome_prop_s
{
  FAR const char *name;     /* NULL if the model does not have it */
  uint8_t siid;             /* MIoT only */
  uint8_t piid;
};

struct mihome_model_s
{
  FAR const char *model;
  bool miot;
  struct mihome_prop_s prop[PROP_COUNT];
  FAR const char *set_power;  /* miio methods */
  FAR const char *set_temp;
  int run_load;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* The first entry is used when MIIO_CONF names no model */

static const struct mihome_model_s g_models[] =
{
  /* AC partner 2, docs/miio/README.md 4.3: below about 60 W the
   * compressor does not run
   */

  {
    "lumi.acpartner.mcn02", false,
    {
      { "power" }, { "mode" }, { "tar_temp" }, { "load_power" }
    },
    "set_power", "set_tar_temp", 60
  },
};

static volatile enum mihome_mode_e g_mode;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int mihome_open(FAR struct mihome_s *h)
{
  bool mock = mihome_is_mock();
  int ret;
  int i;

  if (h->model != NULL && h->mock == mock)
    {
      return 0;
    }

  /* The mock is the first model, the AC partner */

  mihome_close(h);
  h->mock = mock;
  if (mock)
    {
      h->model = &g_models[0];
      return 0;
    }

  ret = miio_open(&h->miio, MIIO_CONF);
  if (ret < 0)
    {
      return ret;
    }

  for (i = 0; i < nitems(g_models); i++)
    {
      if (h->miio.model[0] == '\0' ||
          strcmp(h->miio.model, g_models[i].model) == 0)
        {
          h->model = &g_models[i];
          return 0;
        }
    }

  miio_close(&h->miio);
  return -ENOTSUP;
}

static int mihome_call(FAR struct mihome_s *h, FAR const char *method,
                       FAR const char *params, FAR char *buf, size_t len)
{
  return h->mock ? mihome_mock_call(method, params, buf, len) :
                   miio_call(&h->miio, method, params, buf, len);
}

/* Point v at the n values of the result, strings without their quotes */

static int mihome_values(FAR char *buf, bool miot, FAR char **v, int n)
{
  FAR char *p = strstr(buf, "\"result\":[");
  FAR char *q = p != NULL ? strchr(p, ']') : NULL;
  int i;

  if (q == NULL)
    {
      return -EPROTO;
    }

  *q = '\0';
  p += 10;
  for (i = 0; i < n && p != NULL; i++)
    {
      if (miot)
        {
          p = strstr(p, "\"value\":");
          if (p == NULL)
            {
              break;
            }

          v[i] = p + 8;
          p = v[i] + strcspn(v[i], ",}");
          if (*p != '\0')
            {
              *p++ = '\0';
            }
        }
      else
        {
          v[i] = strsep(&p, ",");
        }

      if (*v[i] == '"')
        {
          v[i]++;
          v[i][strcspn(v[i], "\"")] = '\0';
        }
    }

  return i == n ? 0 : -EPROTO;
}

/* Write one property, value is JSON */

static int mihome_set(FAR struct mihome_s *h, int prop,
                      FAR const char *value)
{
  FAR const struct mihome_model_s *mo = h->model;
  FAR const struct mihome_prop_s *pr = &mo->prop[prop];
  char params[128];
  char buf[256];
  int ret;

  if (pr->name == NULL)
    {
      return -ENOTSUP;
    }

  if (mo->miot)
    {
      snprintf(params, sizeof(params),
               "[{\"did\":\"%s\",\"siid\":%d,\"piid\":%d,\"value\":%s}]",
               pr->name, pr->siid, pr->piid, value);
    }
  else
    {
      snprintf(params, sizeof(params), "[%s]", value);
    }

  ret = mihome_call(h, mo->miot ? "set_properties" :
                       prop == PROP_POWER ? mo->set_power : mo->set_temp,
                    params, buf, sizeof(buf));
  if (ret < 0)
    {
      return ret;
    }

  return strstr(buf, mo->miot ? "\"code\":0" : "\"ok\"") != NULL ?
         0 : -EPROTO;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void mihome_set_mode(enum mihome_mode_e mode)
{
  g_mode = mode;
}

enum mihome_mode_e mihome_get_mode(void)
{
  return g_mode;
}

bool mihome_is_mock(void)
{
  return g_mode == MIHOME_MOCK ||
         (g_mode == MIHOME_AUTO && access(MIIO_CONF, F_OK) != 0);
}

void mihome_init(FAR struct mihome_s *h)
{
  h->model = NULL;
  h->mock = false;
}

void mihome_close(FAR struct mihome_s *h)
{
  if (h->model != NULL && !h->mock)
    {
      miio_close(&h->miio);
    }

  h->model = NULL;
}

int mihome_get(FAR struct mihome_s *h, FAR struct mihome_state_s *s)
{
  FAR const struct mihome_model_s *mo;
  FAR const struct mihome_prop_s *pr;
  FAR char *v[PROP_COUNT];
  int prop[PROP_COUNT];
  char params[256];
  char buf[512];
  size_t len;
  int ret;
  int n;
  int i;

  ret = mihome_open(h);
  if (ret < 0)
    {
      return ret;
    }

  /* Ask for the properties the model has */

  mo = h->model;
  len = snprintf(params, sizeof(params), "[");
  for (i = n = 0; i < PROP_COUNT; i++)
    {
      pr = &mo->prop[i];
      if (pr->name == NULL)
        {
          continue;
        }

      if (mo->miot)
        {
          len += snprintf(params + len, sizeof(params) - len,
                          "%s{\"did\":\"%s\",\"siid\":%d,\"piid\":%d}",
                          n > 0 ? "," : "", pr->name, pr->siid, pr->piid);
        }
      else
        {
          len += snprintf(params + len, sizeof(params) - len, "%s\"%s\"",
                          n > 0 ? "," : "", pr->name);
        }

      prop[n++] = i;
    }

  snprintf(params + len, sizeof(params) - len, "]");

  ret = mihome_call(h, mo->miot ? "get_properties" : "get_prop", params,
                    buf, sizeof(buf));
  if (ret >= 0)
    {
      ret = mihome_values(buf, mo->miot, v, n);
    }

  if (ret < 0)
    {
      return ret;
    }

  memset(s, 0, sizeof(*s));
  s->mock = h->mock;
  s->model = mo->model;
  s->run_load = mo->run_load;
  for (i = 0; i < n; i++)
    {
      s->fields |= 1 << prop[i];
      switch (prop[i])
        {
          case PROP_POWER:
            s->on = strcmp(v[i], "on") == 0 || strcmp(v[i], "true") == 0;
            break;

          case PROP_MODE:
            strlcpy(s->mode, v[i], sizeof(s->mode));
            break;

          case PROP_TEMP:
            s->temp = atoi(v[i]);
            break;

          case PROP_LOAD:
            s->load = strtod(v[i], NULL) + 0.5;
            break;
        }
    }

  return 0;
}

int mihome_power(FAR struct mihome_s *h, bool on)
{
  int ret;

  ret = mihome_open(h);
  if (ret < 0)
    {
      return ret;
    }

  return mihome_set(h, PROP_POWER, h->model->miot ?
                                   (on ? "true" : "false") :
                                   on ? "\"on\"" : "\"off\"");
}

int mihome_temp(FAR struct mihome_s *h, int temp)
{
  char value[8];
  int ret;

  ret = mihome_open(h);
  if (ret < 0)
    {
      return ret;
    }

  snprintf(value, sizeof(value), "%d", temp);
  return mihome_set(h, PROP_TEMP, value);
}
