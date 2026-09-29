/* Unit tests for the wildcard matcher in src/match.c
   (used for nick/IP patterns in nicks.allow and console.allow). */

#include "pch.h"
#include "test.h"

static void test_null_and_empty(void)
{
	/* null strings must never match */
	CHECK_INT(match(NULL, "abc"), 0);
	CHECK_INT(match("abc", NULL), 0);
	CHECK_INT(match("", "abc"), 0);
	CHECK_INT(match("abc", ""), 0);
}

static void test_plain(void)
{
	/* plain comparison is case-insensitive (strncasecmp-based) */
	CHECK_INT(match("abc", "abc"), 1);
	CHECK_INT(match("ABC", "abc"), 1);
	CHECK_INT(match("abc", "abd"), 0);

	/* without wildcards, lengths must agree */
	CHECK_INT(match("abc", "abcd"), 0);
	CHECK_INT(match("abcd", "abc"), 0);
}

static void test_star(void)
{
	CHECK_INT(match("*", "abc"), 1);
	CHECK_INT(match("*c", "abc"), 1);
	CHECK_INT(match("a*", "abc"), 1);
	CHECK_INT(match("*b*", "abc"), 1);
	CHECK_INT(match("*ab*", "xxabyy"), 1);
	CHECK_INT(match("a*c", "abcd"), 0);	/* anchored tail */
	CHECK_INT(match("a**b", "axxb"), 1);	/* redundant stars collapse */
	CHECK_INT(match("*c?", "abcd"), 1);	/* star + single-char wildcard */
	CHECK_INT(match("?*", "ab"), 1);
}

static void test_question(void)
{
	CHECK_INT(match("a?c", "abc"), 1);
	CHECK_INT(match("a?c", "ac"), 0);		/* '?' needs a char */
	CHECK_INT(match("?", ""), 0);
	CHECK_INT(match("??", "ab"), 1);
}

static void test_quoting(void)
{
	/* backslash quotes the next character */
	CHECK_INT(match("\\*", "*"), 1);	/* literal '*' */
	CHECK_INT(match("\\*", "abc"), 0);
	CHECK_INT(match("a\\?c", "a?c"), 1);	/* literal '?' */
	CHECK_INT(match("a\\?c", "abc"), 0);
	CHECK_INT(match("\\\\", "\\"), 1);	/* literal backslash */
}

static void test_percent_legacy(void)
{
	/* Legacy behavior: '%' only acts as a wildcard as part of a run
	   adjacent to '*'; on its own it is matched literally. Asserting
	   the observed semantics to pin them against regressions. */
	CHECK_INT(match("%", "abc"), 0);
	CHECK_INT(match("a%b", "ab"), 0);
	CHECK_INT(match("*%*", "x"), 1);	/* run collapsed into '*' */
	CHECK_INT(match("%*", "x"), 1);
}

static void test_patterns_real(void)
{
	/* shapes actually used in dcd nick/IP allow files */
	CHECK_INT(match("*admin*", "SuperAdmin92"), 1);
	CHECK_INT(match("ad?in", "admin"), 1);
	CHECK_INT(match("192.168.*", "192.168.5.7"), 1);	/* trailing '*' spans dots */
	CHECK_INT(match("192.168.*", "192.168.5"), 1);
	CHECK_INT(match("*.example.com", "a.example.com"), 1);
}

int main(void)
{
	RUN(test_null_and_empty);
	RUN(test_plain);
	RUN(test_star);
	RUN(test_question);
	RUN(test_quoting);
	RUN(test_percent_legacy);
	RUN(test_patterns_real);
	TEST_REPORT();
}
