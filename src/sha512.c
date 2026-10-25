/* SHA-512 (FIPS 180-4) via OpenSSL libcrypto.
 * Used to verify SHA-512 password hashes stored in dcd.users. */

#include <string.h>

#include <openssl/evp.h>

#include "sha512.h"

void sha512_hex(const void *data, size_t len, char out[SHA512_HEX_LEN + 1])
{
	unsigned char digest[64];
	size_t i;

	if (EVP_Q_digest(NULL, "SHA512", NULL, data, len, digest, NULL) != 1) {
		*out = 0;
		return;
	}

	for (i = 0; i < sizeof(digest); i++) {
		*out++ = "0123456789abcdef"[digest[i] >> 4];
		*out++ = "0123456789abcdef"[digest[i] & 0xf];
	}
	*out = 0;
}

/* password_match() - verify a supplied password against a stored one.
 * Stored hashes use an /etc/shadow-like "$id$salt$hash" layout:
 *	$sha512$<salt>$<hex(sha512(salt . password))>
 * with the salt restricted to the /etc/shadow alphabet [a-zA-Z0-9./].
 * Any other stored value is treated as plaintext (legacy entries). */

#define PW_HASH_PREFIX "$sha512$"
#define PW_HASH_PREFIX_LEN (sizeof(PW_HASH_PREFIX) - 1)
#define PW_SALT_MAX 64
#define PW_INPUT_MAX 4096

static int salt_char_ok(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9') || c == '.' || c == '/';
}

static int hex_char_ok(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

int password_match(const char *stored, const char *supplied)
{
	const char *salt, *sep, *hex;
	size_t salt_len, pass_len, i;
	unsigned char diff = 0;
	char computed[SHA512_HEX_LEN + 1];

	if (!stored) return 0;

	if (strncmp(stored, PW_HASH_PREFIX, PW_HASH_PREFIX_LEN))
		return supplied && !strcmp(stored, supplied);

	if (!supplied) return 0;

	salt = stored + PW_HASH_PREFIX_LEN;
	sep = strchr(salt, '$');
	if (!sep) return 0;

	salt_len = (size_t)(sep - salt);
	if (!salt_len || salt_len > PW_SALT_MAX) return 0;
	for (i = 0; i < salt_len; i++)
		if (!salt_char_ok(salt[i])) return 0;

	hex = sep + 1;
	if (strlen(hex) != SHA512_HEX_LEN) return 0;
	for (i = 0; i < SHA512_HEX_LEN; i++)
		if (!hex_char_ok(hex[i])) return 0;

	pass_len = strlen(supplied);
	if (salt_len + pass_len > PW_INPUT_MAX) return 0;

	{
		char buf[PW_SALT_MAX + PW_INPUT_MAX];
		memcpy(buf, salt, salt_len);
		memcpy(buf + salt_len, supplied, pass_len);
		sha512_hex(buf, salt_len + pass_len, computed);
	}

	/* constant-time digest comparison */
	for (i = 0; i < SHA512_HEX_LEN; i++)
		diff |= (unsigned char)(computed[i] ^ hex[i]);

	return diff == 0;
}
