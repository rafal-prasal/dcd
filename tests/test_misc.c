/* Unit tests for helpers in src/misc.c: add2buf, my_duplicate/my_free,
   make_path, uptime, flag_myinfo, lock_gen, get_progname, my_sprintf. */

#include "pch.h"
#include "test.h"

extern config_t conf;

/* add2buf {{{ */

static void test_add2buf_basic(void)
{
	char *buf = NULL;
	int r;

	r = add2buf(&buf, "abc");
	CHECK_STR(buf, "abc");
	CHECK_INT(r, 4);		/* length incl. NUL of the appended part */

	r = add2buf(&buf, "de");
	CHECK_STR(buf, "abcde");
	CHECK_INT(r, 6);

	/* empty and NULL appends are no-ops returning 0 */
	CHECK_INT(add2buf(&buf, ""), 0);
	CHECK_STR(buf, "abcde");
	CHECK_INT(add2buf(&buf, NULL), 0);
	CHECK_STR(buf, "abcde");

	my_free(buf);
	CHECK(buf == NULL);
}

static void test_add2buf_maxblock(void)
{
	char *buf = (char *)malloc(8);
	char *big;
	int r;

	/* my_realloc() refuses sizes above MAXBLOCK and frees the original:
	   add2buf must null the caller's pointer instead of leaving it dangling */
	strcpy(buf, "seed");
	big = (char *)malloc(MAXBLOCK + 1);
	memset(big, 'a', MAXBLOCK);
	big[MAXBLOCK] = 0;

	r = add2buf(&buf, big);
	CHECK_INT(r, 0);
	CHECK(buf == NULL);		/* no dangling pointer */

	free(big);
} /* }}} */

/* my_duplicate / my_free {{{ */

static void test_my_duplicate(void)
{
	char *p = NULL;

	my_duplicate("hello", &p);
	CHECK_STR(p, "hello");

	/* re-duplicating replaces (and frees) the previous copy */
	my_duplicate("world", &p);
	CHECK_STR(p, "world");

	my_free(p);
	CHECK(p == NULL);

	/* my_free on an already-NULL pointer must be a safe no-op */
	my_free(p);
	CHECK(p == NULL);

	/* my_free1(NULL) (no pointer-to-pointer at all) must not crash */
	my_free1(NULL);
} /* }}} */

/* make_path {{{ */

static void test_make_path(void)
{
	char *file;

	file = strdup("dcd.users");
	make_path("/etc/dcd", &file);
	CHECK_STR(file, "/etc/dcd/dcd.users");
	my_free(file);

	/* trailing slash in dir must not produce '//' */
	file = strdup("x.conf");
	make_path("/etc/dcd/", &file);
	CHECK_STR(file, "/etc/dcd/x.conf");
	my_free(file);

	/* file already containing directories keeps only its basename */
	file = strdup("/tmp/deep/nicks.allow");
	make_path("/etc", &file);
	CHECK_STR(file, "/etc/nicks.allow");
	my_free(file);

	/* NULL and empty inputs are left untouched */
	file = NULL;
	make_path("/etc", &file);
	CHECK(file == NULL);

	file = strdup("");
	make_path("/etc", &file);
	CHECK_STR(file, "");
	my_free(file);
} /* }}} */

/* uptime {{{ */

static void test_uptime(void)
{
	char str[32];
	time_t now, up = 86400 + 3600 + 120 + 5;	/* 1 day, 01:02:05 */

	/* a start time in the future clamps to zero */
	time(&now);
	uptime(now + 10, str);
	CHECK_STR(str, "00:00:00");

	uptime(now - up, str);
	/* allow one second of drift across the call */
	CHECK(0 == strcmp(str, "1 day, 01:02:05") ||
	      0 == strcmp(str, "1 day, 01:02:04"));

	uptime(now - 45, str);
	/* sub-minute uptime renders as HH:MM:SS */
	CHECK(0 == strcmp(str, "00:00:45") ||
	      0 == strcmp(str, "00:00:44"));
} /* }}} */

/* flag_myinfo {{{ */

static void test_flag_myinfo(void)
{
	myinfo_t mi;

	memset(&mi, 0, sizeof(mi));

	mi.flag = 0;
	CHECK_STR(flag_myinfo(&mi), "");
	mi.flag = 1;
	CHECK_STR(flag_myinfo(&mi), "Normal");
	mi.flag = 2;
	CHECK_STR(flag_myinfo(&mi), "Away");
	mi.flag = 3;
	CHECK_STR(flag_myinfo(&mi), "Away");
	mi.flag = 8;
	CHECK(0 == strncmp(flag_myinfo(&mi), "Fireball", 8));

	/* out-of-range flag values (client-supplied!) must fall back to "" */
	mi.flag = 12;
	CHECK_STR(flag_myinfo(&mi), "");
	mi.flag = -1;				/* 0xFF unsigned: was OOB read */
	CHECK_STR(flag_myinfo(&mi), "");
} /* }}} */

/* lock_gen / get_progname / my_sprintf {{{ */

static void test_lock_gen(void)
{
	char lock[SALT_LEN];
	int i;

	memset(lock, 0, sizeof(lock));
	lock_gen(lock);

	CHECK_INT(strlen(lock), SALT_LEN - 1);
	CHECK(0 == strncmp(lock, "EXTENDEDPROTOCOL", 16));

	/* random tail must stay inside the printable range used (33..92) */
	for (i = 16; i < SALT_LEN - 1; i++) {
		if ((unsigned char)lock[i] < 33 || (unsigned char)lock[i] > 33 + 59) {
			printf("FAIL lock_gen byte %d out of range: %d\n", i, lock[i]);
			t_failures++;
			break;
		}
	}
	t_checks++;
}

static void test_get_progname(void)
{
	CHECK_STR(get_progname((char *)"/usr/sbin/dcd"), "dcd");
	CHECK_STR(get_progname((char *)"dcd"), "dcd");
	CHECK_STR(get_progname((char *)"a/b/c"), "c");
}

static void test_my_sprintf(void)
{
	char *p, *in;

	p = my_sprintf("%s=%d", "answer", 42);
	CHECK_STR(p, "answer=42");
	my_free(p);

	/* force the growth loop: output far beyond the initial 8 KiB buffer */
	in = (char *)malloc(10001);
	memset(in, 'a', 10000);
	in[10000] = 0;

	p = my_sprintf("%s!", in);
	CHECK(p != NULL);
	if (p) {
		CHECK_INT(strlen(p), 10001);
		CHECK_INT(p[10000], '!');
		my_free(p);
	}
	free(in);
} /* }}} */

int main(void)
{
	RUN(test_add2buf_basic);
	RUN(test_add2buf_maxblock);
	RUN(test_my_duplicate);
	RUN(test_make_path);
	RUN(test_uptime);
	RUN(test_flag_myinfo);
	RUN(test_lock_gen);
	RUN(test_get_progname);
	RUN(test_my_sprintf);
	TEST_REPORT();
}
