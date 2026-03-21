/* $Id: log.h,v 1.1 2007/02/20 07:48:01 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __vXhz__DCDLOG_H__INCLUDED__ )
#define __vXhz__DCDLOG_H__INCLUDED__

#define LOG_LEVELS      7

#define LEV_ACCESS      1
#define LEV_DEBUG       2
#define LEV_PROTOCOL    4
#define LEV_DAEMON      8
#define LEV_USER        16
#define LEV_CHAT        32
#define LEV_NETWORK     64

#define FAC_ACCESS      0
#define FAC_USER        1
#define FAC_DEBUG       2
#define FAC_PROTOCOL    3
#define FAC_DAEMON      4
#define FAC_CHAT        5
#define FAC_NETWORK     6

#define PR_DEBUG       0
#define PR_INFO        1
#define PR_NOTICE      2
#define PR_WARNING     3
#define PR_CRITICAL    4
#define PR_ALERT       5
#define PR_ERROR       6
#define PR_EMERGENCY   7

void log_open();

void log_write(int facility, int priority, const char *fmt, ...);

void log_close();

#endif 

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
