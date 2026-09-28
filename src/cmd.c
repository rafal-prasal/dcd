/* $Id: cmd.c,v 1.10 2006-04-19 16:12:42 rav Exp $ */

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

/* ASCII-only helpers: command names are pure ASCII and must not depend
   on the current locale (the old code used tolower()/strcasecmp()) */

static unsigned char cmd_lower(unsigned char c)
{
	return (c>='A' && c<='Z') ? (unsigned char)(c+('a'-'A')) : c;
}

static int cmd_str_eq(const char *a, const char *b, int ci)
{
	if (!ci) return !strcmp(a,b);

	while (*a && *b)
	{
		if (cmd_lower((unsigned char)*a) != cmd_lower((unsigned char)*b)) return 0;
		a++;
		b++;
	}

	return !*a && !*b;
}

/* FNV-1a over the command name */
static unsigned cmd_hash(const char *s, int ci)
{
	unsigned h = 2166136261u;

	while (*s)
	{
		unsigned char c = (unsigned char)*s++;
		if (ci) c = cmd_lower(c);
		h = (h ^ c) * 16777619u;
	}

	return h;
}

void cmd_dict_build(cmd_dict_t *dict, const cmd_t *set, int case_insensitive)
{
	unsigned b;

	dict->set=set;
	dict->ci=case_insensitive;
	dict->n=0;

	for (b=0;b<CMD_DICT_BUCKETS;b++) dict->head[b]=-1;

	while (dict->n < CMD_DICT_MAXENTRIES && set[dict->n].cmd && set[dict->n].cmd[0])
	{
		unsigned i=dict->n;
		b = cmd_hash(set[i].cmd, case_insensitive) & (CMD_DICT_BUCKETS-1);

		dict->next[i]=dict->head[b];
		dict->head[b]=(short)i;

		dict->n++;
	}
}

void cmd_dict_exec(cmd_dict_t *dict, const char *cmd, void *param)
{
	const cmd_t *found=NULL;

	if (cmd && dict->n)
	{
		short i = dict->head[cmd_hash(cmd,dict->ci) & (CMD_DICT_BUCKETS-1)];

		while (i>=0)
		{
			if (cmd_str_eq(dict->set[i].cmd,cmd,dict->ci))
			{
				found=&dict->set[i];
				break;
			}
			i=dict->next[i];
		}
	}

	if (!found) found=&dict->set[dict->n]; /* default handler (terminator entry) */

	if (found->funct) (found->funct)(param);
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
