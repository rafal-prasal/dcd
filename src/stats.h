/* Hub runtime telemetry: byte totals, login/logout rotation.
 * See stats.c; counters are safe from any thread. */

#if !defined(__STATS_H_INCLUDED__)
#define __STATS_H_INCLUDED__

#include <stddef.h>

/* accumulate transferred bytes (0 for the direction that doesn't apply) */
void stats_add_tcp(size_t rx, size_t tx);
void stats_add_udp(size_t rx, size_t tx);

/* registration lifecycle (call on state transitions through
   STATE_REGISTERED and out to QUIT/QUIT_SHOW) */
void stats_user_registered(void);
void stats_user_left(void);

/* multi-line human-readable report into buf (no trailing newline) */
void stats_format(char *buf, size_t size);

void stats_reset(void);

#endif /* __STATS_H_INCLUDED__ */
