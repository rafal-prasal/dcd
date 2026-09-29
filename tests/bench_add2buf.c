/* Performance test: add2buf() in-place append (src/misc.c) vs the
   full-copy implementation it replaced. Asserts the in-place path is
   faster than the old malloc+strcpy+strcat path and prints per-chunk
   growth for regression visibility. */

#include "pch.h"
#include "test.h"

#define CHUNK	64
#define N_SMALL	5000
#define N_BIG	10000

static char chunk[CHUNK + 1];

/* the pre-optimization append: full copy of *buf on every call */
static int add2buf_naive(char **buf, char *string)
{
	char *tmp;
	int blen = 0, len;

	if (!string || !*string) return 0;
	if (*buf) blen = strlen(*buf);
	len = blen + 1 + (int)strlen(string);

	tmp = (char *)malloc(len);
	strcpy(tmp, *buf ? *buf : "");
	strcat(tmp, string);
	free(*buf);
	*buf = tmp;
	return len;
}

static double run_add2buf(int n, int (*appender)(char **, char *))
{
	char *buf = NULL;
	int i;
	double t0;

	t0 = now_sec();
	for (i = 0; i < n; i++)
		appender(&buf, chunk);
	t0 = now_sec() - t0;

	free(buf);		/* appender may have NULLed on MAXBLOCK */
	return t0;
}

int main(void)
{
	double t_small, t_big, t_naive;
	int i;

	memset(chunk, 'x', CHUNK);
	chunk[CHUNK] = 0;

	/* warmup + sanity: content must survive appends */
	{
		char *buf = NULL;
		for (i = 0; i < 10; i++) add2buf(&buf, chunk);
		CHECK_INT(strlen(buf), CHUNK * 10);
		free(buf);
	}

	t_small = run_add2buf(N_SMALL, add2buf);
	t_big = run_add2buf(N_BIG, add2buf);
	t_naive = run_add2buf(N_BIG, add2buf_naive);

	printf("add2buf %6d chunks: %8.1f us (%6.1f ns/chunk)\n",
		N_SMALL, t_small * 1e6, t_small / N_SMALL * 1e9);
	printf("add2buf %6d chunks: %8.1f us (%6.1f ns/chunk)\n",
		N_BIG, t_big * 1e6, t_big / N_BIG * 1e9);
	printf("naive full-copy %d chunks: %8.1f us (%6.1f ns/chunk)\n",
		N_BIG, t_naive * 1e6, t_naive / N_BIG * 1e9);

	/* in-place realloc append must beat the old full-copy path */
	CHECK(t_big < t_naive);

	/* Absolute floor for optimized builds. Note: add2buf() still runs
	   strlen(*buf) per call, so cost grows with buffer size (quadratic in
	   aggregate). The in-place optimization removed the extra copies —
	   the measured ~2x per-chunk growth from 5k to 10k chunks reflects
	   that remaining scan; a re-introduced full-copy path would push the
	   floor well past this bound. */
#ifdef NDEBUG
	CHECK(N_SMALL / t_small > 2e5);	/* < 5 us per append at small sizes */
#endif

	printf("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures ? 1 : 0;
}
