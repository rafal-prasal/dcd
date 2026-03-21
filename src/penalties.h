/* $Id: penalties.h,v 1.19 2006/03/20 01:03:56 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined(__PENALTIES_H_INCLUDED__)
#define __PENALTIES_H_INCLUDED__

/* PENALTIES / user flags {{{ */
#define PENALTY_BANNED		1
#define PENALTY_NODL		2
#define PENALTY_NONE		0
#define PENALTY_NOPBLCHAT	4
#define PENALTY_NOPRVCHAT	8
#define PENALTY_NOSEARCH	16
/* end of PENALTIES / user flags }}} */

#define SRCH_IP	 1
#define SRCH_NICK	-1
#define SRCH_BOTH	0

/*checks if the user->ip is listed on >penalties< list and changes user->penalty
 to fit those from the list */
void user_penalty(userrec_t *usr);

/**/
int penalties_ip2itemnumber(char *ip);

/**/
void addpenalty(Tpenalty *penrec);

/**/
void delpenalty(int no);

/**/
void user_set_penalty(char *ip,char *nick, char *op,int penalty, unsigned long start_date,long duration, char *rsn, int only_memory);

/**/
int user_tst_penalty(userrec_t *usr, int penalty);

/**/
long user_exp_penalty(Tpenalty *penalty); 

/**/
int penalty_welcome(userrec_t *usr);

/**/
void penalprvmsg(userrec_t *to, char *op, char *fmt, ...);

/**/
void penalmsg(Tpenalty *penrec);

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

