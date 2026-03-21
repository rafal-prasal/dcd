/* $Id: cmd.opt_line.h,v 1.4 2006/03/20 01:03:54 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CMD_OPT_LINE_H__)
#define __CMD_OPT_LINE_H__

typedef struct {
	char *opt;
	char *value;
}opt_param_t;

/* opt_line() - parse command-line options */
void opt_line(int argc, char * argv[] );

#endif

/* VIM Settings {{{
* Local variables:
* tab-width: 14
* c-basic-offset: 4
* soft-stop-width: 4
* c indent on
* End:
* vim600: sw=4 ts=4 sts=4 cindent fdm=marker
* vim<600: sw=4 ts=4
* }}} */
