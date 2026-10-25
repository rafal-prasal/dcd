/* $Id$ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * Runtime telemetry: traffic totals, user rotation (logins/logouts,
 * concurrent online, EWMA of logins/hour). Counters are read and
 * updated from the listener threads, cleaner and user handlers, so
 * plain counters use GCC atomics (relaxed); the login-rate EWMA is
 * double arithmetic and is guarded by its own mutex.
 *
 * Command dispatch counters live in the dictionaries themselves
 * (see cmd.c / cmd_dict_exec and cmd_stats_format).
 * ----------------------------------------------------------------
 * }}} */

#include "pch.h"

#define STATS_ALPHA 0.02	/* EWMA over roughly the last 50 logins */

static unsigned long rx_tcp, tx_tcp, rx_udp, tx_udp;
static unsigned long logins, logouts, online, max_online;

static pthread_mutex_t stats_throttle = PTHREAD_MUTEX_INITIALIZER;
static double login_rate;		/* EWMA, logins per hour */
static time_t last_login;

void stats_add_tcp(size_t rx, size_t tx)
{
	if (rx) __atomic_fetch_add(&rx_tcp, rx, __ATOMIC_RELAXED);
	if (tx) __atomic_fetch_add(&tx_tcp, tx, __ATOMIC_RELAXED);
}

void stats_add_udp(size_t rx, size_t tx)
{
	if (rx) __atomic_fetch_add(&rx_udp, rx, __ATOMIC_RELAXED);
	if (tx) __atomic_fetch_add(&tx_udp, tx, __ATOMIC_RELAXED);
}

void stats_user_registered(void)
{
	time_t now=time(NULL);
	unsigned long cur, want;

	__atomic_fetch_add(&logins, 1, __ATOMIC_RELAXED);

	cur=__atomic_fetch_add(&online, 1, __ATOMIC_RELAXED)+1;
	do {
		want=__atomic_load_n(&max_online,__ATOMIC_RELAXED);
		if (cur<=want) break;
	} while (!__atomic_compare_exchange_n(&max_online,&want,cur,
			0,__ATOMIC_RELAXED,__ATOMIC_RELAXED));

	pthread_mutex_lock(&stats_throttle);

	if (last_login)
	{
		double inst, gap=(double)(now-last_login);
		if (gap<1.0) gap=1.0;
		inst=3600.0/gap;
		login_rate = login_rate ? (1.0-STATS_ALPHA)*login_rate + STATS_ALPHA*inst
					: inst;
	}
	last_login=now;

	pthread_mutex_unlock(&stats_throttle);
}

void stats_user_left(void)
{
	unsigned long cur;

	__atomic_fetch_add(&logouts, 1, __ATOMIC_RELAXED);

	cur=__atomic_load_n(&online,__ATOMIC_RELAXED);
	while (cur && !__atomic_compare_exchange_n(&online,&cur,cur-1,
			0,__ATOMIC_RELAXED,__ATOMIC_RELAXED))
		cur=__atomic_load_n(&online,__ATOMIC_RELAXED);
}

unsigned long stats_online(void)
{
	return __atomic_load_n(&online,__ATOMIC_RELAXED);
}

unsigned long stats_max_online(void)
{
	return __atomic_load_n(&max_online,__ATOMIC_RELAXED);
}

unsigned long stats_total_logins(void)
{
	return __atomic_load_n(&logins,__ATOMIC_RELAXED);
}

static void stats_bytes(char *buf, size_t size, unsigned long n)
{
	unsigned long v=n;
	int unit=0;		/* 0=B 1=KB 2=MB 3=GB 4=TB */
	static const char *names[]={"B","KB","MB","GB","TB"};

	while (v>=1000 && unit<4)
	{
		v/=1000;
		unit++;
	}

	snprintf(buf,size,"%lu%s",v,names[unit]);
}

void stats_format(char *buf, size_t size)
{
	char rx[32], tx[32], urx[32], utx[32];
	time_t now=time(NULL);
	double uph, ewma;
	unsigned long nlogins=__atomic_load_n(&logins,__ATOMIC_RELAXED),
		nlogouts=__atomic_load_n(&logouts,__ATOMIC_RELAXED),
		nonline=__atomic_load_n(&online,__ATOMIC_RELAXED),
		nmax=__atomic_load_n(&max_online,__ATOMIC_RELAXED);

	stats_bytes(rx,sizeof(rx),__atomic_load_n(&rx_tcp,__ATOMIC_RELAXED));
	stats_bytes(tx,sizeof(tx),__atomic_load_n(&tx_tcp,__ATOMIC_RELAXED));
	stats_bytes(urx,sizeof(urx),__atomic_load_n(&rx_udp,__ATOMIC_RELAXED));
	stats_bytes(utx,sizeof(utx),__atomic_load_n(&tx_udp,__ATOMIC_RELAXED));

	uph = (now>starttime && nlogins) ? (double)nlogins*3600.0/(double)(now-starttime) : 0.0;

	pthread_mutex_lock(&stats_throttle);
	ewma=login_rate;
	pthread_mutex_unlock(&stats_throttle);

	snprintf(buf,size,
		"TCP in: %s  TCP out: %s\n"
		"UDP in: %s  UDP out: %s\n"
		"logins: %lu  logouts: %lu\n"
		"online: %lu  max online: %lu\n"
		"rotation: %.2f logins/h avg, %.2f logins/h (EWMA)",
		rx,tx,urx,utx,nlogins,nlogouts,nonline,nmax,uph,ewma);
}

void stats_reset(void){
	__atomic_store_n(&rx_tcp,0,__ATOMIC_RELAXED);
	__atomic_store_n(&tx_tcp,0,__ATOMIC_RELAXED);
	__atomic_store_n(&rx_udp,0,__ATOMIC_RELAXED);
	__atomic_store_n(&tx_udp,0,__ATOMIC_RELAXED);
	__atomic_store_n(&logins,0,__ATOMIC_RELAXED);
	__atomic_store_n(&logouts,0,__ATOMIC_RELAXED);
	/* keep max_online at least at the currently online count */
	__atomic_store_n(&max_online,__atomic_load_n(&online,__ATOMIC_RELAXED),
		__ATOMIC_RELAXED);

	pthread_mutex_lock(&stats_throttle);
	login_rate=0.0;
	last_login=0;
	pthread_mutex_unlock(&stats_throttle);
}

/* VIM Settings {{{
* Local variables:
* tab-width: 4
* c-basic-offset: 4
* End:
* vim600: sw=4 ts=4 sts=4 cindent fdm=marker
* vim<600: sw=4 ts=4
* }}} */
