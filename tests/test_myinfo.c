/* Unit tests for $MyINFO parsing (fill_myinfo) and nick validation
   (validnick) from src/misc.c. */

#include "pch.h"
#include "test.h"

extern config_t conf;
extern userrec_t *user[];
extern int NUSERS;

/* format: $ALL <nick> <desc>$ $<ctype><flag>$<email>$<share>$ */
static const char *GOOD = "$ALL MyNick SomeDesc$ $Conn\x01$me@dcd.pl$1234567890$";

static userrec_t usr;
static char nick_buf[] = "MyNick";	/* userrec_t.nick is a pointer */
static char info_buf[1024];
static myinfo_t mi;

static void setup(void)
{
	memset(&usr, 0, sizeof(usr));
	memset(&mi, 0, sizeof(mi));
	usr.nick = nick_buf;
	usr.cons = 1;	/* console user: skips login/password checks */

	conf.conf_nallow = (char *)"";	/* validnick() derefs this */
	conf.allow_non_us_ascii_nicks = 0;
}

static int run_fill(const char *src)
{
	strncpy(info_buf, src, sizeof(info_buf) - 1);
	info_buf[sizeof(info_buf) - 1] = 0;
	return fill_myinfo(&usr, &mi, info_buf, (char *)src);
}

static void test_fill_ok(void)
{
	CHECK_INT(run_fill(GOOD), MyINFO_OK);
	CHECK_STR(mi.desc, "SomeDesc");
	CHECK_STR(mi.ctype, "Conn");	/* raw flag byte stripped from ctype */
	CHECK_INT(mi.flag, 1);
	CHECK_STR(mi.email, "me@dcd.pl");
	CHECK(mi.share == 1234567890ULL);
	CHECK_STR(mi.cache, GOOD);	/* cache holds the original line */
}

static void test_fill_errors(void)
{
	CHECK_INT(run_fill("$AL MyNick D$ $C\x01$a@b$1$"), MyINFO_ALL);
	CHECK_INT(run_fill("$ALL OtherNick D$ $C\x01$a@b$1$"), MyINFO_NICK);

	/* description longer than DESC_LEN-1 */
	{
		char bad[512], filler[300];
		memset(filler, 'd', 260);
		filler[260] = 0;
		sprintf(bad, "$ALL MyNick %s$ $C\x01$a@b$1$", filler);
		CHECK_INT(run_fill(bad), MyINFO_DESC);
	}

	/* the single space between the two '$' after desc is missing */
	CHECK_INT(run_fill("$ALL MyNick D$$C\x01$a@b$1$"), MyINFO_SPACE);

	/* empty connection token */
	CHECK_INT(run_fill("$ALL MyNick D$ $$a@b$1$"), MyINFO_SPEED);

	/* email longer than EMAIL_LEN-1 (64) */
	{
		char bad[512], filler[80];
		memset(filler, 'e', 70);
		filler[70] = 0;
		sprintf(bad, "$ALL MyNick D$ $C\x01$%s$1$", filler);
		CHECK_INT(run_fill(bad), MyINFO_EMAIL);
	}

	/* share longer than SHARE_LEN-1 (30) */
	{
		char bad[512], filler[40];
		memset(filler, '9', 35);
		filler[35] = 0;
		sprintf(bad, "$ALL MyNick D$ $C\x01$a@b$%s$", filler);
		CHECK_INT(run_fill(bad), MyINFO_SHARE);
	}

	/* missing trailing '$' after share */
	CHECK_INT(run_fill("$ALL MyNick D$ $C\x01$a@b$1"), MyINFO_SHARE);
}

/* security regressions found by flawfinder/cppcheck review */
static void test_fill_security(void)
{
	/* flag must be a raw protocol byte in 1..11, not arbitrary text */
	CHECK_INT(run_fill("$ALL MyNick D$ $ConnA$a@b$1$"), MyINFO_FLAG);
	CHECK_INT(run_fill("$ALL MyNick D$ $Conn\x0c$a@b$1$"), MyINFO_FLAG);
	CHECK_INT(run_fill("$ALL MyNick D$ $Conn\x00dummy$a@b$1$"), MyINFO_FLAG);

	/* trailing garbage after the final '$' used to overflow myinfo->cache
	   via strcpy(line); must be rejected instead (cache is 467 bytes) */
	{
		char bad[1024];
		char x[441];

		memset(x, 'X', 440);
		x[440]=0;
		snprintf(bad,sizeof(bad),
			"$ALL MyNick SomeDesc$ $Conn\x01$me@dcd.pl$1234567890$%s$", x);
		CHECK_INT(run_fill(bad), MyINFO_LINE);
	}

	/* flag 1..11 accepted (raw bytes, including high values) */
	CHECK_INT(run_fill("$ALL MyNick D$ $C\x0b$a@b$1$"), MyINFO_OK);
}

/* validnick {{{ */

static void test_validnick_basic(void)
{
	userrec_t probe;
	memset(&probe, 0, sizeof(probe));
	probe.cons = 1;

	CHECK_INT(validnick(&probe, NULL), NICK_IS_EMPTY);
	CHECK_INT(validnick(&probe, ""), NICK_IS_EMPTY);

	/* NICK_LEN is 32: strlen+1>NICK_LEN -> at most 31 chars */
	CHECK_INT(validnick(&probe, "01234567890123456789012345678901"), NICK_IS_TOO_LONG);	/* 32 */
	CHECK_INT(validnick(&probe, "0123456789012345678901234567890"), NICK_IS_OK);		/* 31 */

	CHECK_INT(validnick(&probe, "#hash"), NICK_IS_WITH_INVALID_CHARACTERS);
	CHECK_INT(validnick(&probe, "a$b"), NICK_IS_WITH_INVALID_CHARACTERS);
	CHECK_INT(validnick(&probe, "a:b"), NICK_IS_WITH_INVALID_CHARACTERS);
	CHECK_INT(validnick(&probe, "a<b"), NICK_IS_WITH_INVALID_CHARACTERS);
	CHECK_INT(validnick(&probe, "a>b"), NICK_IS_WITH_INVALID_CHARACTERS);
	CHECK_INT(validnick(&probe, "a?b"), NICK_IS_WITH_INVALID_CHARACTERS);
	CHECK_INT(validnick(&probe, "with space"), NICK_IS_WITH_INVALID_CHARACTERS);

	CHECK_INT(validnick(&probe, "Hub"), NICK_IS_RESERVED);
	CHECK_INT(validnick(&probe, "hub"), NICK_IS_RESERVED);	/* case-insensitive */
	CHECK_INT(validnick(&probe, "Hub-Security"), NICK_IS_RESERVED);

	CHECK_INT(validnick(&probe, "RegularNick"), NICK_IS_OK);
} /* }}} */

static void test_validnick_in_use(void)
{
	static userrec_t existing;

	memset(&existing, 0, sizeof(existing));
	user[0] = &existing;
	existing.id = 0;
	NUSERS = 1;
	user_set_nick(&existing, "Bob");

	CHECK_INT(validnick(&usr, "Bob"), NICK_IS_ALREADY_USED);
	/* comparison is case-insensitive */
	CHECK_INT(validnick(&usr, "bOb"), NICK_IS_ALREADY_USED);
	CHECK_INT(validnick(&usr, "NotBob"), NICK_IS_OK);

	free(existing.nick);
	existing.nick = NULL;
	user[0] = NULL;
	NUSERS = 0;
} /* }}} */

int main(void)
{
	setup();
	RUN(test_fill_ok);
	RUN(test_fill_errors);
	RUN(test_fill_security);
	RUN(test_validnick_basic);
	RUN(test_validnick_in_use);
	TEST_REPORT();
}
