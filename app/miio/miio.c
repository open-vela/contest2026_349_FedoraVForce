/****************************************************************************
 * app/miio/miio.c
 *
 * miio protocol, see docs/miio/README.md:
 *
 *   key = MD5(token), iv = MD5(key || token)
 *   packet = 2131 | length | 0 | device id | stamp | checksum | payload
 *   payload = AES-128-CBC(key, iv, PKCS7(JSON))
 *   checksum = MD5(header[0:16] || token || payload)
 *
 * Every command starts with a hello for the device id and the stamp, and
 * is tried twice with a 2 s timeout each.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mbedtls/aes.h>
#include <mbedtls/md5.h>

#include "miio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MIIO_PORT             54321
#define MIIO_HEAD             32
#define MIIO_MAX              1024    /* Largest JSON, request or reply */
#define MIIO_TRIES            2

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t get_be32(FAR const uint8_t *p)
{
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | p[2] << 8 | p[3];
}

static void put_be32(FAR uint8_t *p, uint32_t v)
{
  p[0] = v >> 24;
  p[1] = v >> 16;
  p[2] = v >> 8;
  p[3] = v;
}

static void put_head(FAR uint8_t *p, size_t len, uint32_t did,
                     uint32_t stamp)
{
  p[0] = 0x21;
  p[1] = 0x31;
  p[2] = len >> 8;
  p[3] = len;
  put_be32(p + 4, 0);
  put_be32(p + 8, did);
  put_be32(p + 12, stamp);
}

static int miio_hello(FAR struct miio_s *m, FAR uint32_t *stamp)
{
  uint8_t pkt[MIIO_HEAD];
  ssize_t n;

  memset(pkt, 0xff, sizeof(pkt));
  put_head(pkt, MIIO_HEAD, 0xffffffff, 0xffffffff);
  memset(pkt + 4, 0xff, 4);

  if (sendto(m->fd, pkt, sizeof(pkt), 0, (FAR struct sockaddr *)&m->addr,
             sizeof(m->addr)) < 0)
    {
      return -errno;
    }

  n = recv(m->fd, pkt, sizeof(pkt), 0);
  if (n < 0)
    {
      return errno == EAGAIN ? -ETIMEDOUT : -errno;
    }

  if (n != MIIO_HEAD || pkt[0] != 0x21 || pkt[1] != 0x31)
    {
      return -EBADMSG;
    }

  m->did = get_be32(pkt + 8);
  *stamp = get_be32(pkt + 12);
  return 0;
}

/* One try: hello, send the command and wait for the reply with its id */

static int miio_send(FAR struct miio_s *m, FAR const char *json,
                     FAR uint8_t *pkt, FAR char *buf, size_t len)
{
  mbedtls_aes_context aes;
  uint8_t iv[16];
  uint32_t stamp = 0;
  size_t plen;
  ssize_t n;
  char tag[24];
  int pad;
  int ret;

  ret = miio_hello(m, &stamp);
  if (ret < 0)
    {
      return ret;
    }

  /* PKCS7 pad and encrypt the JSON behind the header */

  plen = strlen(json);
  pad = 16 - plen % 16;
  memcpy(pkt + MIIO_HEAD, json, plen);
  memset(pkt + MIIO_HEAD + plen, pad, pad);
  plen += pad;

  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, m->key, 128);
  memcpy(iv, m->iv, sizeof(iv));
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, plen, iv,
                        pkt + MIIO_HEAD, pkt + MIIO_HEAD);
  mbedtls_aes_free(&aes);

  /* The checksum is taken with the token in its place */

  put_head(pkt, MIIO_HEAD + plen, m->did, stamp + 1);
  memcpy(pkt + 16, m->token, 16);
  mbedtls_md5(pkt, MIIO_HEAD + plen, pkt + 16);

  if (sendto(m->fd, pkt, MIIO_HEAD + plen, 0,
             (FAR struct sockaddr *)&m->addr, sizeof(m->addr)) < 0)
    {
      return -errno;
    }

  /* Skip late replies to an earlier try until the timeout */

  snprintf(tag, sizeof(tag), "\"id\":%" PRIu32, m->id);
  for (; ; )
    {
      n = recv(m->fd, pkt, MIIO_HEAD + MIIO_MAX, 0);
      if (n < 0)
        {
          return errno == EAGAIN ? -ETIMEDOUT : -errno;
        }

      plen = n - MIIO_HEAD;
      if (n <= MIIO_HEAD || plen % 16 || plen > len)
        {
          continue;
        }

      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_dec(&aes, m->key, 128);
      memcpy(iv, m->iv, sizeof(iv));
      mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, plen, iv,
                            pkt + MIIO_HEAD, (FAR uint8_t *)buf);
      mbedtls_aes_free(&aes);

      pad = buf[plen - 1];
      if (pad < 1 || pad > 16)
        {
          return -EBADMSG;
        }

      plen -= pad;
      while (plen > 0 && buf[plen - 1] == '\0')
        {
          plen--;
        }

      if (plen == len)
        {
          return -E2BIG;
        }

      buf[plen] = '\0';
      if (strstr(buf, tag) != NULL)
        {
          return plen;
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int miio_open(FAR struct miio_s *m, FAR const char *conf)
{
  struct timeval tv =
    {
      2, 0
    };

  struct timespec ts;
  uint8_t buf[32];
  char hex[33];
  char ip[16];
  FAR FILE *fp;
  int ret;
  int i;

  memset(m, 0, sizeof(*m));
  m->fd = -1;

  fp = fopen(conf, "r");
  if (fp == NULL)
    {
      return -ENOENT;
    }

  ret = fscanf(fp, "%15s %32s %31s", ip, hex, m->model);
  fclose(fp);

  m->addr.sin_family = AF_INET;
  m->addr.sin_port = htons(MIIO_PORT);
  if (ret < 2 || strlen(hex) != 32 ||
      inet_pton(AF_INET, ip, &m->addr.sin_addr) != 1)
    {
      ret = -EINVAL;
      goto out;
    }

  for (i = 0; i < 16; i++)
    {
      if (sscanf(hex + 2 * i, "%2hhx", &m->token[i]) != 1)
        {
          ret = -EINVAL;
          goto out;
        }
    }

  mbedtls_md5(m->token, 16, m->key);
  memcpy(buf, m->key, 16);
  memcpy(buf + 16, m->token, 16);
  mbedtls_md5(buf, 32, m->iv);

  m->fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (m->fd < 0)
    {
      ret = -errno;
      goto out;
    }

  setsockopt(m->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  /* The device drops ids it has just seen, so do not start at 1 */

  clock_gettime(CLOCK_MONOTONIC, &ts);
  m->id = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  ret = 0;

out:
  memset(hex, 0, sizeof(hex));
  memset(buf, 0, sizeof(buf));
  return ret;
}

void miio_close(FAR struct miio_s *m)
{
  if (m->fd >= 0)
    {
      close(m->fd);
    }

  memset(m, 0, sizeof(*m));
  m->fd = -1;
}

int miio_call(FAR struct miio_s *m, FAR const char *method,
              FAR const char *params, FAR char *buf, size_t len)
{
  uint8_t pkt[MIIO_HEAD + MIIO_MAX];
  char json[MIIO_MAX - 16];
  int ret = -EINVAL;
  int i;

  for (i = 0; i < MIIO_TRIES; i++)
    {
      m->id++;
      ret = snprintf(json, sizeof(json),
                     "{\"id\":%" PRIu32 ",\"method\":\"%s\",\"params\":%s}",
                     m->id, method, params);
      if (ret >= sizeof(json))
        {
          return -E2BIG;
        }

      ret = miio_send(m, json, pkt, buf, len);
      if (ret != -ETIMEDOUT)
        {
          break;
        }
    }

  return ret;
}
