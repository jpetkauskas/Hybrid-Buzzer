#include "captive_dns.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <stdint.h>
#include <string.h>

#define DNS_PORT    53
#define DNS_BUF_LEN 256 /* a UDP DNS query/response comfortably fits */
#define DNS_TYPE_A  1
#define DNS_TTL     60

static uint32_t resolve_addr; /* network-byte-order IPv4 to answer with */

/* Turn the received query in `buf` (length `len`) into a response that points
   the queried A record at resolve_addr. Returns the response length, or 0 if
   the message is too malformed to answer. */
static int build_response(uint8_t *buf, int len)
{
  if (len < 12) /* smaller than a DNS header */
  {
    return 0;
  }

  /* Walk the QNAME labels (length-prefixed, terminated by a 0 byte). */
  int q = 12;
  while (q < len && buf[q] != 0)
  {
    q += buf[q] + 1;
  }
  if (q + 5 > len) /* need the 0 terminator + qtype(2) + qclass(2) */
  {
    return 0;
  }

  int qtype = (buf[q + 1] << 8) | buf[q + 2];

  buf[2] = 0x81; /* QR=1, opcode=0, RD preserved */
  buf[3] = 0x80; /* RA=1, RCODE=0 */

  int out = q + 5; /* end of the question section */
  int answers = 0;

  if (qtype == DNS_TYPE_A && out + 16 <= DNS_BUF_LEN)
  {
    buf[out++] = 0xC0; /* name is a pointer... */
    buf[out++] = 0x0C; /* ...to the question at offset 12 */
    buf[out++] = 0x00;
    buf[out++] = 0x01; /* type A */
    buf[out++] = 0x00;
    buf[out++] = 0x01; /* class IN */
    buf[out++] = (DNS_TTL >> 24) & 0xFF;
    buf[out++] = (DNS_TTL >> 16) & 0xFF;
    buf[out++] = (DNS_TTL >> 8) & 0xFF;
    buf[out++] = DNS_TTL & 0xFF;
    buf[out++] = 0x00;
    buf[out++] = 0x04; /* RDLENGTH = 4 */
    memcpy(&buf[out], &resolve_addr, 4);
    out += 4;
    answers = 1;
  }

  buf[6] = (answers >> 8) & 0xFF;
  buf[7] = answers & 0xFF;

  return out;
}

static void dns_task(void *arg)
{
  struct sockaddr_in bind_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = htonl(INADDR_ANY),
      .sin_port = htons(DNS_PORT),
  };

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0 || bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
  {
    if (sock >= 0)
    {
      close(sock);
    }
    vTaskDelete(NULL);
    return;
  }

  uint8_t buf[DNS_BUF_LEN];
  for (;;)
  {
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);
    int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &src_len);
    if (len <= 0)
    {
      continue;
    }

    int resp_len = build_response(buf, len);
    if (resp_len > 0)
    {
      sendto(sock, buf, resp_len, 0, (struct sockaddr *)&src, src_len);
    }
  }
}

void start_captive_dns(esp_ip4_addr_t resolve_to)
{
  resolve_addr = resolve_to.addr; /* esp_ip4_addr_t is already network order */
  xTaskCreate(dns_task, "captive_dns", 3072, NULL, 5, NULL);
}
