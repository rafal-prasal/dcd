/* $Id: net.h,v 1.34 2007/04/05 21:46:50 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __vXhz__NET_H__INCLUDED__ )
#define __vXhz__NET_H__INCLUDED__

#if !defined( MSG_NOSIGNAL )
#define MSG_NOSIGNAL 0
#endif

void disttcp(userrec_t *usr, char *msg);

/* used for distributing $UserIP <nick> <ip>| to all users */
void disttcp_userip(userrec_t *usr);

/* something like printf(), but to a socket tcp*/
void disttcpf(userrec_t *usr, const char *fmt,...);

void distudp(hub_t *usr, char *msg);

/* something like printf(), but to a socket udp */
void distudpf(hub_t *_hub, const char *fmt,...);

/* sends myinfo to user(s) if usr is in STATE_REGISTERED */
void dc_myinfo(userrec_t *to, userrec_t *usr);

/* receives data from a socket*/
int my_recv(userrec_t *, int max_length, char *message);

/* this is required, since after disconnect a socket is switched to a TIME_WAIT
 state and the listening port remains occupied for several minutes */
int setsockopts(int sock, so_opts_t *option);

/* send message to public chat window */
void pubmsg(userrec_t *to, const char *fmt, ...);

/* send message to private chat */
void privmsg(userrec_t *to, char *from, const char *fmt, ...);
/* disconnect a user with optional reason message */

void disconnect(userrec_t *usr);

/* strip_telnet() - strip telnet codes - from eggdrop {{{ */
void strip_telnet(userrec_t *usr, char *buf, int *len);

#endif /* __vXhz__NET_H__INCLUDED__ */

/* VIM Settings {{{
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * soft-stop-width: 4
 * c indent on
 * End:
 * vim600: sw=4 ts=4 sts=4 cindent fdm=marker
 * vim<600: sw=4 ts=4
 * }}} */

