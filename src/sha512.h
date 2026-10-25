/* SHA-512 (FIPS 180-4) — see sha512.c */

#if !defined( __vXhz__SHA512_H__INCLUDED__ )
#define __vXhz__SHA512_H__INCLUDED__

#include <stddef.h>

/* lowercase hex digest, NUL terminated (128 chars + NUL) */
#define SHA512_HEX_LEN 128

void sha512_hex(const void *data, size_t len, char out[SHA512_HEX_LEN + 1]);

/* Verify a supplied password against a stored dcd.users password field.
 * Stored hashes use an /etc/shadow-like "$id$salt$hash" layout:
 *	$sha512$<salt>$<hex(sha512(salt . password))>
 * Anything else is compared as plaintext (legacy entries).
 * Returns 1 on match, 0 otherwise. */
int password_match(const char *stored, const char *supplied);

#endif /* __vXhz__SHA512_H__INCLUDED__ */
