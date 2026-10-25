/* Embedded HTTP/WebSocket endpoint: web console + /healthcheck.
 * See src/web.c; a single thread serves all web clients. */

#if !defined(__WEB_H_INCLUDED__)
#define __WEB_H_INCLUDED__

#include <stddef.h>

/* thread entry: accepts HTTP connections on listen_http */
void listen_thread_http(void *args);

/* 1 when the hub core is responsive (user-table lock obtainable) */
int web_health_ok(void);

/* RFC6455 codec, exposed for unit tests */

struct ws_frame
{
	int fin;
	int opcode;
	int masked;
	unsigned char mask[4];
	unsigned long long len;
	size_t hdrlen;		/* header bytes consumed from the source buffer */
};

/* parse one frame header from src (n bytes available); returns 1 on a
   complete header (frame body may still be incomplete: hdrlen+len > n),
   0 when more bytes are needed, -1 on a protocol error */
int ws_parse_frame(const unsigned char *src, size_t n, struct ws_frame *f);

/* write an unmasked server->client frame; returns total length or 0 when
   dstsz is too small */
size_t ws_write_frame(unsigned char *dst, size_t dstsz, int opcode,
	const unsigned char *payload, size_t len);

#endif /* __WEB_H_INCLUDED__ */
