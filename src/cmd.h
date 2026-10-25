/* $Id: cmd.h,v 1.5 2006-04-14 13:29:45 rav Exp $ */

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

#include <stddef.h>

typedef void (*funct_t)(void *); 

typedef struct
{
	char *cmd;
	funct_t funct;
}cmd_t;

/*
   Hash dictionary over a command table.

   The table is a NULL-terminated (or empty-cmd-terminated) array of cmd_t;
   the terminator entry carries the default handler that is executed when
   no command matches. Build the dictionary once (it chains entries into
   buckets), then dispatch with cmd_dict_exec() - O(1) average instead of
   the previous per-call bsearch/linear scan.
 */

#define CMD_DICT_BUCKETS 128u	/* must be a power of two */
#define CMD_DICT_MAXENTRIES 96	/* searchable entries (excluding default) */

#define CMD_DICT_STATS_MAX 4	/* counted dictionaries (dc, chat, udp) */

typedef struct
{
	const cmd_t *set;
	short head[CMD_DICT_BUCKETS];	/* bucket heads, -1 = empty */
	short next[CMD_DICT_MAXENTRIES];	/* per-entry chain, -1 = end */
	unsigned n;				/* searchable entry count */
	int ci;					/* case-insensitive lookups */
	int count;				/* 0: skip dispatch telemetry */
	const char *name;			/* report name for "stats" (NULL: uncounted) */
	/* per-entry dispatch hits; hits[n] is the default (unknown) handler */
	unsigned long hits[CMD_DICT_MAXENTRIES + 1];
} cmd_dict_t;

/* index a command table into the dictionary; call once before first use.
   cmd_dict_build() also enables dispatch telemetry and registers the
   dictionary for the console "stats" report; cmd_dict_build_nc() builds
   an identical dictionary with telemetry left out (config/option parsers
   that share the dispatcher but whose "commands" are not hub traffic). */
void cmd_dict_build(cmd_dict_t *dict, const cmd_t *set, int case_insensitive);
void cmd_dict_build_nc(cmd_dict_t *dict, const cmd_t *set, int case_insensitive);

/* execute the handler registered for cmd (or the default entry's handler) */
void cmd_dict_exec(cmd_dict_t *dict, const char *cmd, void *param);

/* append "name cmd=N ..." lines for dictionaries with telemetry enabled
   (see cmd_dict_build; the caller sets dict->name to be included) */
void cmd_stats_format(char *buf, size_t size);
void cmd_stats_reset(void);

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
