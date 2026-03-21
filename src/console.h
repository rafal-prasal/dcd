/* $Id: console.h,v 1.9 2006/03/20 01:03:54 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __CONSOLE_H__INCLUDED__ )
#define __CONSOLE_H__INCLUDED__

/* Telnet codes. See "TELNET Protocol Specification" (RFC 854) and
 "TELNET Echo Option" (RFC 875) for details. {{{ */

#define TLN_AYT		246		/* Are You There	*/
#define TLN_WILL	251		/* Will			*/
#define TLN_WILL_C	"\373"
#define TLN_WONT	252		/* Won't		*/
#define TLN_WONT_C	"\374"
#define TLN_DO		253		/* Do			*/
#define TLN_DO_C	"\375"
#define TLN_DONT	254		/* Don't		*/
#define TLN_DONT_C	"\376"
#define TLN_IAC		255		/* Interpret As Command	*/
#define TLN_IAC_C	"\377"
#define TLN_ECHO	1		/* Echo			*/
#define TLN_ECHO_C	"\001"

/* end of telnet codes }}} */

void addcons(int sock,char *remote);

char manage_cons(userrec_t *usr,char *line);

void listen_thread_cons(void *args);

#endif /* __vXhz__MAIN_H__INCLUDED__ */

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

