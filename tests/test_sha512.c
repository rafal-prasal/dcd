/* Unit tests for the SHA-512 primitive and the dcd.users password hash
   verification in src/sha512.c. */

#include "pch.h"
#include "test.h"

/* FIPS 180-4 / coreutils reference vectors */
static const char *ABC_HASH =
	"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
	"2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f";
static const char *EMPTY_HASH =
	"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
	"47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e";
/* sha512("sAlt01./" . "secret") */
static const char *SALT_SECRET_HASH =
	"d366a32ab2ce9f97bff8bc33d46cb026d7b115e7f7ca295153206ce21d305f58"
	"857a45d629a632e7e13024f117bab15083d7a63b76d4ddfba6e2cfd9b04cf730";
/* sha512("sAlt01./") */
static const char *SALT_ONLY_HASH =
	"98a68d5d17a06e3b88c56500161724281e04cd46ff151005d7ce7496759f2212"
	"ba03884950636905eafd41bfd5026f0617c1a74150e3b37dcc443461006e468e";

static void test_sha512_known_vectors(void)
{
	char out[SHA512_HEX_LEN + 1];

	sha512_hex("abc", 3, out);
	CHECK_STR(out, ABC_HASH);

	sha512_hex("", 0, out);
	CHECK_STR(out, EMPTY_HASH);

	sha512_hex("sAlt01./secret", 14, out);
	CHECK_STR(out, SALT_SECRET_HASH);
}

static void test_plaintext_entries(void)
{
	CHECK_INT(password_match("changeme", "changeme"), 1);
	CHECK_INT(password_match("changeme", "ChangeMe"), 0);
	CHECK_INT(password_match("changeme", "changem"), 0);
	CHECK_INT(password_match("changeme", ""), 0);
	CHECK_INT(password_match("changeme", NULL), 0);

	CHECK_INT(password_match("", ""), 1);
	CHECK_INT(password_match(NULL, "x"), 0);
	CHECK_INT(password_match(NULL, NULL), 0);
}

static void test_hashed_entries(void)
{
	char good[160];
	snprintf(good, sizeof(good), "$sha512$sAlt01./$%s", SALT_SECRET_HASH);

	CHECK_INT(password_match(good, "secret"), 1);
	CHECK_INT(password_match(good, "Secret"), 0);
	CHECK_INT(password_match(good, "secre"), 0);
	CHECK_INT(password_match(good, ""), 0);
	CHECK_INT(password_match(good, NULL), 0);

	/* empty password hashes to sha512(salt) */
	{
		char h[160];
		snprintf(h, sizeof(h), "$sha512$sAlt01./$%s", SALT_ONLY_HASH);
		CHECK_INT(password_match(h, ""), 1);
		CHECK_INT(password_match(h, "x"), 0);
	}

	/* wrong salt must not match */
	{
		char h[160];
		snprintf(h, sizeof(h), "$sha512$sAlt01./x$%s", SALT_SECRET_HASH);
		CHECK_INT(password_match(h, "secret"), 0);
	}
}

static void test_malformed_hashes(void)
{
	char buf[256];

	/* truncated / extended digest */
	snprintf(buf, sizeof(buf), "$sha512$sAlt01./$%.127s", SALT_SECRET_HASH);
	CHECK_INT(password_match(buf, "secret"), 0);
	snprintf(buf, sizeof(buf), "$sha512$sAlt01./$%sa", SALT_SECRET_HASH);
	CHECK_INT(password_match(buf, "secret"), 0);

	/* uppercase hex is not our lowercase format */
	{
		char upper[SHA512_HEX_LEN + 1];
		size_t i;
		for (i = 0; i < SHA512_HEX_LEN; i++) {
			char c = SALT_SECRET_HASH[i];
			upper[i] = (c >= 'a') ? (char)(c - 32) : c;
		}
		upper[SHA512_HEX_LEN] = 0;
		snprintf(buf, sizeof(buf), "$sha512$sAlt01./$%s", upper);
		CHECK_INT(password_match(buf, "secret"), 0);
	}

	/* non-hex digest */
	snprintf(buf, sizeof(buf), "$sha512$sAlt01./$%s",
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
	CHECK_INT(password_match(buf, "secret"), 0);

	/* missing digest / missing second '$' / empty or bad salt */
	CHECK_INT(password_match("$sha512$sAlt01./$", "secret"), 0);
	CHECK_INT(password_match("$sha512$sAlt01./", "secret"), 0);
	CHECK_INT(password_match("$sha512$", "secret"), 0);
	CHECK_INT(password_match("$sha512$$abc", "secret"), 0);
	CHECK_INT(password_match("$sha512$s!lt$abc", "secret"), 0);

	/* empty salt is invalid */
	snprintf(buf, sizeof(buf), "$sha512$$%s", SALT_SECRET_HASH);
	CHECK_INT(password_match(buf, "secret"), 0);

	/* a $sha512$-prefixed value is never treated as plaintext */
	CHECK_INT(password_match("$sha512$whatever", "$sha512$whatever"), 0);
}

int main(void)
{
	RUN(test_sha512_known_vectors);
	RUN(test_plaintext_entries);
	RUN(test_hashed_entries);
	RUN(test_malformed_hashes);
	TEST_REPORT();
}
