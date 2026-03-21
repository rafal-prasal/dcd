/* $Id: conf.h,v 1.31 2006/08/29 15:50:49 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__CONF_H_INCLUDED__)
#define __CONF_H_INCLUDED__

/* read a line from FP and return the pointer to allocated storage */
char *read_whole_line(FILE *fp, int line_style);

/* parse config file */
char parse_config(int start, config_t *conf);

/* parse text file and play to public chat window */
void parse_messages(int raw, char **result, char *msgfile);

/* parse logins and fill login table */
void parse_logins();

/* parse logins and fill login table */
void parse_hublist();

/* verify login and password {{{ */
int validlogin(userrec_t *usr,char *login,char *pass);

/* validhost() - verify host access */
char validhost(userrec_t *usr,char *access_file,int deeper);

/* nick_allowed() - verify nick access */
int nick_allowed(userrec_t *usr,char *nick);

/* nick_allowed() - verify nick access */
void parse_nick_patterns();

/* penalties_read() - reading penalties file */
void penalties_read();

/* penalties_write() - writing penalties file */
void penalties_write();

#endif

/* VIM Settings {{{
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * soft-stop-width: 4
 * c indent on
 * End:
 * vim600: sw=4 ts=4 sts=4 cindent fdm=marker
 * vim<600: sw=4 ts=4
 * }}} */
