/* Minimal unit-test harness for dcd tests (no external deps). */

#if !defined(__DCD_TEST_H_INCLUDED__)
#define __DCD_TEST_H_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <time.h>

static int t_checks = 0;
static int t_failures = 0;

#define CHECK(cond) \
	do { \
		t_checks++; \
		if (!(cond)) { \
			printf("FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			t_failures++; \
		} \
	} while (0)

#define CHECK_INT(got, want) \
	do { \
		long t_g = (long)(got), t_w = (long)(want); \
		t_checks++; \
		if (t_g != t_w) { \
			printf("FAIL %s:%d: %s: got %ld, want %ld\n", \
				__FILE__, __LINE__, #got, t_g, t_w); \
			t_failures++; \
		} \
	} while (0)

#define CHECK_STR(got, want) \
	do { \
		const char *t_g = (got), *t_w = (want); \
		t_checks++; \
		if (!t_g || !t_w || strcmp(t_g, t_w)) { \
			printf("FAIL %s:%d: %s: got \"%s\", want \"%s\"\n", __FILE__, __LINE__, \
				#got, t_g ? t_g : "(null)", t_w ? t_w : "(null)"); \
			t_failures++; \
		} \
	} while (0)

#define RUN(fn) \
	do { \
		printf("== %s\n", #fn); \
		fn(); \
	} while (0)

#define TEST_REPORT() \
	do { \
		printf("%d checks, %d failures\n", t_checks, t_failures); \
		return t_failures ? 1 : 0; \
	} while (0)

__attribute__((unused))
static double now_sec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

#endif /* __DCD_TEST_H_INCLUDED__ */
