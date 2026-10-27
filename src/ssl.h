/* $Id$ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * Optional TLS support (OpenSSL libssl):
 *   - encrypted hub client port (listen_port_ssl, default 412) so
 *     TLS-capable DC++-style clients can connect with ssl://host:412
 *   - HTTPS on the embedded web endpoint (listen_port_https)
 *
 * TLS is optional: it is only active when ssl_cert_file is configured
 * and the certificate/key load successfully at startup. The regular
 * plaintext ports (listen_port_main, listen_port_http, listen_port_cons)
 * are unaffected, so old clients keep working side by side.
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __SSL_H_INCLUDED__ )
#define __SSL_H_INCLUDED__

#include <openssl/ssl.h>
#include <openssl/err.h>

/* build the server SSL context from conf.ssl_cert_file / conf.ssl_key_file;
 * called once at startup (before setuid, so a root-owned key is readable).
 * 0: TLS ready, -1: not configured, -2: configured but failed to load.
 * A load failure never aborts the daemon: the hub runs plaintext-only. */
int ssl_init(void);

/* 1 when a server context is live (ports with TLS may be opened) */
int ssl_available(void);

/* TLS server handshake on an already accepted, O_NONBLOCK socket;
 * poll-driven with a total deadline, so it cannot block a listen thread.
 * Returns the SSL object on success, NULL on any handshake failure
 * (the caller keeps ownership of the socket). */
SSL *ssl_accept_socket(int sock);

/* bytes decrypted and buffered by libssl but not yet read by the caller;
 * poll() will not report such data, so readers must also check this.
 * NULL-safe. */
int ssl_pending(void *ssl);

/* best-effort TLS shutdown + free (NULL-safe); the fd stays open and is
 * closed by the caller. */
void ssl_destroy(void *ssl);

/* clear SO_LINGER(0) on an accepted socket: the hub's default linger
 * makes close() send RST, which discards pending TLS records and breaks
 * the peer's SSL teardown; TLS connections must close cleanly. */
void ssl_clean_close(int sock);

/* drop a socket whose handshake failed: drain the leftover bytes libssl
 * did not consume (unread data in the receive queue makes close() send
 * RST and discard the TLS alert we just wrote), then close. */
void ssl_reject_socket(int sock);

#endif /* __SSL_H_INCLUDED__ */

/* VIM Settings {{{
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * soft-stop-width: 4
 * c indent on
 * End:
 * vim600: sw=4 ts=4 sts=4 cindent fdm=marker
 * vim<600: sw=4 ts=4
 * }}} */
