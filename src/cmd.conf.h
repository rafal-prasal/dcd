/* $Id: cmd.conf.h,v 1.2 2006/04/02 17:30:58 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CMD_CONF_H__)
#define __CMD_CONF_H__

typedef struct {
	char *line;
	char *cmd;
	char *value;
	int start;
}conf_param_t;

/* opt_line() - parse command-line options */
void conf_cmd_exec(int start, char *line);

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
