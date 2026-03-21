/* $Id: cmd.dc.h,v 1.3 2006/03/20 01:03:53 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CMD_DC_H__)
#define __CMD_DC_H__

typedef struct{
	userrec_t *usr;
	char *cmd;
	char *param;	
	char *line;	
}dc_param_t;

/* run chat or console command */
void dc_cmd_exec(userrec_t *usr, char *line);

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
