/* $Id: cmd.udp.h,v 1.1 2006/08/27 16:42:40 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CMD_UDP_H__)
#define __CMD_UDP_H__

typedef struct{
	char *ip;
	char *cmd;
	char *param;	
	char *line;	
}udp_param_t;

/* run chat or console command */
void udp_cmd_exec(char *ip, char *line);

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
