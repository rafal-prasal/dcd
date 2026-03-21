/* $Id: cmd.c,v 1.10 2006/04/19 16:12:42 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#include "pch.h"

void cmd_exec(cmd_t *cmd_set, char *cmd, void *param)
{
	int i=0;
	
	while(cmd_set[i].cmd && cmd_set[i].cmd[0])
	{
		if(!strcmp(cmd_set[i].cmd,cmd)) break;
		i++;
	}
	
	if (cmd_set[i].funct) (cmd_set[i].funct)(param);
}

int cmdcmp(const void *_cmd, const void *_cmd_e)
{
	cmd_t *cmd_e= (cmd_t *)_cmd_e;
	char *cmd = (char *)_cmd;
		
	return strcmp(cmd, cmd_e->cmd);
}

void string_tolower(char *cmd)
{
	int i=0;
	
	while(cmd[i])
	{
		cmd[i] = tolower(cmd[i]);
		i++;
	}		
}

void bin_cmd_exec(cmd_t *cmd_set, int n_cmd_set, char *cmd, void *param)
{
	size_t n = n_cmd_set;	
	size_t size = sizeof(cmd_t);
	cmd_t *found=NULL;
						
	found = (cmd_t *)bsearch((void *)cmd, (void *)cmd_set, n-1, size, cmdcmp);
	
	if (!found) found = (cmd_t *)&cmd_set[n-1];
	
	if (found->funct) (found->funct)(param);
}

void case_bin_cmd_exec(cmd_t *cmd_set, int n_cmd_set, char *cmd, void *param)
{
	string_tolower(cmd);
	bin_cmd_exec(cmd_set, n_cmd_set, cmd, param);
}

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
