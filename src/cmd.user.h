/* $Id: cmd.user.h,v 1.21 2006/08/28 16:22:26 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CMD_USER_H_INCLUDED__)
#define __CMD_USER_H_INCLUDED__

/* visibility const for fmthln {{{*/
#define VIS_CHAT	1 // fmthln line is visible only in chat
#define VIS_CONS	2 // fmthln line is visible only in console
#define VIS_ALL	3 // fmthln line is visible in chat and in console
/* }}} */

typedef struct{
	userrec_t *usr;
	char *cmd;
	char *param;	
	char *line;	
} chat_param_t;

/* run chat or console command */
void chat_cmd_exec( userrec_t *usr,char *line, char *line_copy);

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
