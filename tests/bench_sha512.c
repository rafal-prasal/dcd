/* Performance test: SHA-512 password hashing (src/sha512.c via OpenSSL).
   sha512_hex()/password_match() run on every hashed-password login attempt;
   the absolute floors only guard against catastrophic regressions (e.g. a
   per-call provider re-fetch) and are checked in optimized builds. */

#include "pch.h"
#include "test.h"

#define SMALL_ITER 200000
#define BIG_ITER   200
#define MATCH_ITER 50000
#define BIG_LEN    (1u << 20)

int main(void)
{
	static char big[BIG_LEN];
	char out[SHA512_HEX_LEN + 1];
	char stored[192];
	const char *salt = "abcdefghij123456";
	const char *pass = "hunter2-let-me-in";
	double t0, dt, rate, small_rate, bulk_rate;
	int i, hits = 0;
	unsigned char acc = 0;

	/* pin correctness before timing */
	sha512_hex("abc", 3, out);
	CHECK_STR(out,
		"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
		"2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");

	/* warm up: first EVP_Q_digest() call may allocate the provider context */
	sha512_hex(salt, strlen(salt), out);

	/* password-sized inputs: the actual login path */
	t0 = now_sec();
	for (i = 0; i < SMALL_ITER; i++) {
		sha512_hex(pass, strlen(pass), out);
		acc ^= (unsigned char)out[0];
	}
	dt = now_sec() - t0;
	small_rate = (double)SMALL_ITER / dt;
	/* same input every round: XOR of identical digests cancels out */
	CHECK_INT(acc, 0);
	printf("sha512_hex(%2zu B): %8.1f kop/s  (%5.2f us/op)\n",
		strlen(pass), small_rate / 1e3, dt / SMALL_ITER * 1e6);

	/* bulk throughput: shows the digest primitive itself is fast */
	memset(big, 'x', sizeof(big));
	acc = 0;
	t0 = now_sec();
	for (i = 0; i < BIG_ITER; i++) {
		sha512_hex(big, BIG_LEN, out);
		acc ^= (unsigned char)out[0];
	}
	dt = now_sec() - t0;
	bulk_rate = (double)BIG_ITER * BIG_LEN / dt / (1u << 20);
	CHECK_INT(acc, 0);
	printf("sha512_hex(%zu MiB): %7.1f MiB/s\n",
		(size_t)BIG_LEN >> 20, bulk_rate);

	/* full login check: prefix parse + salted hash + constant-time compare */
	{
		char hex[SHA512_HEX_LEN + 1];
		char joined[64];
		size_t sl = strlen(salt), pl = strlen(pass);
		memcpy(joined, salt, sl);
		memcpy(joined + sl, pass, pl);
		sha512_hex(joined, sl + pl, hex);
		sprintf(stored, "$sha512$%s$%s", salt, hex);
	}

	t0 = now_sec();
	for (i = 0; i < MATCH_ITER; i++)
		hits += password_match(stored, pass);
	dt = now_sec() - t0;
	rate = (double)MATCH_ITER / dt;

	CHECK_INT(hits, MATCH_ITER);
	CHECK_INT(password_match(stored, "wrong-password"), 0);
	printf("password_match: %8.1f ops/s  (%5.1f us/op)\n",
		rate, dt / MATCH_ITER * 1e6);

#ifdef NDEBUG
	/* generous floors: sub-50us per password-sized hash, above 50 MiB/s
	   bulk, sub-200us per full login match */
	CHECK(small_rate > 2e4);
	CHECK(bulk_rate > 50.0);
	CHECK(rate > 5e3);
#endif

	printf("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures ? 1 : 0;
}
