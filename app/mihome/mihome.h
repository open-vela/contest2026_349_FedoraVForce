/****************************************************************************
 * app/mihome/mihome.h
 *
 * Mi Home device control for the apps: read the state, switch on and off,
 * set the temperature.  What to call on which model comes from a table in
 * mihome.c.  Two backends: the device in MIIO_CONF over the miio protocol
 * (app/miio), or a mock AC partner in memory (mihome_mock.c) when there is
 * no MIIO_CONF, for demos without network.  Apps and NSH have to say when
 * they show the mock.
 ****************************************************************************/

#ifndef __APP_MIHOME_MIHOME_H
#define __APP_MIHOME_MIHOME_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>

#include "miio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Fields of struct mihome_state_s that the model has */

#define MIHOME_POWER          (1 << 0)
#define MIHOME_MODE           (1 << 1)
#define MIHOME_TEMP           (1 << 2)
#define MIHOME_LOAD           (1 << 3)

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum mihome_mode_e
{
  MIHOME_AUTO,              /* The device if there is MIIO_CONF, else mock */
  MIHOME_REAL,
  MIHOME_MOCK
};

struct mihome_model_s;

struct mihome_s
{
  FAR const struct mihome_model_s *model;   /* NULL until opened */
  bool mock;                /* Backend of the last call */
  struct miio_s miio;
};

struct mihome_state_s
{
  bool mock;
  FAR const char *model;
  unsigned int fields;      /* MIHOME_* */
  bool on;
  int temp;                 /* Target, degree C */
  int load;                 /* W */
  int run_load;             /* Load above which it really runs, 0 if none */
  char mode[16];            /* e.g. "cool" */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Backend for every handle of every task, MIHOME_AUTO at boot */

void mihome_set_mode(enum mihome_mode_e mode);
enum mihome_mode_e mihome_get_mode(void);

/* Whether a call now goes to the mock */

bool mihome_is_mock(void);

/* A closed handle, every call opens it when needed, and again with the
 * other backend after the mode or MIIO_CONF changed
 */

void mihome_init(FAR struct mihome_s *h);
void mihome_close(FAR struct mihome_s *h);

/* All return 0 or a negated errno:
 *   -ENOENT, -EINVAL   no or a malformed MIIO_CONF
 *   -ENOTSUP           model in MIIO_CONF not in the table, or a field it
 *                      does not have
 *   -ETIMEDOUT, ...    errors of miio_call(), the device is offline
 *   -EPROTO            a reply that does not fit the model
 */

int mihome_get(FAR struct mihome_s *h, FAR struct mihome_state_s *s);

/* The AC partner may come on at another temperature (quick cooling, set
 * in the Mi Home app), that is left as it is.
 */

int mihome_power(FAR struct mihome_s *h, bool on);
int mihome_temp(FAR struct mihome_s *h, int temp);

/* mihome_mock.c: answer a miio call like an AC partner would */

int mihome_mock_call(FAR const char *method, FAR const char *params,
                     FAR char *buf, size_t len);

#endif /* __APP_MIHOME_MIHOME_H */
