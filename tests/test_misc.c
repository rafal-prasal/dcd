/* Unit tests for helpers in src/misc.c: add2buf, my_duplicate/my_free,
   make_path, uptime, flag_myinfo, lock_gen, get_progname, my_sprintf,
   nick2id/user_set_nick (nick hash). */

#include "pch.h"
#include "test.h"

extern config_t conf;
extern userrec_t *user[MAXUSERS];

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

/* nick2id / user_set_nick (nick hash) {{{ */

static userrec_t *mk_user(void)
{
	userrec_t *u = (userrec_t *)calloc(1, sizeof(userrec_t));
	userlist_add(u);
	return u;
}

static void test_nick2id_basic(void)
{
	userrec_t *a = mk_user(), *b = mk_user();

	/* users without a nick are not findable */
	CHECK_INT(nick2id("anything"), -1);

	user_set_nick(a, "Alice");
	user_set_nick(b, "Bob");

	CHECK_INT(nick2id("Alice"), 0);
	CHECK_INT(nick2id("alice"), 0);	/* case-insensitive, like the old scan */
	CHECK_INT(nick2id("BOB"), 1);
	CHECK_INT(nick2id("bob"), 1);
	CHECK_INT(nick2id("Carol"), -1);
	CHECK_INT(nick2id(""), -1);
	CHECK_INT(nick2id(NULL), -1);

	/* re-nicking drops the old key and keeps the record consistent */
	user_set_nick(a, "Alicia");
	CHECK_INT(nick2id("Alice"), -1);
	CHECK_INT(nick2id("alicia"), 0);
	free(a->nick); a->nick = NULL;
	free(b->nick); b->nick = NULL;
	free(a); free(b);
	user[0] = user[1] = NULL;
	NUSERS = 0;
}

static void test_nick2id_swap_remove(void)
{
	userrec_t *a = mk_user(), *b = mk_user(), *c = mk_user();

	user_set_nick(a, "Alice");
	user_set_nick(b, "Bob");
	user_set_nick(c, "Carol");

	/* deleting the middle user moves the last one into its slot; the
	   nick hash must follow the new index (userlist_del re-homes it) */
	userlist_del(b);
	CHECK_INT(NUSERS, 2);
	CHECK(user[1] == c);
	CHECK_INT(c->id, 1);
	CHECK_INT(nick2id("Carol"), 1);
	CHECK_INT(nick2id("Bob"), -1);
	CHECK_INT(nick2id("alice"), 0);

	/* deleting the last slot must not re-home a NULL user */
	userlist_del(c);
	CHECK_INT(NUSERS, 1);
	CHECK_INT(nick2id("Carol"), -1);
	CHECK_INT(nick2id("Alice"), 0);

	free(a->nick); a->nick = NULL;
	free(b->nick); b->nick = NULL;
	free(c->nick); c->nick = NULL;
	free(a); free(b); free(c);
	user[0] = NULL;
	NUSERS = 0;
}

static void test_nick2id_validnick(void)
{
	userrec_t *a = mk_user(), *probe;

	conf.conf_nallow = (char *)"";	/* the daemon always initializes this */

	user_set_nick(a, "Alice");

	probe = (userrec_t *)calloc(1, sizeof(userrec_t));
	probe->cons = 0;

	/* validnick() must reject a nick that is in use, case-insensitively */
	CHECK_INT(validnick(probe, "alice"), NICK_IS_ALREADY_USED);
	CHECK_INT(validnick(probe, "NewNick"), NICK_IS_OK);

	free(probe);
	free(a->nick); a->nick = NULL;
	free(a);
	user[0] = NULL;
	NUSERS = 0;
}

static void test_nick2id_many(void)
{
	int i;
	char nick[32];
	userrec_t *users[500];

	/* enough keys to exercise collisions and the backward-shift delete */
	for (i = 0; i < 500; i++) {
		users[i] = mk_user();
		sprintf(nick, "User_%03d", i);
		user_set_nick(users[i], nick);
	}

	for (i = 0; i < 500; i++) {
		sprintf(nick, "user_%03d", i);
		CHECK_INT(nick2id(nick), i);
	}

	/* delete every other user (forces probe-chain shifts); swap-remove
	   permutes the table, so verify against each survivor's own id */
	for (i = 499; i >= 1; i -= 2)
		userlist_del(users[i]);

	for (i = 0; i < 500; i += 2) {
		sprintf(nick, "USER_%03d", i);
		CHECK_INT(nick2id(nick), users[i]->id);
		CHECK_INT(user[users[i]->id], users[i]);
	}
	for (i = 1; i < 500; i += 2) {
		sprintf(nick, "User_%03d", i);
		CHECK_INT(nick2id(nick), -1);
	}

	for (i = 0; i < 500; i++) {
		free(users[i]->nick);
		users[i]->nick = NULL;
		free(users[i]);
	}
	for (i = 0; i < NUSERS; i++) user[i] = NULL;
	NUSERS = 0;
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
	RUN(test_nick2id_basic);
	RUN(test_nick2id_swap_remove);
	RUN(test_nick2id_validnick);
	RUN(test_nick2id_many);
	TEST_REPORT();
}
