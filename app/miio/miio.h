/****************************************************************************
 * app/miio/miio.h
 *
 * Minimal miio LAN client: JSON-RPC with a Mi Home device over UDP 54321,
 * AES-128-CBC encrypted.  Protocol only, what to call on which device is
 * up to app/mihome.  The device IP and token come from a config file,
 * never from the build.
 ****************************************************************************/

#ifndef __APP_MIIO_MIIO_H
#define __APP_MIIO_MIIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stddef.h>
#include <stdint.h>

#include <netinet/in.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* <ip>\n<32 hex token>\n and an optional <model>\n */

#define MIIO_CONF             "/tmp/miio.conf"

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct miio_s
{
  int fd;
  struct sockaddr_in addr;
  uint8_t token[16];
  uint8_t key[16];
  uint8_t iv[16];
  char model[32];             /* From the config, "" if not there */
  uint32_t did;               /* Device id from the last hello */
  uint32_t id;                /* Id of the last command */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Read the config file and open the socket.  Returns 0 or a negated
 * errno: -ENOENT without the file, -EINVAL if it is malformed.
 */

int miio_open(FAR struct miio_s *m, FAR const char *conf);
void miio_close(FAR struct miio_s *m);

/* Send method with params (a JSON array, e.g. "[]") and put the JSON of
 * the reply in buf.  Returns its length or a negated errno: -ETIMEDOUT if
 * the device does not answer, -EBADMSG if the reply does not decrypt
 * (both mean a wrong token, or the device is offline).
 */

int miio_call(FAR struct miio_s *m, FAR const char *method,
              FAR const char *params, FAR char *buf, size_t len);

#endif /* __APP_MIIO_MIIO_H */
