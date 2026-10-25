/* $Id: cmd.user.c,v 1.140 2007/03/01 13:41:22 rav Exp $ */

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
extern config_t conf;		/* configuration data */

extern volatile sig_atomic_t rehash;

extern pthread_mutex_t mutex_myinfo;

/* the assistant function {{{ */

/* chat_nick2id(*usr,*nick)
.description.
			gets the id for the nick; if the nick is not specified or user does not exist
			apropriate message is shown to the usr

.args.
			userrec_t *usr	-IN-	 sending user's userec
			char *nick				-IN-	 the nick, whose id we want

.return values.
			<value> - the id of the user
			-1	 - user not exists or nick not specified
{{{ */
int chat_nick2id(userrec_t *usr, char *nick)
{
	int n;

	if (!nick || !*nick)
	{
		pubmsg(usr,"\tUser nick needed");
		pubmsg(usr,"========================================");
		return -1;
	}
	n=nick2id(nick);

	if (n<0)
	{
		pubmsg(usr,"\tUser '%s' not found",nick);
		pubmsg(usr,"========================================");
		return -1;
	}
	return n;
} /* }}} */

/* fmthln(*usr, *perm, console, visibility, *commnd, *description)
.description.
		displays formatted help line to >usr< whose usr->perm matches one of the >perm<
		the line format is
		in chat: <HUB>#commmand		 <- decription
		in cons: commmand				<- decription

.args.
		userrec_t *usr - IN -	the user record; if usr is console user, does not display '#' before commnd
		char *perm			- IN - contains permissions i.e:"cas"; only users with one of thease will see the line
		int console		- IN - determines if line is visible from console, chat or both
		char *commnd		- IN - contains the command; if no specified, does not display '#' before commnd
		char *description	- IN - contains description of the command; if no description is specified, display commnd as <HUB> commnd ; else display <HUB> (empty line)

.exaple use.
		fmthln(usr,"cas","Test",NULL);	---displays-->	'<HUB> Test'
		fmthln(usr,"cas",NULL,"Test");	---displays-->	'					Test'
		fmthln(usr,"cas","Axe","Test");	---displays-->	'#Axe				Test'

{{{*/
void fmthln(userrec_t *usr,char *perm, int visibility, char *commnd, char *description)
{
	if ((perm) && (usr->perm) &&(
		!(strchr(usr->perm,'s') && strchr(perm,'s')) &&
		!(strchr(usr->perm,'c') && strchr(perm,'c')) &&
		!(strchr(usr->perm,'a') && strchr(perm,'a')) )) return;

	if (!(usr->cons && visibility&VIS_CONS) && !(!(usr->cons) && visibility&VIS_CHAT)) return;

	//if user is console user or no commnd is specified, display '#' before commnd
	if (description) pubmsg(usr,(commnd)?"%c%-30s<- %s":"%c%-30s %s" ,(usr->cons==0)?((!commnd)?' ':'#'):' ',(commnd)?commnd:"",description);
	//if no description is specified, display commnd as pubmsg; else display empty line as pubmsg
	else pubmsg(usr,"%s",(commnd)?commnd:"");
}/* }}} */

/* opbreak - user with less permissions can not run a command on OP or sOP {{{ */
int opbreak(userrec_t *op,userrec_t *usr, char *commnd)
{
	if(!commnd)
	{
		pubmsg(usr,"ERROR: in 'opbreak': command not specified; returning 1; ");
		return 1;
	}

	if (usr->perm!=NULL && op->perm!=NULL)
	{
		if (op->cons && strchr(op->perm,'s')) return 0;

		if (strchr(op->perm,'a') && strchr(usr->perm,'a'))
		{
			pubmsg(op,"\tYou are trying to %s OP", commnd);
			pubmsg(op,"========================================");
			return 1;
		}

		if (strchr(usr->perm,'s'))
		{
			pubmsg(op,"\tYou are trying to %s sOP", commnd);
			pubmsg(op,"========================================");
			pubmsg(usr,"'%s' is trying to %s you.", op->nick,commnd);
			return 1;
		}
	}

	return 0;

}/* }}} */

/* }}} */
/* the COMMAND functions {{{ */

/* cmdhelp - shows usr the help screen {{{*/
void chat_help( chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	if (!usr->cons) disttcp(usr,line);

	fmthln(usr,	NULL,	VIS_ALL,	"========================================",	NULL);
	fmthln(usr,	NULL,	VIS_ALL,	"HELP",	NULL);
	fmthln(usr,	NULL,	VIS_ALL,	"========================================", NULL);
	fmthln(usr,	NULL,	VIS_ALL,	"[commands listed by categories]",			NULL);
	fmthln(usr,	NULL,	VIS_ALL, 	NULL,										NULL);

	fmthln(usr,	NULL,	VIS_ALL,	"-----------------------------------------",NULL);
	fmthln(usr,	NULL,	VIS_ALL,	"[info]:",	NULL);
	fmthln(usr,	NULL,	VIS_ALL,	"-----------------------------------------",NULL);
	fmthln(usr,	"c",	VIS_CONS,	"conf",		"prints Current hub configuration");
	fmthln(usr,	"c",	VIS_CONS,	"stat",		"prints Statistics");
	fmthln(usr,	NULL,	VIS_ALL,	"rules",	"prints rules which must be respected on the hub");
	fmthln(usr,	NULL,	VIS_ALL,	"uptime",	"prints hub uptime");
	fmthln(usr,	NULL,	VIS_ALL,	"date",		"prints hub date and time of server");
	fmthln(usr,	NULL,	VIS_ALL,	"time",		"same as command date");
	fmthln(usr,	"c",	VIS_CONS,	"users",	"prints All hub users");
	fmthln(usr,	"c",	VIS_CONS,	"showops",	"prints All hub operators");
	fmthln(usr,	NULL,	VIS_ALL,	"version",	"prints Version of the hub");
	fmthln(usr,	"c",	VIS_CONS,	"who",		"prints Who is on the console");
	fmthln(usr,	"sac", 	VIS_ALL,	"whois",	"prints info about user");
	fmthln(usr,	"sac", 	VIS_ALL,	"whoip",	"prints list of nicks having specified ip");
	fmthln(usr,	NULL, 	VIS_ALL,	"myip",		"prints your ip in the hub");

	fmthln(usr,	NULL, 	VIS_ALL,	"-----------------------------------------", NULL);
	fmthln(usr,	"as", 	VIS_ALL, 	NULL,NULL);

	fmthln(usr,	"as", 	VIS_ALL,	"[punishments]:", NULL);
	fmthln(usr,	"as", 	VIS_ALL,	"-----------------------------------------",NULL);
	fmthln(usr,	"as", 	VIS_ALL,	"kick",			"kicks the user");
	fmthln(usr,	"as", 	VIS_ALL,	"punish",		"punishes the user");
	fmthln(usr,	"as", 	VIS_ALL,	"lspunished",	"prints the list of punished users");
	fmthln(usr,	"as", 	VIS_ALL,	"amnesty",		"removes penalty from user");
	fmthln(usr,	"as", 	VIS_ALL,	"amnesty_all",	"amnesty of all punished");
	fmthln(usr,	"as", 	VIS_ALL,	"-----------------------------------------",NULL);
	fmthln(usr,	"as", 	VIS_ALL, 	NULL,NULL);

	fmthln(usr,	NULL,	VIS_ALL,	"[communication & redir]:" ,NULL);
	fmthln(usr,	NULL,	VIS_ALL,	"-----------------------------------------" ,NULL);
	fmthln(usr,	"s",	VIS_ALL,	"msg"			,"sends message to user in public as hub");
	fmthln(usr,	"s",	VIS_ALL,	"msg_all"		,"sends message to all users in public as hub");
	fmthln(usr,	"s",	VIS_ALL,	"msg_prv"		,"sends private message to user in private chat as hub");
	fmthln(usr,	"s",	VIS_ALL,	"msg_prv_all"	,"sends private message to all users in private as hub");
	fmthln(usr,	NULL,	VIS_ALL,	"mmsg"			,"sends message to user in public as your message");
	fmthln(usr,	NULL,	VIS_ALL,	"mmsg_all"		,"sends message to all users in public as your message");
	fmthln(usr,	"as",	VIS_ALL,	"redirect"		,"moves user to another hub");
	fmthln(usr,	"s",	VIS_ALL,	"redirect_all"	,"moves all users to another hub");
	fmthln(usr,	NULL,	VIS_ALL, "-----------------------------------------" ,NULL);
	fmthln(usr,	"c",	VIS_ALL, NULL ,NULL);

	fmthln(usr,"c", VIS_CONS,	"[other commands]:" ,NULL);
	fmthln(usr,"c", VIS_CONS,	"-----------------------------------------" ,NULL);
	fmthln(usr,"c", VIS_CONS,	"rehash"	,"reloads hub configuration");
	fmthln(usr,"c", VIS_CONS,	"quit"		,"leave the console");
	fmthln(usr,"c", VIS_CONS,	"-----------------------------------------" ,NULL);
	fmthln(usr,NULL, VIS_ALL, "========================================", NULL);
	return;

} /* }}} */

/* hlp_redirect - shows help for redirect command {{{*/
void hlp_redirect(userrec_t *usr)
{
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tREDIRECT");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tMoves the user to another hub.");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%credirect <nick> <where>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"\t\t<nick> is the nick of user");
	pubmsg(usr,"\t\t<where> is the adress of hub into <user> should be moved");
	pubmsg(usr,"========================================");
} /* }}} */

/* cmdredirect - redirect's the user to another hub {{{ */
void chat_redirect(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *who=NULL, 
		 *where=NULL,
		 *__strtok_temp__=NULL;
	int n;

	if (!usr->cons) disttcp(usr,line);

	//checking if user has apriopriate permissions
	if (usr->perm && !strchr(usr->perm,'s') && !strchr(usr->perm,'a')) return;

	if (!par || !*par) { hlp_redirect( usr ); return; }

	who=strtok_r(par," ",&__strtok_temp__);
	where=__strtok_temp__;//where

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tREDIRECT");
	pubmsg(usr,"========================================");

	// checks the nick, returns id
	if((n=chat_nick2id(usr,who))<0) return;

	if (!where || (where && !where[0]))
	{
		pubmsg(usr,"\tDo not know where to redirect");
		pubmsg(usr,"========================================");
		return;
	}

	if (!strcmp(usr->nick,who))
	{
		pubmsg(usr,"\tYou are trying to move yourself");
		pubmsg(usr,"========================================");
		return;
	}

	if (opbreak(usr,user[n],"redirect"))return;

	log_write(FAC_USER,PR_INFO,"'%s'(OP) moved %s to '%s'",usr->nick,user[n]->nick,where);

	disttcpf(user[n],"$ForceMove %s|",where);
	
	user[n]->reason=strdup("successfully redirected");
	user_set_state(user[n],STATE_QUIT);

	pubmsg(usr,"\tRedirecting '%s' to %s",who,where);
	pubmsg(usr,"========================================");
} /* }}} */

/* hlp_redirect - shows help for redirect command {{{*/
void hlp_redirect_all(userrec_t *usr)
{
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tREDIRECT_ALL");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tMoves all the users to another hub.");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%credirect_all <where>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"\t<where> is the adress of hub into users should be moved.");
	pubmsg(usr,"========================================");
} /* }}} */

/* cmdredirectall - redirect all users to anther hub {{{*/
void chat_redirect_all(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *where=NULL;

	int temp=0,
		n;

	if (!usr->cons) disttcp(usr,line);

	//checking if user has apriopriate permissions
	if (usr->perm && !strchr(usr->perm,'s')) return;

	if (!par || !*par) { hlp_redirect_all(usr); return; }
	where=par;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tREDIRECT_ALL");
	pubmsg(usr,"========================================");

	if (!where || (where && !where[0]))
	{
		pubmsg(usr,"\tdo not know where to redirect to");
		pubmsg(usr,"========================================");
		return;
	}

	log_write(FAC_USER,PR_INFO,"'%s'(sOP) moved ALL to '%s'",usr->nick,where);

	pubmsg(usr,"\tMoving users to %s",where);

	temp=0;
	for(n=0;n<NUSERS;n++)
		if (user[n] && user_tst_state(user[n],STATE_LOGGEDIN|STATE_SR|STATE_REGISTERED) && usr->perm && !strchr(user[n]->perm,'s'))
		{
			temp++;
			disttcpf(user[n],"$ForceMove %s|",where);
			user[n]->reason=strdup("successfully redirected");
			user_set_state(user[n],STATE_QUIT);
		}

	pubmsg(usr,"\tCommunicate sent to %d of %d users",temp,NUSERS);
	pubmsg(usr,"========================================");
	log_write(FAC_USER,PR_INFO,"'%s'(OP) moved ALL to '%s'",usr->nick,where);
} /* }}} */

/* cmduptime - displays hub uptime {{{ */
void chat_uptime(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line,
		 str[32];

	if (!usr->cons) disttcp(usr,line);

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tUPTIME");
	pubmsg(usr,"========================================");
	uptime(starttime,str);
	pubmsg(usr,"\t%s",str);
	pubmsg(usr,"========================================");
} /* }}} */

/* cmdrules - displays dcd.rules {{{ */
void chat_rules(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	if (!usr->cons) disttcp(usr,line);

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tRULES");
	pubmsg(usr,"========================================");
	if (conf.msg_rules) pubmsg(usr,"\n%s",conf.msg_rules);
	else pubmsg(usr,"\tNo rules specified");
	pubmsg(usr,"========================================");
} /* }}} */


/* cmdversion - displays hub version {{{ */
void chat_version(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	if (!usr->cons) disttcp(usr,line);

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tVERSION");
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tProject page: http://www.dc.ds.pg.gda.pl");	
	pubmsg(usr,"\t%s\t%s (build: %s)",PACKAGE_NAME,PACKAGE_VERSION,BUILD_TIME);
	pubmsg(usr,"========================================");
} /* }}} */

	/* cmdlspunished - displays the list of punished {{{*/
void chat_lspunished(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	int n;
	long time_, 
		 days, 
		 hours, 
		 min, 
		 sec;

	if (!usr->cons) disttcp(usr,line);

	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(usr->perm,'s') && !strchr(usr->perm,'a')) return;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tLSPUNISHED");
	pubmsg(usr,"========================================");

	if (NPENAL==0) pubmsg(usr,"\tNobody is punished :(");
	else pubmsg(usr,"\tIP : Nick: Punisher : Penalty : Time(d,hh:mm:ss) : Reason");

	for(n=0;n<NPENAL;n++)
	{

		time_=user_exp_penalty(penalties[n]);
		if (time_<=0) continue;

		days=time_/(long)(24*60*60);
		time_=time_%(24*60*60);

		hours=time_/(long)(60*60);
		time_=time_%(60*60);

		min=time_/(long)60;
		time_=time_%60;

		sec=time_;

		pubmsg(usr,"\t%s : %s : %s : %s%s%s%s%s : %ld, %ld:%ld:%ld : %s", penalties[n]->ip,
																penalties[n]->nick,
																strchr(usr->perm,'s')?penalties[n]->op:"HUB",
																(penalties[n]->penalty&PENALTY_BANNED)?"[ban] ":"",
																(penalties[n]->penalty&PENALTY_NODL)?"[nodl] ":"",
																(penalties[n]->penalty&PENALTY_NOSEARCH)?"[nosearch] ":"",
																(penalties[n]->penalty&PENALTY_NOPBLCHAT)?"[nopblchat] ":"",
																(penalties[n]->penalty&PENALTY_NOPRVCHAT)?"[noprvchat] ":"",
																days,
																hours,
																min,
																sec,
																penalties[n]->reason);
	}
	pubmsg(usr,"========================================");
} /* }}} */

/* hlp_punish - displays help for punish command {{{*/
void hlp_punish(userrec_t *usr)
{
	pubmsg(usr,"\tPunishment not specified");
	pubmsg(usr,"\tAvailable punishments sample use:");
	pubmsg(usr," ");
	pubmsg(usr,"\t\t%cpunish <nick/ip> <how> [time] [reason]",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"\t<how>");
	pubmsg(usr,"\t\tnosearch		- user has no permission to use search");
	pubmsg(usr,"\t\tnopblchat		- user has no permission to chat in main chat");
	pubmsg(usr,"\t\tnoprvchat		- user has no permission to chat in private chat");
	pubmsg(usr,"\t\tnochat			- user has no permission to	chan in main and private chat");
	pubmsg(usr,"\t\tnodl			- user has no permission to download");
	pubmsg(usr,"\t\tban				- user is banned");
	pubmsg(usr," ");
	pubmsg(usr,"\tYou can punish with multiple punishments, merging them with '+'");
	pubmsg(usr,"\ti.e. ");
	pubmsg(usr,"\t\t%cpunish UserNick nochat+nodl+nosearch 20",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"\t[time]			- the duration of punishment in minutes");
	pubmsg(usr," ");
	pubmsg(usr,"\t[time] and [reason] are optional");
	pubmsg(usr,"\tif no time is specified the duration of penalty will be %ld minute(s)",STD_PENALTY_DURATION);
	pubmsg(usr," ");
	pubmsg(usr,"\t1 day - 1440 minutes");
	pubmsg(usr,"\t3 days - 4320 minutes");
	pubmsg(usr,"\t1 week - 10080 minutes");
	pubmsg(usr,"\teternity (1 year) - -1 minutes :)");
	pubmsg(usr," ");
	pubmsg(usr,"\texamples:");
	pubmsg(usr,"\t\t%cpunish UserNick nochat 10 Was naughty.",(usr->cons)?' ':'#');
	pubmsg(usr,"\t\t%cpunish UserNick nochat+nodl",(usr->cons)?' ':'#');
	pubmsg(usr,"\t\t%cpunish UserNick nosearch Was naughty.",(usr->cons)?' ':'#');
	pubmsg(usr,"\t\t%cpunish UserIP nochat 10 Was naughty.",(usr->cons)?' ':'#');
	pubmsg(usr,"========================================");
} /* }}} */

/* cmdpunish - punishes the user with one of the avaliable penalties {{{ */
void chat_punish(chat_param_t *param)
{

	const char *punish[]=
	{
		"nopblchat",
		"ban",
		"nosearch",
		"noprvchat",
		"nodl",
		"nochat",
		"" /* indicates end of list */
	};
	userrec_t *usr=param->usr;

	char *par=param->param,
		 *line=param->line,
		 *who_tmp=NULL,
		 *how_tmp=NULL,
		 *msg_tmp=NULL,
		 *tmp=NULL,
		 *tmp1=NULL,
		 *pun=NULL, 
		 *__strtok_temp__;

	long n=0,
		 countpunish=0, 
		 duration=0;

	int punishment=0,
 		a; /* for checking ip */

	if (!usr->cons) disttcp(usr,line);

	/*checking if user has apriopriate permissions*/
	if ( usr->perm && !strchr(usr->perm,'s') && !strchr(usr->perm,'a'))return;

	if (!par || !*par) { hlp_punish(usr); return;	}

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tPUNISH");
	pubmsg(usr,"========================================");

	who_tmp=strtok_r(par," ",&__strtok_temp__); //who
	how_tmp=strtok_r(NULL," ",&__strtok_temp__); //how
	tmp=__strtok_temp__; //time + message

	if (tmp)
	{
		duration=strtol(tmp,&tmp1,10);

		if (duration==0) duration=conf.std_penalty_duration;
		if (*tmp1) msg_tmp=tmp1;
	}
	else duration=conf.std_penalty_duration;

	if (!who_tmp || (who_tmp && !who_tmp[0]) || !how_tmp || (how_tmp && !how_tmp[0]))
	{
		hlp_punish(usr) ;
		return;
	}

	if (duration==-1) duration=365*24*60; //-1 is one year

	duration=duration*60;// to get duration in seconds

	pun=strtok_r(how_tmp,"+",&__strtok_temp__);

	while(pun && pun[0])
	{
		/* parse punishment {{{ */
		countpunish=-1;
		while(*punish[++countpunish]) // was ...nish++], and didn't work
			if (pun && !strcasecmp(pun,punish[countpunish])) break;
		how_tmp=pun;								/* tmp2 contains current punishment(s) */
		pun=strtok_r(NULL,"+",&__strtok_temp__);

		switch(countpunish)
		{
			case 0: //nopblchat
					punishment|=(int)PENALTY_NOPBLCHAT;
					continue;

			case 1: //ban
					punishment|=(int)PENALTY_BANNED;
					continue;

			case 2: //nosearch
					punishment|=(int)PENALTY_NOSEARCH;
					continue;

			case 3: //noprvchat
					punishment|=(int)PENALTY_NOPRVCHAT;
					continue;

			case 4: //nodl
					punishment|=(int)PENALTY_NODL;
					continue;

			case 5: //nochat
					punishment|=(int)PENALTY_NOPRVCHAT|PENALTY_NOPBLCHAT;
					continue;

			default:
					pubmsg(usr,"Unknown punishment '%s'. Type '#punish' for more help",how_tmp);
					break;
		}
/* end of parse punishment }}}*/
	}

	if (punishment)
	{
		n=chat_nick2id(usr,who_tmp);
		if(n<0)
		{
			if (sscanf(who_tmp,"%d.%d.%d.%d",&a,&a,&a,&a)!=4) return;
			user_set_penalty(who_tmp,"(offline)",usr->nick, punishment,(unsigned long)time(NULL),duration,msg_tmp,0);	
		}
		else
		{
			if (user[n]->perm && strchr(user[n]->perm,'s')) return;
			user_set_penalty(user[n]->ip,user[n]->nick,usr->nick, punishment,(unsigned long)time(NULL),duration,msg_tmp,0);
		}
	}

	pubmsg(usr,"========================================");

} /* }}} */

/* hlp_msg - shows help for msg command {{{*/
void hlp_msg(userrec_t *usr)
{
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tShows the <message> to the <user> as 'Hub'");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cmsg <nick> <message>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_msg - sends message as hub to the user {{{*/
void chat_msg(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *who=NULL,
		 *msg=NULL,
		 *__strtok_temp__=NULL;
	int n;


	if (!usr->cons) disttcp(usr,line);

	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(usr->perm,'s')) return;
	
	if (!par || !*par) { hlp_msg(usr); return; }

	who=strtok_r(par," ",&__strtok_temp__);
	msg=__strtok_temp__;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG");
	pubmsg(usr,"========================================");

	n=chat_nick2id(usr,who); 
	if(n<0) return;

	if (!msg || (msg && !msg[0]))
	{
		pubmsg(usr,"\tYou have to specify message");
		pubmsg(usr,"========================================");
		return;
	}
	if (user[n]!=usr) pubmsg(user[n],"%s",msg);
}/* }}} */

/* hlp_msg_prv - shows help for msg_prv command {{{*/
void hlp_msg_prv( userrec_t *usr)
{

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG_PRV");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tSends private <message> to the <user> as 'Hub'");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cmsg <nick> <message>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_msg_prv - sends message as hub to the user {{{*/
void chat_msg_prv(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *who=NULL,
		 *msg=NULL,
		 *__strtok_temp__=NULL;

	int n;


	if (!usr->cons) disttcp(usr,line);

	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(usr->perm,'s')) return;

	if (!par || !*par) { hlp_msg_prv(usr); return;}

	who=strtok_r(par," ",&__strtok_temp__);
	msg=__strtok_temp__;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG_PRV");
	pubmsg(usr,"========================================");

	n=chat_nick2id(usr,who); 
	if(n<0)return;

	if (!msg || (msg && !msg[0]))
	{
		pubmsg(usr,"\tYou have to specify message");
		pubmsg(usr,"========================================");
		return;
	}

	if (user[n]!=usr) privmsg(user[n],NULL,"%s",msg);
}/* }}} */

/* hlp_msg_prv_all - shows help for msg_prv_all command {{{*/
void hlp_msg_prv_all( userrec_t *usr)
{
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG_PRV_ALL");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tSends private <message> to all the users as 'Hub'");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cmsg_prv_all <message>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */

/* msg_prv_all - sends private message as hub to the user {{{*/
void chat_msg_prv_all(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *msg=NULL;

	if (!usr->cons) disttcp(usr,line);	
	
	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(param->usr->perm,'s')) return;

	if (!par || !*par) { hlp_msg_prv_all(usr); return;}

	msg=par;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG_PRV_ALL");
	pubmsg(usr,"========================================");

	if (!msg || (msg && !msg[0]))
	{
		pubmsg(usr,"\tYou have to specify message");
		pubmsg(usr,"========================================");
		return;
	}

	privmsg(NULL,NULL,"%s",msg);
}/* }}} */



/* hlp_mmsg - shows help for msg command {{{*/
void hlp_mmsg( userrec_t *usr)
{
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMMSG");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description:");
	pubmsg(usr,"\tShows the <message> to the <user> as Your message");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cmmsg <nick> <message>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */

/* mmsg - sends message as hub to the user {{{*/
void chat_mmsg(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *who=NULL,
		 *msg=NULL,
		 *__strtok_temp__=NULL;
	int n;

	if (!usr->cons) disttcpf(usr,"%s|",line);	

	//checking if user has apriopriate permissions
	if (!par || !*par) { hlp_mmsg(usr); return;}

	who=strtok_r(par," ",&__strtok_temp__);
	msg=__strtok_temp__; 

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMMSG");
	pubmsg(usr,"========================================");

	n=chat_nick2id(usr,who); 
	if(n<0)return;

	if (!msg || (msg && !msg[0]))
	{
		pubmsg(usr,"\tYou have to specify message");
		pubmsg(usr,"========================================");
		return;
	}
	if (user[n]!=usr) disttcpf(user[n],"<%s> %s|",usr->nick,"%s",msg);
}/* }}} */

/* hlp_msg_all - shows help for msg command {{{*/
void hlp_msg_all( userrec_t *usr)
{

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMSG_ALL");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tShows the <message> to all users as 'Hub'");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cmsg_all <message>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */

/* msgall - sends message as hub to all users {{{*/
void chat_msg_all(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line,
		 *par=param->param;

	if (!usr->cons) disttcp(usr,line);	

	//checking if user has apriopriate permissions
	if (usr->perm && !strchr(usr->perm,'s')) return;

	if (!par || !*par) { hlp_msg_all(usr); return; }

	pubmsg(NULL,"%s",par);
	log_write(FAC_CHAT,PR_INFO,"<HUB>(%s) -> ALL: %s", usr->nick,par);

} /* }}} */

/* hlp_msg_all - shows help for msg command {{{*/
void hlp_mmsg_all( userrec_t *usr)
{
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMMSG_ALL");
	pubmsg(usr,"========================================");
	pubmsg(usr," ");
	pubmsg(usr,"Description:");
	pubmsg(usr,"\tShows the <message> to all users as your message");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cmmsg_all <message>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */

/* mmsg_all - sends message as hub to all users {{{*/
void chat_mmsg_all(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line;

	if (!usr->cons) disttcp(usr,line);	
	//checking if user has apriopriate permissions
	if (!par|| !*par) { hlp_mmsg_all(usr); return;	}

	disttcpf(NULL,"<%s> %s|",usr->nick,par);

} /* }}} */

/* hlp_amnesty() {{{ */
void hlp_amnesty( userrec_t *usr)
{
	pubmsg(usr," ");
	pubmsg(usr,"Description:");
	pubmsg(usr,"\tRemoving punishment from user");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%c amnesty <user/ip>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */


/* chat_amnesty - cancels all of current user's penalties {{{ */
void chat_amnesty(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *who=NULL;

	if (!usr->cons) disttcp(usr,line);	

	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(usr->perm,'s') && !strchr(usr->perm,'a')) return;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tAMNESTY");
	pubmsg(usr,"========================================");

	if (!par|| !*par) { hlp_amnesty(usr); return;	};
	who=par;

	if (!who || (who && !who[0]))
	{
		pubmsg(usr,"\tUser's nick or ip needed");
		pubmsg(usr,"========================================");
		return;
	}

	log_write(FAC_USER,PR_INFO,"Amnesty for '%s' by %sOP '%s'",who,strchr(usr->perm,'s')?"s":"",usr->nick);

	user_set_penalty(who,who,"",PENALTY_NONE,0,0,NULL,0);

	pubmsg(usr,"\tAmnesty of USER/IP: %s",who);
	pubmsg(usr,"========================================");

} /* }}} */

 /* amnesty_all - cancels all of penalties {{{ */
void chat_amnesty_all(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;
	int i,
		max=NPENAL;
		
	if (!usr->cons) disttcp(usr,line);	

	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(usr->perm,'s')) return;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tAMNESTY_ALL");
	pubmsg(usr,"========================================");

	log_write(FAC_USER,PR_INFO,"Amnesty for ALL by %sOP '%s'",usr->nick,strchr(usr->perm,'s')?"s":"");

	for(i=0; i<max;i++) delpenalty(0);
	penalties_write();

	pubmsg(usr,"\tAmnesty of all punished");
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_conf {{{ */
void chat_conf (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	if (!usr->cons) disttcp(usr,line);	

	//can be used only from console
	if (!usr->cons) return;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tCONF");
	pubmsg(usr,"========================================");
	pubmsg(usr,"Config file: %s", conf.conf_main);
	pubmsg(usr,"Welcome file: %s", conf.conf_welcome);
	pubmsg(usr,"MOTD file: %s", conf.conf_motd);
	pubmsg(usr,"Rules file: %s", conf.conf_rules);
	pubmsg(usr,"Bans file: %s", conf.conf_banned);
	pubmsg(usr,"Console users file: %s", conf.conf_cusers);
	pubmsg(usr,"Console allow file: %s", conf.conf_callow);
	pubmsg(usr,"Nicks allow file: %s", conf.conf_nallow);
    pubmsg(usr,"Log Facility: %u", conf.log_facility);
	pubmsg(usr,"========================================");
	pubmsg(usr,"Main listening port[TCP]: %u", conf.listen_port_main);

	if (conf.listen_port_cons) pubmsg(usr,"Console listening port[TCP]: %u", conf.listen_port_cons);
	if (conf.minslots) pubmsg(usr,"Minslots checking port[UDP]: %u", conf.listen_port_udp);
	pubmsg(usr,"Max_receive_once: %u", conf.max_receive_once);

	pubmsg(usr,"Main Listening threads: %u", conf.n_listen_main);
	pubmsg(usr,"User managing threads: %u", conf.n_user_manager);
	pubmsg(usr,"========================================");
	pubmsg(usr,"UID: %s(%u)", conf.user, conf.uid);
	pubmsg(usr,"GID: %s(%u)", conf.group, conf.gid);
	pubmsg(usr,"========================================");
	pubmsg(usr,"Hub Name: %s", conf.hubname);
	pubmsg(usr,"Admin contact: %s", conf.admin_contact);
	pubmsg(usr,"User limit: %u", conf.userlimit);
	pubmsg(usr,"Allow broken keys: %d", conf.allow_broken_key);
	pubmsg(usr,"Idle timeout: %u", conf.idle_timeout);
	pubmsg(usr,"Registration timeout: %u", conf.register_timeout);
	pubmsg(usr,"penalties update interval (s): %u", conf.penalties_update_interval);
	pubmsg(usr,"Search interval (s): %u", conf.search_interval);
	pubmsg(usr,"Minimum data shared: %"PRIu64" MB", conf.minshare);
	pubmsg(usr,"Minimum slots: %u", conf.minslots);
	pubmsg(usr,"Max commands: %u", conf.max_commands);	
	pubmsg(usr,"Allow chat: %d", conf.allow_chat);
	pubmsg(usr,"Flags: %u", conf.flags);
	pubmsg(usr,"========================================");

    

	if (conf.redirect_switch)
	{
		pubmsg(usr,"Redirections:");
		if(conf.redirect_switch&REDIRECT_SWITCH_HUB_IS_FULL) pubmsg(usr,"Hub is full: %s",conf.redirect_hub_is_full);
		if(conf.redirect_switch&REDIRECT_SWITCH_MINSHARE)	pubmsg(usr,"Min_share: %s",conf.redirect_minshare);
		if(conf.redirect_switch&REDIRECT_SWITCH_MINSLOTS)	pubmsg(usr,"Min_slots: %s",conf.redirect_minslots);
		pubmsg(usr,"========================================");
	}

} /* }}} */

/* chat_rehash {{{ */
void chat_rehash(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	if (!usr->cons) 

	//can be used only from console
	if (!usr->cons)
	{
		disttcp(usr,line);	
		return;
	}
	
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tREHASH");
	pubmsg(usr,"========================================");
	
	rehash=1;

} /* }}} */

/* chat_stat {{{ */
void chat_stat (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line,
		 str[32];

	uint64_t tmp=0;
	int i, 
		reg=0;


	if (!usr->cons) 
	{
		disttcp(usr,line);	
		return;
	}

	for(i=0;i<NUSERS;i++)
		if (user_tst_state(user[i],STATE_REGISTERED))
		{
			tmp+=user[i]->myinfo.share;
			reg++;
		}

	tmp=tmp/(int)(1024*1024);

	i=0; // MB
	if (tmp>=1024)
	{
		tmp=tmp/(int)1024;
		i=1; // GB

		if (tmp>=1024)
		{
			tmp=tmp/(int)1024;
			i=2; // TB
		}
	}

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tSTAT");
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tUsers registered: %d",reg);
	pubmsg(usr,"\tUsers waiting:	%d",NUSERS-reg);
	pubmsg(usr,"\tShared data: %"PRIu64"%cB",tmp,(i==0)?'M':((i==1)?'G':'T'));
	uptime(starttime,str);
	pubmsg(usr,"\tUptime: %s",str);
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_stats {{{ */
void chat_stats(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char buf[4096],
		 cmds[4096],
		*line=param->param,
		 *p,
		 *nl;

	if (!usr->cons)
	{
		disttcp(usr,param->line);
		return;
	}

	if (line && !strcmp(line,"reset"))
	{
		stats_reset();
		cmd_stats_reset();
		pubmsg(usr,"========================================");
		pubmsg(usr,"\tStats counters reset");
		pubmsg(usr,"========================================");
		return;
	}

	stats_format(buf,sizeof(buf));
	cmd_stats_format(cmds,sizeof(cmds));

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tSTATS");
	pubmsg(usr,"========================================");

	for (p=buf; *p; p=nl+1)
	{
		nl=strchr(p,'\n');
		if (nl) *nl=0;
		pubmsg(usr,"\t%s",p);
		if (!nl) break;
	}

	pubmsg(usr,"----------------------------------------");

	for (p=cmds; *p; p=nl+1)
	{
		nl=strchr(p,'\n');
		if (nl) *nl=0;
		pubmsg(usr,"\t%s",p);
		if (!nl) break;
	}

	pubmsg(usr,"========================================");
} /* }}} */

/* hlp_() {{{ */
void hlp_whois( userrec_t *usr)
{
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tShows info of specified user");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cwhois <nick>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */


/* chat_whois {{{ */
void chat_whois(chat_param_t *param)
{
	userrec_t *usr=param->usr;

	char *par=param->param,
		 *line=param->line,
		 str[32],
		*who=NULL;

	int i, 
		count;

	uint64_t tmp;

	if (!usr->cons) disttcp(usr,line);	

	//checking if user has apriopriate permissions
	if (usr->perm && !strchr(usr->perm,'c') &&
		!strchr(usr->perm,'s') &&
		!strchr(usr->perm,'a')) return;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tWHOIS");
	pubmsg(usr,"========================================");

	if (!par|| !*par) { hlp_whois(usr); return;	};	


	who=par;
	i=chat_nick2id(usr,who);
	if (i<0) return;
	
	pthread_mutex_lock(&mutex_myinfo);

	pubmsg(usr,"Nick: %s",user[i]->nick);
	pubmsg(usr,"IP: %s",user[i]->ip);
	uptime(user[i]->idle,str);
	pubmsg(usr,"Idle: %s",str);
	pubmsg(usr,"Flag: %s",flag_myinfo(&user[i]->myinfo));

	if (user[i]->cons==0)
	{
		tmp=user[i]->myinfo.share;
		tmp/=1024*1024; count=0; // MB
		if (tmp>=1024)
		{
			tmp/=1024; count=1; // GB
			if (tmp>=1024) { tmp/=1024; count=2; } // TB
		}
		pubmsg(usr,"Shared: %"PRIu64"%cB",tmp,(count==0)?'M':((count==1)?'G':'T'));
	}

	pubmsg(usr,"E-mail: %s",user[i]->myinfo.email);
	pubmsg(usr,"Connection: %s",user[i]->myinfo.ctype);
	pubmsg(usr,"Description: %s",user[i]->myinfo.desc);
	pubmsg(usr,"Permissions: %s", user[i]->perm?user[i]->perm:"not set");

	if (user[i]->penalty) pubmsg(usr,"Punishments: %d",user[i]->penalty->penalty);
	pubmsg(usr,"========================================");

	pthread_mutex_unlock(&mutex_myinfo);
} /* }}} */

/* hlp_whoip() {{{ */
void hlp_whoip( userrec_t *usr)
{
	pubmsg(usr," ");
	pubmsg(usr,"Description: ");
	pubmsg(usr,"\tShows every nick which has specified ip");
	pubmsg(usr," ");
	pubmsg(usr,"Syntax:");
	pubmsg(usr,"\t%cwhoip <ip>",(usr->cons)?' ':'#');
	pubmsg(usr," ");
	pubmsg(usr,"========================================");
} /* }}} */


/* chat_whoip {{{ */
void chat_whoip (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *ip;
		 
	int i,
		count=0;

	if (!usr->cons) disttcp(usr,line);	

	//checking if user has apriopriate permissions
	if (usr->perm && !strchr(usr->perm,'c') &&
		!strchr(usr->perm,'s') &&
		!strchr(usr->perm,'a')) return;

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tWHOIP");
	pubmsg(usr,"========================================");

	if (!par|| !*par) { hlp_whoip(usr); return;	};	


	ip=par;

	if (!ip || (ip && !ip[0]))
	{
		pubmsg(usr,"\tYou have to specify user's ip");
		pubmsg(usr,"========================================");
		return;
	}

	for(i=0;i<NUSERS;i++)
		if (user[i] &&
			user_tst_state(user[i],STATE_REGISTERED) &&
			!strcmp(user[i]->ip,ip))
			{
				pubmsg(usr,"%s",user[i]->nick);
				count++;
			}

	if (!count) pubmsg(usr,"\tNobody of this ip was found");
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_quit {{{ */
void chat_quit (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	//can be used only from console
	if (!usr->cons) 
	{
		disttcp(usr,line);	
		return;
	}

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tQUIT");
	pubmsg(usr,"========================================");
	user_set_state(usr,STATE_QUIT);
} /* }}} */

/* chat_who {{{ */
void chat_who (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;

	int i;
	char str[32];

	if (!usr->cons) 
	{
		disttcp(usr,line);	
		return;
	}

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tWHO");
	pubmsg(usr,"========================================");

	pubmsg(usr,"USER				 FROM			 IDLE");
	for (i=0;i<NUSERS;i++)
		if (user[i] && user_tst_state(user[i],STATE_REGISTERED) &&(user[i]->cons))
		{
			uptime(user[i]->idle,str);
			disttcpf(usr,"%-20s %-15s %s\r\n",user[i]->nick,user[i]->ip,str);
		}
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_users {{{ */
void chat_users (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;
	long i;

	if (!usr->cons) 
	{
		disttcp(usr,line);	
		return;
	}

	pubmsg(usr,"========================================");
	pubmsg(usr,"\tUSERS");
	pubmsg(usr,"========================================");

	for (i=0;i<NUSERS;i++)
	{
		if (user[i] && user_tst_state(user[i],STATE_REGISTERED))
			pubmsg(usr,"%s",user[i]->nick);		
	}
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_kick {{{ */
void chat_kick (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *par=param->param,
		 *line=param->line,
		 *who=NULL,
		 *msg=NULL,
		 *__strtok_temp__=NULL;

	int i;

	if (!usr->cons) disttcp(usr,line);	

	//checking if user has apriopriate permissions
	if ( usr->perm && !strchr(usr->perm,'s') && !strchr(usr->perm,'a')) return;

	if (!par||!*par) return;

	who=strtok_r(par," ",&__strtok_temp__);
	msg=__strtok_temp__;
	
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tKICK:");
	pubmsg(usr,"========================================");

	i=chat_nick2id(usr,who);

	if(i<0)return;

	if (user[i]==usr)
	{
		pubmsg(usr,"\tIt's simplier to use 'quit'.");
		pubmsg(usr,"========================================");
		return;
	}

    if (opbreak(usr,user[i],"kick"))return;
	
	pubmsg(user[i],"You have been kicked off by %s.",usr->nick);
	if (msg) pubmsg(user[i],"Reason: %s.",msg);

	log_write(FAC_USER,PR_INFO,"'%s'@%s Kicked by '%s'.",user[i]->nick,user[i]->ip,usr->nick);
	
	user[i]->reason=strdup("successfully kicked");
	user_set_state(user[i],STATE_QUIT);

} /* }}} */

/* chat_date {{{ */
void chat_date (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char 	*line=param->line,
			str[32];

	struct tm timeptr;
	time_t now;
		
	if (!usr->cons) disttcp(usr,line);	
	
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tDATE");
	pubmsg(usr,"========================================");
	now=time(NULL);
	strftime(str, 32, "%Y.%m.%d %T %Z", localtime_r(&now, &timeptr));
	pubmsg(usr,"%s",str);
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_myip {{{ */
void chat_myip (chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char 	*line=param->line;

	if (!usr->cons) disttcp(usr,line);
	
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tMyIP");
	pubmsg(usr,"========================================");
	pubmsg(usr,"%s",usr->ip);
	pubmsg(usr,"========================================");
} /* }}} */

/* chat_showops {{{ */
void chat_showops(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char 	*line=param->line;
	int i, found=0;
	
	if (!usr->cons) 
	{
		disttcp(usr,line);	
		return;
	}
	
	pubmsg(usr,"========================================");
	pubmsg(usr,"\tShowOps");
	pubmsg(usr,"========================================");

	for(i=0; i<NUSERS; i++) 
		if (user[i] && 
			user_tst_state(user[i],STATE_REGISTERED) &&
			(strchr(user[i]->perm,'s') || strchr(user[i]->perm,'a')))
			{
				pubmsg(usr,"%s",user[i]->nick);
				found++;
			}
	
	if (!found) pubmsg(usr,"There are no operators");

	pubmsg(usr,"========================================");
} /* }}} */


void chat_not_found(chat_param_t *param)
{
	userrec_t *usr=param->usr;
	char *line=param->line;
	char *cmd=param->cmd;

	if((user_tst_penalty(usr,PENALTY_NOPBLCHAT) && !strchr(usr->perm,'s')) || usr->cons) return;

	if (conf.max_chat_length>0 && 
		!strchr(usr->perm,'s') && 
		strlen(line)>conf.max_chat_length) 
	{
		if (conf.kick_max_chat_length>0) 
		{
			pubmsg(usr, "You are kicked because the message sent by you was longer than %d",conf.max_chat_length);
			usr->reason=strdup("too long chat message");
			user_set_state(usr,STATE_QUIT);
		}
		
		pubmsg(usr,"Message was not sent because it is longer than %d",conf.max_chat_length);

		return;
	}

	if(!conf.allow_chat)		
	{
		pubmsg(usr,"Public chat is not allowed.");
		return;
	}

    log_write(FAC_CHAT,PR_INFO,"%s",line);		

	if (cmd && cmd[0]=='#') 
	{
		disttcp(usr,line);
		return;
	}

	disttcp(NULL,line);
}

/*
void cmd_chat_exec(*usr,*line)

.description:
 runs the in main chat or console given command, depending on usr->cons

.args:
	userrec_t *usr - userrec of the user, who gives the command
	char *line - the line following the command

 {{{*/

static cmd_dict_t chat_dict;
static pthread_once_t chat_dict_once = PTHREAD_ONCE_INIT;

static void chat_cmd_dict_init(void)
{
	static const cmd_t chat_set[]=
	{
		{ "amnesty",		(funct_t)chat_amnesty				},
		{ "amnesty_all",	(funct_t)chat_amnesty_all 			},
		{ "conf",			(funct_t)chat_conf					},
		{ "date",			(funct_t)chat_date					},
		{ "help", 			(funct_t)chat_help					},
		{ "kick",			(funct_t)chat_kick					},
		{ "lspunished",		(funct_t)chat_lspunished			},
		{ "mmsg",			(funct_t)chat_mmsg					},
		{ "mmsg_all",		(funct_t)chat_mmsg_all				},
		{ "msg",			(funct_t)chat_msg					},
		{ "msg_all",		(funct_t)chat_msg_all				},
		{ "msg_prv",		(funct_t)chat_msg_prv				},
		{ "msg_prv_all",	(funct_t)chat_msg_prv_all			},
		{ "myip",			(funct_t)chat_myip					},
		{ "punish",			(funct_t)chat_punish				},
		{ "quit",			(funct_t)chat_quit					},
		{ "redirect", 		(funct_t)chat_redirect				},
		{ "redirect_all", 	(funct_t)chat_redirect_all			},
		{ "rehash",			(funct_t)chat_rehash				},
		{ "rules",			(funct_t)chat_rules					},
		{ "showops",		(funct_t)chat_showops				},
		{ "stat",			(funct_t)chat_stat					},
		{ "stats",			(funct_t)chat_stats				},
		{ "time",			(funct_t)chat_date					},
		{ "uptime", 		(funct_t)chat_uptime				},
		{ "users",			(funct_t)chat_users					},
		{ "version",		(funct_t)chat_version				},
		{ "who",			(funct_t)chat_who					},
		{ "whoip",			(funct_t)chat_whoip					},
		{ "whois",			(funct_t)chat_whois					},
		{ "",				(funct_t)chat_not_found				}
	};

	cmd_dict_build(&chat_dict, chat_set, 1);
	chat_dict.name="chat";
}

void chat_cmd_exec(userrec_t *usr, char *line, char *line_copy)
{
	char *delim_[]={" \n", " |"},
		 *delim__[]={"\n",	"|"};

	chat_param_t chat_param;

	char *cmd=NULL,
		 *par=NULL,
		 *temp=line,
		 *delim=delim_[1],
		 *delimp=delim__[1];

	if (pthread_once(&chat_dict_once, chat_cmd_dict_init)) return;

	if(usr->cons)
	{
		delim=delim_[0];
		delimp=delim__[0];
	}

	cmd=strsep(&temp,delim);
	if (!temp) return;

	par=strsep(&temp,delimp);

	chat_param.usr=usr;
	chat_param.param=par;
	chat_param.line=line_copy;
	chat_param.cmd=cmd;

	if ((cmd && cmd[0]=='#') || usr->cons) 
	{	
		if (!usr->cons && cmd && cmd[0]=='#') cmd++;
		cmd_dict_exec(&chat_dict, cmd, &chat_param);
	}
	else chat_not_found(&chat_param);
	
//	my_free(line_copy);
}
/* }}} */

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
