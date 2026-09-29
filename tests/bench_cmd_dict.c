/* Performance test: hash command dictionary (src/cmd.c) vs the linear
   strcmp scan it replaced. Asserts the dictionary dispatches at least
   2x faster on a conf_set-sized table (65 entries), where a miss costs
   the linear scan ~65 strcmp calls and the dictionary one FNV-1a pass. */

#include "pch.h"
#include "test.h"

#define NTAB	65
#define ROUNDS	200000

static char names[NTAB][16];
static cmd_t tab[NTAB + 1];

static volatile long sink;

static void handler(void *p) { (void)p; sink++; }
static void def_handler(void *p) { (void)p; sink += 100; }

/* the pre-migration dispatch: linear scan until empty/NULL cmd */
static void linear_exec(const char *cmd)
{
	int i;
	for (i = 0; i < NTAB; i++) {
		if (!strcmp(tab[i].cmd, cmd)) {
			handler(NULL);
			return;
		}
	}
	def_handler(NULL);
}

int main(void)
{
	cmd_dict_t d;
	int i, r;
	double t0, t_dict, t_lin, t_miss;
	long dict_ops = 0;

	for (i = 0; i < NTAB; i++) {
		sprintf(names[i], "conf_opt_%02d", i);
		tab[i].cmd = names[i];
		tab[i].funct = handler;
	}
	tab[NTAB].cmd = NULL;
	tab[NTAB].funct = def_handler;

	cmd_dict_build(&d, tab, 0);
	CHECK_INT(d.n, NTAB);

	/* warmup */
	for (i = 0; i < NTAB; i++) cmd_dict_exec(&d, names[i], NULL);

	t0 = now_sec();
	for (r = 0; r < ROUNDS; r++)
		for (i = 0; i < NTAB; i++) {
			cmd_dict_exec(&d, names[i], NULL);
			dict_ops++;
		}
	t_dict = now_sec() - t0;

	t0 = now_sec();
	for (r = 0; r < ROUNDS; r++)
		for (i = 0; i < NTAB; i++)
			linear_exec(names[i]);
	t_lin = now_sec() - t0;

	/* misses exercise the dictionary's default path vs a full scan */
	t0 = now_sec();
	for (r = 0; r < ROUNDS; r++)
		cmd_dict_exec(&d, "no_such_option", NULL);
	t_miss = now_sec() - t0;

	printf("cmd_dict: hits   %8.1f Mops/s\n", dict_ops / t_dict / 1e6);
	printf("cmd_dict: misses %8.1f Mops/s\n", (double)ROUNDS / t_miss / 1e6);
	printf("baseline linear scan: %8.1f Mops/s\n", (double)ROUNDS * NTAB / t_lin / 1e6);

	/* dictionary must be clearly ahead of the scan it replaced */
	CHECK(t_dict < t_lin / 2.0);

	/* absolute sanity floor for optimized builds */
#ifdef NDEBUG
	CHECK(dict_ops / t_dict > 1e7);
#endif

	printf("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures ? 1 : 0;
}
