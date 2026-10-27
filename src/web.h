/* Embedded HTTP/WebSocket endpoint: web console + /healthcheck.
 * See src/web.c; a single thread serves all web clients. */

#if !defined(__WEB_H_INCLUDED__)
#define __WEB_H_INCLUDED__

#include <stddef.h>

/* thread entry: accepts HTTP connections on listen_http */
void listen_thread_http(void *args);

/* 1 when the hub core is responsive (user-table lock obtainable) */
int web_health_ok(void);

/* --- embedded web chat (/chat page, JSON protocol over /chatws) ---
 * Hub-side entry points. They never touch the user table and never take
 * mutex_users; the caller (worker/console paths) must already hold it.
 * They take only the chat-registry and output-queue locks. */

/* 1 when at least one logged-in web chat session exists */
int web_chat_active(void);

/* forward a hub public-chat multicast line ("<nick> text|") to chat clients */
void web_chat_broadcast(const char *line);

/* 1 when nick is claimed by a logged-in web chat session (nick reservation
   in validnick(): a hub client must not take the nick of a web user) */
int web_chat_nick_used(const char *nick);

/* deliver a private message to a web chat session; 1 when delivered,
   0 when no web session owns the nick */
int web_chat_deliver_pm(const char *to,const char *from,const char *msg);

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
