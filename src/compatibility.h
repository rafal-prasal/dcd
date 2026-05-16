/* $Id: compatibility.h,v 1.2 2007/04/05 21:46:49 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __vXhz__COMPATIBILITY_H__INCLUDED__ )
#define __vXhz__COMPATIBILITY_H__INCLUDED__

#ifdef INCLUDED_STRSEP
int strsep(char **, char *sep);
#endif

#ifdef INCLUDED_DAEMON
int daemon(int nochdir,int noclose);
#endif

#endif /* __vXhz__MISC_H__INCLUDED__ */

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

