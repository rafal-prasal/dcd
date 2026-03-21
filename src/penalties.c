/* $Id: penalties.c,v 1.53 2007/02/19 20:59:19 rav Exp $ */

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

extern int NPENAL; /* penalties count */
extern int NUSERS; /* penalties count */

extern Tpenalty *penalties[MAXPENALTIES]; /* table of penalties */
extern userrec_t *user[MAXUSERS];	/* users table */

extern pthread_mutex_t mutex_penalties;	/* provides synchronization between threads */
extern int test_penalty_counter;

/*
penalties_find_id( crit, *str )

.description:
	gets the penalties id.

.args
	int crit		- IN - SRCH_IP
							SRCH_NICK
							SRCH_BOTH

	char *str		- IN - the searching string

.return values:
	-1		- the ip is not present on the penalties list
	value	- the number of the ithem containing the >ip< is >value<
{{{*/

int penalties_find_id(int crit, char *str)
{
	int i;

	for(i=0;i<NPENAL;i++)
		if (penalties[i] &&
			((crit>=0 && match(penalties[i]->ip, str)) ||
			 (crit<=0 && match(penalties[i]->nick, str)))) goto leave;
						 

	return -1;
	
leave:
	return i;	
} /* }}} */

/*
void user_penalty(*usr)

.description:
 checks if the user->ip is listed on >penalties< list and changes user->penalty
 to fit those from the list

.args:
	userrec_t *usr - IN(ip) & OUT(penalty) - userrec
{{{*/

void user_penalty(userrec_t *usr)
{
	int nr;//itemnumber

	nr=penalties_find_id(SRCH_IP,usr->ip);

	if (nr<0)
	{
		usr->penalty=NULL;
		return;
	}

	usr->penalty=penalties[nr];

} /* }}} */

/*
void addpenalty(*penrec)

.description:
 adds the peanlty to penalties list and userrecs

.args:
	Tpenalty *penrec - penalty record

{{{*/

void addpenalty(Tpenalty *penrec)
{

	/* FIXME: find penalty wich ends in shortest amount of time */
	if (NPENAL+1>MAXPENALTIES)
	{
		my_free(penrec->ip);
		my_free(penrec->reason);
		my_free(penrec->nick);
		my_free(penrec->op);
		my_free(penrec);
		return;
	}

	penalties[NPENAL++]=penrec; /* change the item when it exists */
	penalmsg(penrec);	/* display info about changed penalty; kicks baned */

}/* }}} */

/*
void delpenalty(no, block)

.description:
 removes the peanlty to penalties list and userrecs by itemnuber in penalties[]

.args:
	int no - itemnumber of user's penalty in penalties list
	int block - use mutexes while deleting??

{{{*/

void delpenalty( int no )
{
	int i;
	Tpenalty *penrec=penalties[no];

	if (!penrec) return;

	/* remove penalty from userrecs */
	for (i=0;i<NUSERS;i++)
		if (user[i] && user[i]->penalty==penrec) user[i]->penalty = NULL;

	/* remove penalty from penalties[] */

	penalties[no]=penalties[--NPENAL];
	penalties[NPENAL]=NULL;

	/* free penrec */
	my_free(penrec->ip);
	my_free(penrec->reason);
	my_free(penrec->nick);
	my_free(penrec->op);
	my_free(penrec);

}/* }}} */

/*
user_set_penalty()
.description:
	sets user's penalty

.args
	char *ip		- user's ip
	char *nick		- user's nick(only PENALTY_NONE)
	char *op		- punisher
	int penalty		- the penalty {0-none}
	unsigned long	start_date - START_DATE
	long duration	- duration of the penalty {in minutes?)
	char *reason	- reason of penalty

{{{*/

void user_set_penalty(char *ip,char *nick, char *op,int penalty, unsigned long start_date,long duration, char *rsn, int only_memory)
{
	int nr;		/* penalty list itemnumber */
	Tpenalty *penrec=NULL; /* penalty record (used to add new penalty) */
	char *reason=NULL;

	pthread_mutex_lock(&mutex_penalties);

	/* removing penalty (when penalty==PENALTY_NONE) {{{ */
	if (penalty==PENALTY_NONE)
	{

		nr=penalties_find_id(SRCH_IP,ip);
		if (nr<0) nr=penalties_find_id(SRCH_NICK,nick);

		//penalties are deleted while penalties_write
		if (nr>=0) penalties[nr]->end_date=penalties[nr]->start_date;
		goto leave;
	} /* }}} */

	/* if no reason */
	if (!rsn) my_duplicate("NO REASON",&reason);
	else my_duplicate(rsn,&reason);

	nr=penalties_find_id(SRCH_IP,ip); //punishments by ip ONLY!

// if (nr<0) nr=penalties_find_id(SRCH_NICK,nick);
//	pubmsg(NULL,"IP=%s, NICK=%s, NR=%d",ip,nick,nr);

		if (nr<0) /* if penalety does not yet exist */
		{
				penrec=(Tpenalty *) my_malloc(sizeof(Tpenalty));		/* create a penalty record... */
				memset(penrec,0,sizeof(Tpenalty));

				my_duplicate(ip,&penrec->ip);
				my_duplicate(nick,&penrec->nick);
				my_duplicate(op,&penrec->op);

				penrec->penalty=penalty;
				penrec->reason=reason;

				penrec->start_date=start_date;
				penrec->end_date=(unsigned long)duration+penrec->start_date;

				addpenalty(penrec); /* ...to add it */
				goto leave;
		}

	penalties[nr]->penalty = penalty;	/* penalty overwrites existing */

	my_duplicate(reason,&penalties[nr]->reason);	/* set the new reason of penalty */
	my_free(reason);

	my_duplicate(nick, &penalties[nr]->nick);	/* set the new nick */
	my_duplicate(op, &penalties[nr]->op);

	penalties[nr]->start_date = start_date;
	penalties[nr]->end_date = (unsigned long)duration+penalties[nr]->start_date;

	penalmsg(penalties[nr]);	/* display info about changed penalty; kicks baned */

leave:
	if (!only_memory) penalties_write();

	pthread_mutex_unlock(&mutex_penalties);
} /* }}} */

/*
user_exp_penalty(penalty)

.description:
	tests if penalty expired

.args
	Tpenalty *penalty

.return values:
	<=0: the >penalty< expired
	>0: the >penalty< duration
{{{*/

long user_exp_penalty(Tpenalty *penalty)
{

	return (long)((time_t)penalty->end_date-time(NULL));
} /* }}} */

/*
user_tst_penalty(usr, penalty, duration)

.description:
	tests user's for penalties

.args
	userrec_t *usr - IN - userrec
	int penalty			- IN - the penalty (0 - none)

.return values:
	0: the >penalty< does NOT match user's penalty, the user has no penalty;
	1: the >penalty< DOES match user's penalty
{{{*/

int user_tst_penalty(userrec_t *usr, int penalty)
{
	int code=0;

	if (usr->penalty)
	{
		code = (usr->penalty->penalty&penalty);

		// if penalty expired
		if (user_exp_penalty(usr->penalty)<=0) code = 0;
	}

	return code;
} /* }}} */

int penalty_welcome(userrec_t *usr)
{
	long time_, days, hours,sec,min;

	/*sOP cannot be punished*/
	if (usr->perm && strchr(usr->perm,'s')) return 0;

	/* get the penalties for the user*/
	user_penalty(usr);

	if (user_tst_penalty(usr,~PENALTY_NONE))
	{
		time_=user_exp_penalty(usr->penalty);

		days=time_/(long)(24*60*60);
		time_=time_%(24*60*60);

		hours=time_/(long)(60*60);
		time_=time_%(60*60);

		min=time_/(long)60;
		time_=time_%60;

		sec=time_;
		pubmsg(usr,"Your host(%s@%s) is punished with:",usr->penalty->nick,usr->penalty->ip);
		
		pubmsg(usr,"%s%s%s%s%s",(usr->penalty->penalty&PENALTY_BANNED)?"[ban] ":"",
					(usr->penalty->penalty&PENALTY_NODL)?"[nodl] ":"",
					(usr->penalty->penalty&PENALTY_NOSEARCH)?"[nosearch] ":"",
					(usr->penalty->penalty&PENALTY_NOPBLCHAT)?"[nopblchat] ":"",
					(usr->penalty->penalty&PENALTY_NOPRVCHAT)?"[noprvchat] ":"");

		pubmsg(usr,"Time left: %ld days and %ld:%ld:%ld",days,hours,min,sec);
		pubmsg(usr,"REASON: %s",usr->penalty->reason);
		pubmsg(usr,"-----------------------------------");

	}

	if (user_tst_penalty(usr,PENALTY_BANNED)) 
	{
		usr->reason=strdup("is banned");
	    user_set_state(usr,STATE_QUIT);
	    return 1;
	}

	return 0;
}

/* penalprvmsg(*to,*from,*fmt,...)
.description.
	sends the private message to from 'from' userrec 'to' userrec;
	the message contains type of penalty and reason;
.args.
	userrec_t *to	//recievers's userrec
	userrec_t *from	//sender's userrec
	char *message		//the message
{{{ */
void penalprvmsg(userrec_t *to, char *op, char *fmt, ...)
{
	char *str=NULL;
	va_list args;

	va_start(args , fmt);


	str=(char *)my_vsprintf(fmt,args);
	privmsg(to,op,"%s",str);
	va_end(args);

	if ((to->penalty) && (to->penalty->reason))
	{
		privmsg(to,op,"R e a s o n:");
		privmsg(to,op,"%s",to->penalty->reason==NULL?"NO REASON":to->penalty->reason);
	}
	my_free(str);
} /* }}} */

/* penalmsg()
.description.
	sends the penalty messages (pub, priv and debug)
	kicks banned users;
.args.
	Tpenalty *penrec	//info about penalty

{{{ */
void penalmsg(Tpenalty *penrec)
{
	int i;
	long days, 
		 hours, 
		 min, 
		 sec, 
		 time_;

	time_=user_exp_penalty(penrec);

	days=time_/(long)(24*60*60);
	time_=time_%(24*60*60);

	hours=time_/(long)(60*60);
	time_=time_%(60*60);
	min=time_/(long)60;
	time_=time_%60;

	sec=time_;

	//show info about punished nick
	pubmsg(NULL,"'%s' has no permission to: %s%s%s%s%s",penrec->nick,
							    (penrec->penalty&PENALTY_BANNED)?"[enter the hub (ban)] ":"",
							    (penrec->penalty&PENALTY_NODL)?"[download] ":"",
							    (penrec->penalty&PENALTY_NOSEARCH)?"[use search] ":"",
							    (penrec->penalty&PENALTY_NOPBLCHAT)?"[chat in public] ":"",
							    (penrec->penalty&PENALTY_NOPRVCHAT)?"[chat in private] ":"");

	pubmsg(NULL,"for %ld days and %ld:%ld:%ld because: %s.",days,
								hours,
								min,
								sec,
								penrec->reason);

	/*add the item to the penalties list add the penalties to userrec*/
	for (i=0;i<NUSERS;i++)
		if (user[i] && user[i]->perm && !strchr(user[i]->perm,'s') && ((penrec==user[i]->penalty) || (user[i]->ip && match(user[i]->ip,penrec->ip))))
		{
			user[i]->penalty=penrec;
			penalprvmsg(user[i],penrec->op,"You were punished as '%s'. You can't %s%s%s%s%s for %ld days and %ld:%ld:%ld.",
													 penrec->nick,
													 (penrec->penalty&PENALTY_BANNED)?"[enter the hub (ban)] ":"",
													 (penrec->penalty&PENALTY_NODL)?"[download] ":"",
													 (penrec->penalty&PENALTY_NOSEARCH)?"[use search] ":"",
													 (penrec->penalty&PENALTY_NOPBLCHAT)?"[chat in public] ":"",
													 (penrec->penalty&PENALTY_NOPRVCHAT)?"[chat in private] ":"",
													 days,
													 hours,
													 min,
													 sec);

			log_write(FAC_USER,PR_INFO,"'%s'@%s punished by '%s' with %s%s%s%s%s for %ld days and %ld:%ld:%ld because: %s.",
													 user[i]->nick,
													 penrec->ip,
													 penrec->op,
													 (penrec->penalty&PENALTY_BANNED)?"[ban] ":"",
													 (penrec->penalty&PENALTY_NODL)?"[nodl] ":"",
													 (penrec->penalty&PENALTY_NOSEARCH)?"[nosearch] ":"",
													 (penrec->penalty&PENALTY_NOPBLCHAT)?"[nopblchat] ":"",
													 (penrec->penalty&PENALTY_NOPRVCHAT)?"[noprvchat] ":"",
													 days,
													 hours,
													 min,
													 sec,
													 user[i]->penalty->reason);

			if (penrec->penalty&PENALTY_BANNED) 
			{
				user[i]->reason=strdup("is banned");
				user_set_state(user[i],STATE_QUIT);
			}
			
		}
} /* }}} */

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
