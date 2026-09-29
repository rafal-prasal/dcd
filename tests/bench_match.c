/* Performance test: wildcard matcher (src/match.c) throughput.
   match() runs on every nick/pattern comparison during login checks;
   the absolute floor only guards against catastrophic complexity
   regressions and is checked in optimized builds. */

#include "pch.h"
#include "test.h"

#define ITER	300000

static const char *patterns[] = {
	"*admin*",
	"ad?in",
	"*hub-operator*",
	"192.168.*",
	"*.example.com",
	"\\*literal",
	"user?name*",
	"a*b*c*d*e",
};
#define NPAT (int)(sizeof(patterns)/sizeof(*patterns))

static const char *strings[] = {
	"TheAdministrator",
	"admin",
	"my-hub-operator-92",
	"192.168.25.4",
	"host.example.com",
	"*literal",
	"userXname_extra",
	"axxbyycczzddqqe",
};

int main(void)
{
	int i, r, hits = 0;
	double t0, dt;

	t0 = now_sec();
	for (r = 0; r < ITER; r++) {
		i = r % NPAT;
		hits += match((char *)patterns[i], (char *)strings[i]);
	}
	dt = now_sec() - t0;

	printf("match: %8.1f Mops/s (mixed patterns, %d hits expected)\n",
		(double)ITER / dt / 1e6, ITER);

	/* every pattern is designed to match its string */
	CHECK_INT(hits, ITER);

#ifdef NDEBUG
	/* very generous floor: catches accidental O(n^3)-style regressions */
	CHECK((double)ITER / dt > 5e5);
#endif

	printf("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures ? 1 : 0;
}
