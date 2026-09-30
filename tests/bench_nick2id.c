/* Performance test: nick2id() nick hash (src/misc.c) vs the linear
   strcasecmp scan over user[] it replaced. Asserts the hash resolves
   at least 2x faster at userlimit=1000 scale, where the old scan costs
   up to 1000 strcasecmp calls per nick-addressed command. */

#include "pch.h"
#include "test.h"

#define NUSERS_TEST	1000
#define ROUNDS		2000

static char names[NUSERS_TEST][16];

extern userrec_t *user[MAXUSERS];

static userrec_t *users[NUSERS_TEST];

static volatile long sink;

/* the pre-migration lookup: linear scan over the user table */
static int nick2id_linear(char *nick)
{
	int i;
	for (i = 0; i < NUSERS; i++)
		if (user[i] && user[i]->nick && !strcasecmp(user[i]->nick, nick))
			return i;
	return -1;
}

int main(void)
{
	int i, r, hits = 0;
	double t0, t_hash, t_lin;

	for (i = 0; i < NUSERS_TEST; i++) {
		sprintf(names[i], "User_%04d", i);
		users[i] = (userrec_t *)calloc(1, sizeof(userrec_t));
		userlist_add(users[i]);
		user_set_nick(users[i], names[i]);
	}
	CHECK_INT(NUSERS, NUSERS_TEST);

	/* warmup */
	for (i = 0; i < NUSERS_TEST; i++)
		if (nick2id(names[i]) >= 0) hits++;
	CHECK_INT(hits, NUSERS_TEST);

	t0 = now_sec();
	for (r = 0; r < ROUNDS; r++)
		for (i = 0; i < NUSERS_TEST; i++)
			sink += nick2id(names[i]);
	t_hash = now_sec() - t0;

	t0 = now_sec();
	for (r = 0; r < ROUNDS; r++)
		for (i = 0; i < NUSERS_TEST; i++)
			sink += nick2id_linear(names[i]);
	t_lin = now_sec() - t0;

	printf("nick2id hash:   %8.1f Mops/s\n", (double)ROUNDS * NUSERS_TEST / t_hash / 1e6);
	printf("baseline scan:  %8.1f Mops/s\n", (double)ROUNDS * NUSERS_TEST / t_lin / 1e6);

	/* hash must be clearly ahead of the scan it replaced */
	CHECK(t_hash < t_lin / 2.0);

	/* absolute sanity floor for optimized builds */
#ifdef NDEBUG
	CHECK((double)ROUNDS * NUSERS_TEST / t_hash > 1e6);
#endif

	for (i = 0; i < NUSERS_TEST; i++) {
		free(users[i]->nick);
		free(users[i]);
		user[i] = NULL;
	}
	NUSERS = 0;

	printf("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures ? 1 : 0;
}
