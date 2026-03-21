/* $Id: match.h,v 1.12 2006/03/20 01:03:55 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __MATCH_H__INCLUDED__ )
#define __MATCH_H__INCLUDED__

// used for matching strings against wildcards; returns 0 if no match
int match(char *wildcard,char *hostmask);

#endif /* __vXhz__MATCH_H__INCLUDED__ */

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

