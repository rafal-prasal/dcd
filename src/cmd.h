/* $Id: cmd.h,v 1.5 2006/04/14 13:29:45 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CMD_H_INCLUDED__)
#define __CMD_H_INCLUDED__

typedef void (*funct_t)(void *); 

typedef struct
{
	char *cmd;
	funct_t funct;
}cmd_t;

/* linear search for command and function to execute */
void cmd_exec(cmd_t *cmd_set, char * cmd, void *param);

/* binary case sensitive search for command and function to execute */
void bin_cmd_exec(cmd_t *cmd_set, int n_cmd_set,  char * cmd, void *param);

/* not case sensitive binary search for command and function to execute 
   cmd is firstly set tolower() and then bin_cmd_exec is executed */
void case_bin_cmd_exec(cmd_t *cmd_set, int n_cmd_set,  char * cmd, void *param);

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
