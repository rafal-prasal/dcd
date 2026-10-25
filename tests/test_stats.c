/* Unit tests for the runtime telemetry in src/stats.c */

#include "pch.h"
#include "test.h"

static char buf[4096];

static void test_reset_empty(void)
{
	stats_reset();
	stats_format(buf, sizeof(buf));

	CHECK(strstr(buf, "TCP in: 0B") != NULL);
	CHECK(strstr(buf, "TCP out: 0B") != NULL);
	CHECK(strstr(buf, "logins: 0") != NULL);
	CHECK(strstr(buf, "max online: 0") != NULL);
}

static void test_bytes(void)
{
	stats_reset();
	stats_add_tcp(1500, 2500);
	stats_add_udp(1000, 999);
	stats_add_tcp(0, 0);		/* no-op direction */

	stats_format(buf, sizeof(buf));

	CHECK(strstr(buf, "TCP in: 1KB") != NULL);
	CHECK(strstr(buf, "TCP out: 2KB") != NULL);
	CHECK(strstr(buf, "UDP in: 1KB") != NULL);
	CHECK(strstr(buf, "UDP out: 999B") != NULL);
}

static void test_rotation(void)
{
	stats_reset();

	stats_user_registered();
	stats_user_registered();
	stats_user_registered();
	stats_user_left();

	stats_format(buf, sizeof(buf));

	CHECK(strstr(buf, "logins: 3") != NULL);
	CHECK(strstr(buf, "logouts: 1") != NULL);
	CHECK(strstr(buf, "online: 2") != NULL);
	CHECK(strstr(buf, "max online: 3") != NULL);

	/* never count online below zero */
	stats_user_left();
	stats_user_left();
	stats_user_left();
	stats_user_left();

	stats_format(buf, sizeof(buf));
	CHECK(strstr(buf, "online: 0") != NULL);
	CHECK(strstr(buf, "logouts: 5") != NULL);
}

static void test_reset_keeps_online(void)
{
	stats_reset();
	stats_user_registered();
	stats_user_registered();
	stats_reset();

	stats_format(buf, sizeof(buf));

	CHECK(strstr(buf, "logins: 0") != NULL);
	CHECK(strstr(buf, "logouts: 0") != NULL);
	/* counters are gone but currently online users must stay visible */
	CHECK(strstr(buf, "online: 2") != NULL);
	CHECK(strstr(buf, "max online: 2") != NULL);
}

int main(void)
{
	RUN(test_reset_empty);
	RUN(test_bytes);
	RUN(test_rotation);
	RUN(test_reset_keeps_online);
	TEST_REPORT();
}
