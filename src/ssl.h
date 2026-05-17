/* $Id: ssl.h,v 1.0 2026/05/17 11:00:00 rav Exp $ */

#if !defined( __SSL_H__INCLUDED__ )
#define __SSL_H__INCLUDED__

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
    SSL *ssl;
    SSL_CTX *ctx;
} ssl_t;

#endif /* __SSL_H__INCLUDED__ */
