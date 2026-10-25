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

/* dispatch telemetry registry: built dictionaries with count=1 and a
   name are reported by the console "stats" command */

#define CMD_DICT_REGISTRY_MAX 16

static cmd_dict_t *cmd_dicts[CMD_DICT_REGISTRY_MAX];
static int cmd_dicts_n=0;

static void cmd_dict_register(cmd_dict_t *dict)
{
	int i;

	for (i=0;i<cmd_dicts_n;i++)
		if (cmd_dicts[i]==dict) return;

	if (cmd_dicts_n<CMD_DICT_REGISTRY_MAX) cmd_dicts[cmd_dicts_n++]=dict;
}

static void cmd_dict_unregister(cmd_dict_t *dict)
{
	int i;

	for (i=0;i<cmd_dicts_n;i++)
		if (cmd_dicts[i]==dict)
		{
			cmd_dicts[i]=cmd_dicts[--cmd_dicts_n];
			return;
		}
}

void cmd_dict_build(cmd_dict_t *dict, const cmd_t *set, int case_insensitive)
{
	unsigned b;

	dict->set=set;
	dict->ci=case_insensitive;
	dict->count=1;
	dict->name=NULL;
	dict->n=0;
	memset(dict->hits,0,sizeof(dict->hits));

	for (b=0;b<CMD_DICT_BUCKETS;b++) dict->head[b]=-1;

	while (dict->n < CMD_DICT_MAXENTRIES && set[dict->n].cmd && set[dict->n].cmd[0])
	{
		unsigned i=dict->n;
		b = cmd_hash(set[i].cmd, case_insensitive) & (CMD_DICT_BUCKETS-1);

		dict->next[i]=dict->head[b];
		dict->head[b]=(short)i;

		dict->n++;
	}

	cmd_dict_register(dict);
}

void cmd_dict_build_nc(cmd_dict_t *dict, const cmd_t *set, int case_insensitive)
{
	cmd_dict_build(dict, set, case_insensitive);
	dict->count=0;
	cmd_dict_unregister(dict);
}

void cmd_dict_exec(cmd_dict_t *dict, const char *cmd, void *param)
{
	const cmd_t *found=NULL;
	unsigned idx;

	if (cmd && dict->n)
	{
		short i = dict->head[cmd_hash(cmd,dict->ci) & (CMD_DICT_BUCKETS-1)];

		while (i>=0)
		{
			if (cmd_str_eq(dict->set[i].cmd,cmd,dict->ci))
			{
				found=&dict->set[i];
				idx=(unsigned)i;
				break;
			}
			i=dict->next[i];
		}
	}

	if (!found) { idx=dict->n; found=&dict->set[dict->n]; } /* default handler (terminator entry) */

	if (dict->count)
		__atomic_fetch_add(&dict->hits[idx], 1, __ATOMIC_RELAXED);

	if (found->funct) (found->funct)(param);
}

void cmd_stats_reset(void)
{
	int d;

	for (d=0;d<cmd_dicts_n;d++)
		if (cmd_dicts[d]->count)
			memset(cmd_dicts[d]->hits,0,sizeof(cmd_dicts[d]->hits));
}

void cmd_stats_format(char *buf, size_t size)
{
	int d, i, off=0, named, after;

	if (!size) return;
	buf[0]=0;

	for (d=0;d<cmd_dicts_n;d++)
	{
		cmd_dict_t *dict=cmd_dicts[d];

		if (!dict->count || !dict->name) continue;

		named=off;
		off+=snprintf(buf+off,size-off,"%s",dict->name);
		if (off>=(int)size-1) { off=size-1; break; }
		after=off;

		for (i=0;i<=(int)dict->n;i++)
		{
			unsigned long h=__atomic_load_n(&dict->hits[i],__ATOMIC_RELAXED);

			if (!h) continue;

			off+=snprintf(buf+off,size-off," %s=%lu",
				i==(int)dict->n ? "(unknown)" : dict->set[i].cmd, h);

			if (off>=(int)size-1) { off=size-1; break; }
		}

		if (off==after) off=named;	/* no hits: roll the name back */
		else buf[off++]='\n';

		if (off>=(int)size-1) { off=size-1; break; }
	}

	buf[off]=0;
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
