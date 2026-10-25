/* Unit tests for the hash command dictionary in src/cmd.c */

#include "pch.h"
#include "test.h"

static int g_called;		/* id of last handler executed, -1 none */
static void *g_param;		/* param passed to last handler */

#define ID_NONE (-1)

static void h_default(void *p) { g_called = 0; g_param = p; }
static void h_one(void *p)     { g_called = 1; g_param = p; }
static void h_two(void *p)     { g_called = 2; g_param = p; }
static void h_three(void *p)   { g_called = 3; g_param = p; }

/* handler whose entry has a NULL funct: must be tolerated */
static void h_nul(void *p)     { g_called = 4; g_param = p; }

static void reset(void)
{
	g_called = ID_NONE;
	g_param = NULL;
}

static const cmd_t small_set[] = {
	{"one",    (funct_t)h_one},
	{"two",    (funct_t)h_two},
	{"three",  (funct_t)h_three},
	{NULL,     (funct_t)h_default},
};

static void test_exact_hit(void)
{
	static cmd_dict_t d;
	int param = 7;

	reset();
	cmd_dict_build(&d, small_set, 0);
	CHECK_INT(d.n, 3);

	cmd_dict_exec(&d, "one", &param);
	CHECK_INT(g_called, 1);
	CHECK(g_param == &param);

	cmd_dict_exec(&d, "two", NULL);
	CHECK_INT(g_called, 2);

	cmd_dict_exec(&d, "three", NULL);
	CHECK_INT(g_called, 3);
}

static void test_case_sensitive(void)
{
	static cmd_dict_t d;

	reset();
	cmd_dict_build(&d, small_set, 0);

	/* wrong case must NOT hit in case-sensitive mode -> default */
	cmd_dict_exec(&d, "ONE", NULL);
	CHECK_INT(g_called, 0);

	cmd_dict_exec(&d, "Two", NULL);
	CHECK_INT(g_called, 0);
}

static void test_case_insensitive(void)
{
	static cmd_dict_t d;
	char buf[8];

	reset();
	cmd_dict_build(&d, small_set, 1);

	cmd_dict_exec(&d, "ONE", NULL);
	CHECK_INT(g_called, 1);

	strcpy(buf, "tWo");
	cmd_dict_exec(&d, buf, NULL);
	CHECK_INT(g_called, 2);
	/* the dictionary must not mutate the caller's command string
	   (the removed case_bin_cmd_exec used to lowercase in place) */
	CHECK_STR(buf, "tWo");
}

static void test_miss_and_null(void)
{
	static cmd_dict_t d;

	reset();
	cmd_dict_build(&d, small_set, 0);

	cmd_dict_exec(&d, "bogus", NULL);
	CHECK_INT(g_called, 0);	/* default handler */

	cmd_dict_exec(&d, "", NULL);
	CHECK_INT(g_called, 0);	/* empty command -> default */

	cmd_dict_exec(&d, NULL, NULL);
	CHECK_INT(g_called, 0);	/* NULL command -> default, no crash */
}

static void test_null_handler(void)
{
	static const cmd_t set[] = {
		{"x", (funct_t)h_nul},
		{NULL, NULL},		/* default handler is NULL too */
	};
	static cmd_dict_t d;

	reset();
	cmd_dict_build(&d, set, 0);

	cmd_dict_exec(&d, "x", NULL);
	CHECK_INT(g_called, 4);

	reset();
	cmd_dict_exec(&d, "missing", NULL);
	CHECK_INT(g_called, ID_NONE);	/* NULL default: silently ignored */
}

#define MANY CMD_DICT_MAXENTRIES

static void test_capacity_and_chaining(void)
{
	static cmd_t big_set[MANY + 1];
	char names[MANY][8];
	int i;
	static cmd_dict_t d;

	for (i = 0; i < MANY; i++) {
		sprintf(names[i], "c%03d", i);
		big_set[i].cmd = names[i];
		big_set[i].funct = (funct_t)h_default; /* id 0 marker */
	}
	big_set[MANY].cmd = NULL;
	big_set[MANY].funct = (funct_t)h_one;	/* default handler */

	reset();
	cmd_dict_build(&d, big_set, 0);
	/* the full table up to capacity must be indexed */
	CHECK_INT(d.n, CMD_DICT_MAXENTRIES);

	/* every indexed entry must be findable (exercises bucket chains) */
	for (i = 0; i < (int)d.n; i++) {
		g_called = ID_NONE;
		cmd_dict_exec(&d, names[i], NULL);
		if (g_called != 0) {
			printf("FAIL entry %s not found in dictionary\n", names[i]);
			t_failures++;
			break;
		}
		t_checks++;
	}

	/* a name beyond the indexed range falls through to the default */
	g_called = ID_NONE;
	cmd_dict_exec(&d, "c999", NULL);
	CHECK_INT(g_called, 1);
}

static void test_empty_terminator(void)
{
	static const cmd_t set[] = {
		{"a",    (funct_t)h_one},
		{(char*)"", (funct_t)h_default},	/* empty cmd terminates */
		{"b",    (funct_t)h_two},		/* never indexed */
	};
	static cmd_dict_t d;

	reset();
	cmd_dict_build(&d, set, 0);
	CHECK_INT(d.n, 1);

	cmd_dict_exec(&d, "a", NULL);
	CHECK_INT(g_called, 1);

	g_called = ID_NONE;
	cmd_dict_exec(&d, "b", NULL);
	CHECK_INT(g_called, 0);	/* default handler */
}

static void test_telemetry(void)
{
	static cmd_dict_t td;
	char fbuf[512];

	reset();
	cmd_dict_build(&td, small_set, 0);
	CHECK_INT(td.count, 1);
	td.name = "tst";

	cmd_dict_exec(&td, "one", NULL);
	cmd_dict_exec(&td, "one", NULL);
	cmd_dict_exec(&td, "three", NULL);
	cmd_dict_exec(&td, "bogus", NULL);	/* default entry: index n */

	CHECK_INT((long)__atomic_load_n(&td.hits[0], __ATOMIC_RELAXED), 2);
	CHECK_INT((long)__atomic_load_n(&td.hits[1], __ATOMIC_RELAXED), 0);
	CHECK_INT((long)__atomic_load_n(&td.hits[2], __ATOMIC_RELAXED), 1);
	CHECK_INT((long)__atomic_load_n(&td.hits[td.n], __ATOMIC_RELAXED), 1);

	cmd_stats_format(fbuf, sizeof(fbuf));
	CHECK(strstr(fbuf, "tst one=2 three=1 (unknown)=1") != NULL);

	cmd_stats_reset();
	cmd_stats_format(fbuf, sizeof(fbuf));
	CHECK_STR(fbuf, "");

	/* nc dictionaries: handlers run but nothing is counted or reported */
	{
		static cmd_dict_t nd;

		cmd_dict_build_nc(&nd, small_set, 0);
		CHECK_INT(nd.count, 0);

		cmd_dict_exec(&nd, "one", NULL);
		CHECK_INT(g_called, 1);
		CHECK_INT((long)__atomic_load_n(&nd.hits[0], __ATOMIC_RELAXED), 0);

		cmd_stats_format(fbuf, sizeof(fbuf));
		CHECK_STR(fbuf, "");
	}
}

int main(void)
{
	RUN(test_exact_hit);
	RUN(test_case_sensitive);
	RUN(test_case_insensitive);
	RUN(test_miss_and_null);
	RUN(test_null_handler);
	RUN(test_capacity_and_chaining);
	RUN(test_empty_terminator);
	RUN(test_telemetry);
	TEST_REPORT();
}
