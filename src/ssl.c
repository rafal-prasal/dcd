/* $Id$ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * Implementation of the optional TLS support declared in ssl.h.
 * ----------------------------------------------------------------
 * }}} */

#include "pch.h"

/* total deadline for a client TLS handshake on a listen thread */
#define SSL_HANDSHAKE_TIMEOUT_MS 15000

extern config_t conf;

static SSL_CTX *ssl_server_ctx=NULL;

/* ssl_init() - build the server context from the certificate/key {{{ */
int ssl_init(void)
{
	SSL_CTX *ctx;
	const char *cert=conf.ssl_cert_file,
			 *key=(conf.ssl_key_file && *conf.ssl_key_file) ? conf.ssl_key_file : conf.ssl_cert_file;

	if (!cert || !*cert)
		return -1;

	if (!(ctx=SSL_CTX_new(TLS_server_method())))
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"SSL_CTX_new() failed: %s.",ERR_error_string(ERR_get_error(),NULL));
		return -2;
	}

	SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION);
	SSL_CTX_set_options(ctx,SSL_OP_NO_COMPRESSION);

	if (SSL_CTX_use_certificate_chain_file(ctx,cert)<=0)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"Can't load TLS certificate '%s': %s.",cert,ERR_error_string(ERR_get_error(),NULL));
		SSL_CTX_free(ctx);
		return -2;
	}

	if (SSL_CTX_use_PrivateKey_file(ctx,key,SSL_FILETYPE_PEM)<=0)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"Can't load TLS private key '%s': %s.",key,ERR_error_string(ERR_get_error(),NULL));
		SSL_CTX_free(ctx);
		return -2;
	}

	if (SSL_CTX_check_private_key(ctx)<=0)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"TLS certificate/key mismatch ('%s' / '%s').",cert,key);
		SSL_CTX_free(ctx);
		return -2;
	}

	ssl_server_ctx=ctx;

	log_write(FAC_DAEMON,PR_INFO,"TLS enabled: certificate '%s', key '%s' (client port %u, https port %u; reconfigure requires a restart).",
		cert,key,conf.listen_port_ssl,conf.listen_port_https);

	return 0;
} /* }}} */

int ssl_available(void)
{
	return ssl_server_ctx!=NULL;
}

/* ssl_accept_socket() - nonblocking TLS server handshake {{{ */
SSL *ssl_accept_socket(int sock)
{
	SSL *ssl;
	struct pollfd pfd;
	struct timespec start,now;
	int waited=0,r;

	if (!ssl_server_ctx) return NULL;

	if (!(ssl=SSL_new(ssl_server_ctx)))
	{
		log_write(FAC_NETWORK,PR_ERROR,"SSL_new() failed: %s.",ERR_error_string(ERR_get_error(),NULL));
		return NULL;
	}

	if (SSL_set_fd(ssl,sock)<=0)
	{
		log_write(FAC_NETWORK,PR_ERROR,"SSL_set_fd() failed: %s.",ERR_error_string(ERR_get_error(),NULL));
		SSL_free(ssl);
		return NULL;
	}

	clock_gettime(CLOCK_MONOTONIC,&start);

	while((r=SSL_accept(ssl))<=0)
	{
		int err=SSL_get_error(ssl,r),left;

		if (err!=SSL_ERROR_WANT_READ && err!=SSL_ERROR_WANT_WRITE)
		{
			log_write(FAC_NETWORK,PR_INFO,"TLS handshake failed on fd %d: %s.",sock,ERR_error_string(ERR_get_error(),NULL));
			SSL_free(ssl);
			return NULL;
		}

		clock_gettime(CLOCK_MONOTONIC,&now);
		waited+=(int)((now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000);
		start=now;

		if (waited>=SSL_HANDSHAKE_TIMEOUT_MS)
		{
			log_write(FAC_NETWORK,PR_INFO,"TLS handshake on fd %d timed out.",sock);
			SSL_free(ssl);
			return NULL;
		}

		left=SSL_HANDSHAKE_TIMEOUT_MS-waited;

		pfd.fd=sock;
		pfd.events=(err==SSL_ERROR_WANT_READ)?POLLIN:POLLOUT;
		pfd.revents=0;

		if (poll(&pfd,1,left)<=0 && errno!=EINTR)
		{
			log_write(FAC_NETWORK,PR_INFO,"TLS handshake on fd %d interrupted: %s.",sock,strerror(errno));
			SSL_free(ssl);
			return NULL;
		}
	}

	return ssl;
} /* }}} */

int ssl_pending(void *vssl)
{
	return vssl ? (int)SSL_pending((SSL *)vssl) : 0;
}

void ssl_clean_close(int sock)
{
	struct linger lg;

	lg.l_onoff=0;
	lg.l_linger=0;
	setsockopt(sock,SOL_SOCKET,SO_LINGER,&lg,sizeof(lg));
}

void ssl_reject_socket(int sock)
{
	char scratch[512];

	/* drain whatever libssl left in the kernel receive queue: unread data
	   makes close() emit RST, destroying the TLS alert we just wrote */
	while(recv(sock,scratch,sizeof(scratch),MSG_DONTWAIT)>0) {}

	close(sock);
}

void ssl_destroy(void *vssl)
{
	SSL *ssl=(SSL *)vssl;

	if (!ssl) return;

	/* best-effort close_notify; never block on a nonblocking socket */
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

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
