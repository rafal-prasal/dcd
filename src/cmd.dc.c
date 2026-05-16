/* $Id: cmd.dc.c,v 1.62 2007/02/19 20:59:18 rav Exp $ */
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

extern int NPENAL;
extern int NUSERS;
extern int NHUBS;

extern Tpenalty *penalties[MAXPENALTIES];	/* table of penalties */
extern userrec_t *user[MAXUSERS];			/* users table */
extern config_t conf;						/* configuration data */

extern int rehash;

extern pthread_mutex_t mutex_myinfo;

/* $Key <key> */
void dc_Key(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *key = param->param;	
	
	if(!user_tst_state(usr,STATE_CONNECTED)) return;
	
	if (!conf.allow_broken_key
		&& !validkey(usr,key))
	{
		pubmsg(usr,"Key: invalid reply");
		
		usr->reason=strdup("Invalid key reply");
		user_set_state(usr, STATE_QUIT);		
		return;
	}
	
	user_set_state(usr,STATE_KEY);
}

/* $Quit <nick> */
void dc_Quit(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *nick = param->param;	
	
	if (!nick || !*nick ||
		!user_tst_state(usr,STATE_LOGGEDIN|STATE_REGISTERED|STATE_PASSWORD)) return;
	
	if(strcasecmp(usr->nick,nick))
	{
		pubmsg(usr,"Quit: Fake nick is not allowed");
		usr->reason=strdup("Quit: Fake nick is not allowed");
        log_write(FAC_USER,PR_WARNING,"'%s'@%s sent Faked nick in 'Quit'",usr->nick,usr->ip);
	}

	user_set_state(usr,STATE_QUIT);	
}

/* $ValidateNick <nick> */
void dc_ValidateNick(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *nick = param->param;	
	
	if (!nick || !*nick ||
		!user_tst_state(usr,STATE_KEY)) return;

	switch(validnick(usr,nick))
	{
	case NICK_IS_TOO_LONG:
			pubmsg(usr,"ValidateNick: nick is longer than limit of %d characters",NICK_LEN);			
			usr->reason=strdup("ValidateNick: nick was too long");
			user_set_state(usr,STATE_QUIT);
	break;
	
	case NICK_REQUIRES_PASSWORD:
			my_duplicate(nick,&usr->nick);
			pubmsg(usr,"ValidateNick: nick '%s' requires password", usr->nick);

            log_write(FAC_ACCESS,PR_WARNING,"'%s'@%s: ValidateNick: nick requires password",usr->nick,usr->ip);
			
			if (penalty_welcome(usr)) return;

			user_set_state(usr,STATE_PASSWORD);
			if (!usr->userip[0]) snprintf(usr->userip, sizeof(usr->userip), "$UserIP %s %s|", usr->nick, usr->ip);
			disttcp(usr,"$GetPass|");
	break;

	case NICK_IS_NOT_IN_PATTERNS:
			pubmsg(usr,"ValidateNick: nick '%s' does not match any allowed pattern", nick);
			my_duplicate(nick,&usr->nick);
			
			usr->reason=strdup("ValidateNick: nick does not match any allowed pattern");						
			user_set_state(usr,STATE_QUIT);
	break;

	case NICK_IS_ALREADY_USED:
			pubmsg(usr,"ValidateNick: nick '%s' is already used",nick);

			usr->reason=strdup("ValidateNick: nick is already used");
			user_set_state(usr,STATE_QUIT);		
	break;

	case NICK_IS_RESERVED:
			pubmsg(usr,"ValidateNick: nick '%s' is constantly reserved",nick);
			usr->reason=strdup("ValidateNick: nick is constantly reserved");
			user_set_state(usr,STATE_QUIT);		
	break;

	case NICK_IS_OK:
			if(conf.registered_only) 
			{
				pubmsg(usr,"ValidateNick: registered users only");
				usr->reason=strdup("ValidateNick: registered users only");
				user_set_state(usr,STATE_QUIT);										
				return;
			}
	
			pubmsg(usr,"ValidateNick: nick is fine");
			my_duplicate(nick,&usr->nick);
			log_write(FAC_ACCESS,PR_INFO,"'%s'@%s: ValidateNick: Logs in as normal user",usr->nick,usr->ip);
			
			if (penalty_welcome(usr)) return;

			user_set_state(usr,STATE_LOGGEDIN);
			disttcpf(usr,"$Hello %s|",usr->nick);
			if (!usr->userip[0]) snprintf(usr->userip, sizeof(usr->userip), "$UserIP %s %s|", usr->nick, usr->ip);
	break;
	
	case NICK_IS_WITH_INVALID_CHARACTERS:
			pubmsg(usr,"ValidateNick: nick '%s' Contains invalid characters",nick);
			
			if (!conf.allow_non_us_ascii_nicks) 
				pubmsg(usr,"Nick must be isprint() and !isspace() in US ASCII 7-bit");

			pubmsg(usr,"- '#' is not allowed as the first character");
			pubmsg(usr,"- '?','$',':','<','>' are also not allowed");

			my_duplicate(nick,&usr->nick);

			usr->reason=strdup("ValidateNick: nick contains invalid characters");
			user_set_state(usr,STATE_QUIT);
	break;
	}	
}

/* $MyPass <pass> */
void dc_MyPass(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *passwd = param->param;
	
	int super=0, admin=0;
	
	if (!passwd || !*passwd ||
		!user_tst_state(usr,STATE_PASSWORD)) return;
		
	if (strcmp(passwd,usr->password))
	{
		pubmsg(usr,"MyPass: incorrect password for nick '%s'",usr->nick);
		disttcp(usr,"$BadPass|");		

		usr->reason=strdup("MyPass: incorrect password");
		user_set_state(usr,STATE_QUIT);
		return;
	}
	
	super=(strchr(usr->perm,'s')!=NULL);
	admin=(strchr(usr->perm,'a')!=NULL);

    log_write(FAC_ACCESS,PR_INFO,"'%s'@%s: MyPass: Logs in as %s",usr->nick,usr->ip,
		super?"sOP":
			admin?"OP":"registered user");

	if (super || admin) disttcpf(usr,"$LogedIn %s|",usr->nick);
	disttcpf(usr,"$Hello %s|",usr->nick);
	
	user_set_state(usr,STATE_LOGGEDIN);
	pubmsg(usr,"------------------------------------");
}

/* $GetNickList */
void dc_GetNickList(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char ops[((NICK_LEN+2)*MAXUSERS+2)+20],
	     nicks[((NICK_LEN+2)*MAXUSERS+2)+20];

	int i;
			
	if(!user_tst_state(usr,STATE_LOGGEDIN|STATE_REGISTERED)) return;

	ops[0]='\000';
	nicks[0]='\000';

	strcat(ops,"$OpList ");

	if(!user_tst_supports(usr, SUPPORTS_NoHello_NoGetINFO))
	{
		strcat(nicks,"$NickList ");

		for(i=0;i<NUSERS;i++)
			if (user[i] 
				&& user_tst_state(user[i],STATE_REGISTERED))
			{
				strcat(nicks,user[i]->nick);
				strcat(nicks,"$$");			
			}
	}

	for(i=0;i<NUSERS;i++)
		if (user[i] 
			&& user_tst_state(user[i],STATE_REGISTERED)
			&& user[i]->perm
			&& (strchr(user[i]->perm,'s') 
					|| strchr(user[i]->perm,'a')))
		{			
			strcat(ops,user[i]->nick);
			strcat(ops,"$$");
		}


	if (!user_tst_state(usr,STATE_REGISTERED))
	{
		if(!user_tst_supports(usr,SUPPORTS_NoHello_NoGetINFO));
		{ 
			strcat(nicks,usr->nick);
			strcat(nicks,"$$");
		}

		if (usr->perm && (strchr(usr->perm,'a')||strchr(usr->perm,'s')))
		{
			strcat(ops,usr->nick);
			strcat(ops,"$$");
		}
	}

	strcat(ops,"|");

	if(user_tst_supports(usr,SUPPORTS_NoHello_NoGetINFO))
	{
		for(i=0;i<NUSERS;i++)
			if (user[i] 
				&& user_tst_state(user[i],STATE_REGISTERED))
					dc_myinfo(usr,user[i]);
	}
	else
		{
			strcat(nicks,"|");
			disttcp(usr,nicks);
		}
	
	if(user_tst_supports(usr,SUPPORTS_UserIP2))
	{
		disttcp(usr,usr->userip); 

		for(i=0;i<NUSERS;i++)
			if (user[i] && user_tst_state(user[i],STATE_REGISTERED))
				disttcp(usr,user[i]->userip);
	}

	if (usr->perm && (strchr(usr->perm,'s') || strchr(usr->perm,'a')))
	{
		disttcp(usr,ops);
		disttcp(NULL,ops);
	}
	else disttcp(usr,ops);
}

/* $MyINFO $ALL <nick> <description>$ $<connection><flag>$<e-mail>$<sharesize>$ */
void dc_MyINFO(dc_param_t *param)
{
	int ret=0;

	userrec_t *usr = param->usr;

	char *info = param->param,
		 *line = param->line;	


#ifndef UINT64_C
	myinfo_t myinfo_tmp = { {0}, {0} , 0, {0}, 0, {0} };
#else
	myinfo_t myinfo_tmp = { {0}, {0}, 0, {0}, UINT64_C(0), {0} };
#endif

	if (!info 
		|| !*info 
		|| !user_tst_state(usr,STATE_REGISTERED|STATE_LOGGEDIN)) return;
	
	if ((ret=fill_myinfo(usr, &myinfo_tmp, info,line)))
	{
        log_write(FAC_USER,PR_ERROR,"MyINFO: invalid(%d) '%s'",ret,line);		
		usr->reason=strdup("MyINFO: invalid MyINFO");		
		user_set_state(usr,STATE_QUIT);
		return;
	}
		
	if (conf.minshare &&
		usr->perm && 
		!strchr(usr->perm,'s') && 
		!strchr(usr->perm,'m') && 
		myinfo_tmp.share<conf.minshare*1024*1024)
	{
		pubmsg(usr,"MyINFO: share at least %"PRIu64"MB",conf.minshare);
			
		if (conf.redirect_switch&REDIRECT_SWITCH_MINSHARE)
		{
			pubmsg(usr,"You are being redirected to %s",conf.redirect_minshare);
			disttcpf(usr,"$ForceMove %s|",conf.redirect_minshare);
		}

		usr->reason=strdup("MyINFO: not enough MB shared ");
		user_set_state(usr,STATE_QUIT);
		return;			
	}
		
	if (conf.minslots &&
		!strchr(usr->perm,'s') && 
		!strchr(usr->perm,'r'))
	{
		if (!usr->slots) pubmsg(usr,"MyINFO: Requesting Testing Slots");		

		user_set_state(usr,STATE_SR|usr->state);
		
		disttcpf(usr,"$Search %s:%d F?F?0?1?fakesearch|",usr->con_ip, conf.listen_port_udp);
		disttcpf(usr,"$Search %s:%d F?F?0?1?.|",usr->con_ip, conf.listen_port_udp);
		usr->idle=time(NULL);
	}
	else
		{
			if (conf.msg_motd && !user_tst_state(usr,STATE_REGISTERED))
			{
				pubmsg(usr,"\r\n%s",conf.msg_motd);
				pubmsg(usr,"------------------------------------");
			}
			
			if (!user_tst_state(usr,STATE_REGISTERED)
				&& conf.msg_usercommands 
				&& user_tst_supports(usr, SUPPORTS_UserCommand))
				disttcp(usr,conf.msg_usercommands);
			
			
			user_set_state(usr,STATE_REGISTERED);						
		}

	if(!compare_myinfo(&myinfo_tmp,&usr->myinfo)) return;	
	move_myinfo(&myinfo_tmp, &usr->myinfo);

	dc_myinfo(NULL,usr);
	
	if (!usr->userip[0]) snprintf(usr->userip, sizeof(usr->userip), "$UserIP %s %s|", usr->nick, usr->ip);
	if (user_tst_state(usr,STATE_REGISTERED)) disttcp_userip(usr);
}

/* $GetINFO <othernick> <nick>| */
void dc_GetINFO(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *par = param->param,
		 *othernick, 
		 *nick,
		*__strtok_temp__;
	int id;

	if (!par || !*par ||
		!user_tst_state(usr,STATE_REGISTERED|STATE_LOGGEDIN|STATE_SR)) return;
	
	othernick=strtok_r(par," ",&__strtok_temp__);
	nick=__strtok_temp__;
	
	if (!othernick || !nick || !*othernick || !*nick) return;
	
	if (strcmp(usr->nick,nick)) return;

	if(strcasecmp(usr->nick,nick))
	{
		pubmsg(usr,"GetINFO: Fake nick is not allowed");
		usr->reason=strdup("GetINFO: Fake nick is not allowed");
		user_set_state(usr,STATE_QUIT);
	}		
						
	id=nick2id(othernick);
	if (id<0) return;
	
	if(user[id] && user_tst_state(user[id],STATE_REGISTERED)) dc_myinfo(usr,user[id]);
}

/*
$Supports one[ two[ three]]]|

a part of EXTENDEDPROTOCOL
*/

void dc_Supports(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *par = param->param,
		 *__strtok_temp__,
		 supports[256],
		 *option;
	
	supports[0]='\000';

	strcat(supports,"$Supports");
	
	option = strtok_r(par, " ", &__strtok_temp__);

	if (!option) return;

	do{
		if(	!user_tst_supports(usr,SUPPORTS_NoHello) 
			&& !strcmp(option,"NoHello"))
		{
			strcat(supports," NoHello");
			user_set_supports(usr,SUPPORTS_NoHello);
		}

		if(	!user_tst_supports(usr,SUPPORTS_UserIP2) 
			&& !strcmp(option,"UserIP2"))
		{
			strcat(supports," UserIP2");
			user_set_supports(usr,SUPPORTS_UserIP2);
		}

		if(	!user_tst_supports(usr,SUPPORTS_NoGetINFO) 
			&& !strcmp(option,"NoGetINFO"))
		{
			strcat(supports," NoGetINFO");
			user_set_supports(usr,SUPPORTS_NoGetINFO);
		}

		if(	!user_tst_supports(usr,SUPPORTS_UserCommand) 
			&& !strcmp(option,"UserCommand"))
		{
			strcat(supports," UserCommand");
			user_set_supports(usr,SUPPORTS_UserCommand);
		}
		
		option=strtok_r(NULL," ",&__strtok_temp__);
	}while(option);

	strcat(supports," |");

	if(supports[0]) disttcp(usr,supports);
	else disttcp(usr,"$Supports |");
}

/*
$Search <ip>:<port> <sizerestricted>?<ismaxsize>?<size>?<datatype>?<searchpattern>
$Search Hub:<requestornick> <sizerestricted>?<ismaxsize>?<size>?<datatype>?<searchpattern>

$Search x1:x2 <sizerestricted>?<ismaxsize>?<size>?<datatype>?<searchpattern>
*/

void dc_Search(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *line = param->line,
		 *par = param->param,
		 *x1,
		 *x2,
		 *__strtok_temp__;

	if (!conf.allow_search 
		|| !par 
		|| !*par 
		|| !user_tst_state(usr,STATE_REGISTERED)
		|| (user_tst_penalty(usr,PENALTY_NOSEARCH) && !strchr(usr->perm,'s'))) return;
				
	if (!par || !*par) return;
	
	x1=strtok_r(par,":",&__strtok_temp__);
	x2=strtok_r(NULL," ",&__strtok_temp__);
	
	if(!x1 || !*x1 || !x2 || !*x2) return;
	
	if( conf.search_interval &&
		usr->last_search &&
		!strchr(usr->perm,'s') &&
		!strchr(usr->perm,'i') &&
		(difftime(time(NULL),usr->last_search))<(double)conf.search_interval)
	{
		if(difftime(time(NULL),usr->sent_search_interval)>60.0)
		{
			pubmsg(usr,"Search: Minimal interval between searches is %ds",conf.search_interval);
			usr->sent_search_interval=time(NULL);
		}

		return;
	}
		
	if(!strcmp(x1,"Hub"))
	{
		if(!conf.allow_passive && !strchr(usr->perm,'p'))
		{
			pubmsg(usr,"Search:	Passive mode is not allowed");
			usr->reason=strdup("Search: Passive mode is not allowed");			
			user_set_state(usr,STATE_QUIT);
			return;					
		}
		
		if(strcasecmp(usr->nick,x2))
		{
			pubmsg(usr,"Search: Fake nick is not allowed");
			usr->reason=strdup("Search: Fake nick is not allowed");
			user_set_state(usr,STATE_QUIT);
			return;					
		}				
	}
	else
		if( !conf.allow_forwarding && 
			!strchr(usr->perm,'f') &&
			strcasecmp(usr->ip,x1))
		{
			pubmsg(usr,"Search: Forwarding IP is not allowed");
			usr->reason=strdup("Search: Forwarding IP is not allowed");
			user_set_state(usr,STATE_QUIT);
			return;
		}		

	usr->last_search=time(NULL);
	disttcp(NULL,line);
//	distudp(NULL,line);
}

void dc_MultiSearch(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *line = param->line,
		 *par = param->param,
		 *x1,
		 *x2,
		 *__strtok_temp__;

	if (!NHUBS
		|| !conf.allow_search
		|| !par 
		|| !*par 
		|| !user_tst_state(usr,STATE_REGISTERED) 
		|| (user_tst_penalty(usr,PENALTY_NOSEARCH) && !strchr(usr->perm,'s'))) return;
				
	if (!par || !*par) return;
	
	x1=strtok_r(par,":",&__strtok_temp__);
	x2=strtok_r(NULL," ",&__strtok_temp__);
	
	if(!x1 || !*x1 || !x2 || !*x2) return;
	
	if( conf.search_interval 
		&& usr->last_search 
		&& !strchr(usr->perm,'s') 
		&& !strchr(usr->perm,'i') 
		&& (difftime(time(NULL),usr->last_search))<(double)conf.search_interval)
	{
		if(difftime(time(NULL),usr->sent_search_interval)>60.0)
		{
			pubmsg(usr,"MultiSearch: Minimal interval between searches is %ds",conf.search_interval);
			usr->sent_search_interval=time(NULL);
		}

		return;
	}

	if(!strcmp(x1,"Hub"))
	{
		pubmsg(usr,"MultiSearch: Passive mode is not allowed");

		if(!conf.allow_passive && !strchr(usr->perm,'p'))
		{
			usr->reason=strdup("MultiSearch: Passive mode is not allowed on this hub");
			user_set_state(usr,STATE_QUIT);
			return;					
		}
		
	}
	else		
		if( !conf.allow_forwarding 
			&& !strchr(usr->perm,'f') 
			&& strcasecmp(usr->ip,x1))
		{
			pubmsg(usr,"MultiSearch: Forwarding IP is not allowed");
			usr->reason=strdup("MultiSearch: Forwarding IP is not allowed");
			user_set_state(usr,STATE_QUIT);
			return;
		}		

	usr->last_search=time(NULL);

	line[5]='$';
	distudp(NULL,line+5);
}

/* $SR <source_nick> <result> <free_slots>/<total_slots>0x05<hub_name> (<hub_ip:listening_port>)0x05<target_nick> */
void dc_SR(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *line = param->line,
		 *par = param->param,	
		 *source_nick,
		 *target_nick,
		 *__strtok_temp__;

	int max=strlen(line)-1,
		i,
		id;

	if (!conf.allow_search
		|| max<3
		|| !par 
		|| !*par 
		|| !user_tst_state(usr,STATE_REGISTERED) 
		|| (user_tst_penalty(usr,PENALTY_NOSEARCH) && !strchr(usr->perm,'s'))) return;

	source_nick=strtok_r(par," ",&__strtok_temp__);
	if (!source_nick && !*source_nick) return;
	
	if(strcasecmp(usr->nick,source_nick))
	{
		pubmsg(usr,"SR: Fake nick is not allowed");
		usr->reason=strdup("SR: Fake nick is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;
	}

	if(max>=0) line[max]='\000';

	i=max;
					
	while(i>0 && line[i]!='\005') i--;
	
	if (!i || i>max-3) return;

	line[i]='|';
	target_nick=&line[i+1];
	
	if (!target_nick && !*target_nick) return;
		
	id=nick2id(target_nick);
	if (id<0) return;
	
	line[i+1]='\000';
	
	disttcp(user[id],line);
}

/* $Version <version> */
void dc_Version(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *ver = param->param;

	if (!ver || !*ver || strlen(ver)>VER_LEN) return;
	
	my_duplicate(ver,&usr->ver);				
}

/* $To: <othernick> From: <nick> $<<nick2>> <message> */
void dc_To(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	
	char *line = param->line,
		 *par = param->param,
		 *othernick,
		 *nick,
		 *nick2,
		 *msg,
		 *__strtok_temp__;

	int id;
	
	if (!conf.allow_chat ||
	    !par  || !*par ||
	    !user_tst_state(usr,STATE_REGISTERED)) return;

	othernick=strtok_r(par," ",&__strtok_temp__);	//<othernick>
		strtok_r(NULL," ",&__strtok_temp__);			// From:
	nick=strtok_r(NULL," ",&__strtok_temp__);		// nick
		strtok_r(NULL,"<",&__strtok_temp__);			// $<
	nick2=strtok_r(NULL,">",&__strtok_temp__);		// nick2
	msg=__strtok_temp__;
				
	if(!othernick || !nick || !nick2 || !msg || !*msg) return;

	if(strcmp(usr->nick,nick) || strcmp(usr->nick,nick2))
	{
		pubmsg(usr,"To: Fake nick is not allowed");
		usr->reason=strdup("To: Fake nick is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;
	}

	id=nick2id(othernick);
	if (id<0) return;

	// IF src user is punished and is not s'op and dest user is not a s'op
	if(	user_tst_penalty(usr,PENALTY_NOPRVCHAT) && 
		!strchr(usr->perm,'s') && !strchr(user[id]->perm,'s'))
	{
		privmsg(usr,user[id]->nick,"You're punished with no private chat");
		return;		
	}

	// IF dest user is punished and src user is not s'op			
	if(user_tst_penalty(user[id],PENALTY_NOPRVCHAT) && !strchr(usr->perm,'s'))
	{
		privmsg(usr,user[id]->nick,"I'm punished with no private chat");
		return;
	}
	
	if (user[id]->cons) disttcpf(user[id],"'%s'@%s: %s",usr->nick,usr->ip,msg);
	else disttcp(user[id],line);
}

/* $ConnectToMe <RemoteNick> <SenderIp>:<SenderPort> */
void dc_ConnectToMe(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *line = param->line,
		 *par = param->param,
		 *nick,
		 *IP,
		 *__strtok_temp__;

	int id;
		 
	if( !conf.allow_downloads || 
		!par || !*par ||
		!user_tst_state(usr,STATE_REGISTERED) || 
		(user_tst_penalty(usr,PENALTY_NODL) && !strchr(usr->perm,'s'))) return;
		 
	nick=strtok_r(par," ",&__strtok_temp__);
	IP=strtok_r(NULL,":",&__strtok_temp__);
	 
	if(!nick || !*nick || !IP || !*IP) return;
		 
	id=nick2id(nick);
	if(id<0)
	{
//		distudp(NULL,line);
		return;
	}
	
	if( !conf.allow_forwarding && 
		!strchr(usr->perm,'f') &&
		strcmp(usr->ip, IP))
	{
		pubmsg(usr,"ConnectToMe: Forwarding IP is not allowed");
		usr->reason=strdup("ConnectToMe: Forwarding IP is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;			
	}
		
	disttcp(user[id],line);
}

/* $RevConnectToMe <ClientNick> <TargetNick> */
void dc_RevConnecToMe(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *line = param->line,
		 *par = param->param,
		 *Cnick,
		 *Tnick,
		 *__strtok_temp__;
	int id;

	if(!conf.allow_downloads || 
		!par || !*par ||
		!user_tst_state(usr,STATE_REGISTERED) || 
		(user_tst_penalty(usr,PENALTY_NODL) && !strchr(usr->perm,'s'))) return;

	if(!conf.allow_passive && !strchr(usr->perm,'p'))
	{
		pubmsg(usr,"RevConnectToMe:	Passive mode is not allowed");
		usr->reason=strdup("RevConnectToMe:	Passive mode is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;
	}

	Cnick=strtok_r(par," ",&__strtok_temp__);
	Tnick=__strtok_temp__;
	
	if(!Cnick || !*Cnick || !Tnick || !*Tnick) return;

	if(strcmp(Cnick,usr->nick))
	{
		pubmsg(usr,"RevConnectToMe: Fake nick is not allowed");
		usr->reason=strdup("RevConnectToMe: Fake nick is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;
	}
	
	id=nick2id(Tnick);
	if(id<0) 
	{
//		distudpf(NULL,line);
		return;
	}
	
	disttcp(user[id],line);	
}

/* $MultiConnectToMe <RemoteNick> <SenderIp>:<SenderPort> */
void dc_MultiConnectToMe(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *line = param->line,
		 *par = param->param,
		 *IP;

	if (!NHUBS) return;
		
	 
	if( !NHUBS 
		|| !conf.allow_downloads 
		|| !par 
		|| !*par
		|| !user_tst_state(usr,STATE_REGISTERED) 
		|| (user_tst_penalty(usr,PENALTY_NODL) && !strchr(usr->perm,'s'))) 
			return;
		 
	IP=strsep(&par," ");
	IP=strsep(&par,":");
	 
	if(!par) return;
		 		
	if( !conf.allow_forwarding && 
		!strchr(usr->perm,'f') &&
		strcmp(usr->ip, IP))
	{
		pubmsg(usr,"MultiConnectToMe: Forwarding IP is not allowed");
		usr->reason=strdup("MultiConnectToMe: Forwarding IP is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;			
	}
	
	line[5]='$';
	distudp(NULL,line);
}

/* $Kick <victimNick> */
void dc_Kick(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *nick=param->param;
		
	int id,
		op_perm,
		usr_perm;

	if (!nick || !*nick || 
		!user_tst_state(usr,STATE_REGISTERED)) return;
			
	if (!strchr(usr->perm,'s') && !strchr(usr->perm,'a'))
	{	
		pubmsg(usr,"Kick: You are not Operator");
		usr->reason=strdup("Kick: You are not Operator");
		user_set_state(usr,STATE_QUIT);
		return;			
	}
	
	if(!strcmp(usr->nick,nick))
	{
		pubmsg(usr,"Kick: You try to kick yourself");
		return;
	}
	
	id=nick2id(nick);
	if(id<0)
	{
		pubmsg(usr,"Kick: You tried to kick '%s' who was not in userlist",nick);
		return;
	}

	if(strchr(usr->perm,'s')!=NULL) op_perm=2;
	else op_perm=1;
	
	usr_perm=0;
	if(strchr(user[id]->perm,'s')!=NULL) usr_perm=2;
	else if (strchr(user[id]->perm,'s')!=NULL) usr_perm=1;

	if(op_perm<=usr_perm)
	{
		pubmsg(usr,"Kick: You tried to kick '%s' without specific permissions",nick);

        log_write(FAC_USER,PR_ALERT,"'%s'@%s Kick: tried to kick '%s' without specific permissions",usr->nick,usr->ip,nick);
		return;
	}
	
	user[id]->reason=strdup("Kick: Successfully kicked");
	user_set_state(user[id],STATE_QUIT);
}

/* $OpForceMove $Who:<victimNick>$Where:<newAddr>$Msg:<reasonMsg> */
void dc_OpForceMove(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *par = param->param,
		 *nick,
		 *msg,
		 *addr,
		 *__strtok_temp__;
		
	int id,
		op_perm,usr_perm;

	if (!par || !*par || 
		!user_tst_state(usr,STATE_REGISTERED)) return;

	if (!strchr(usr->perm,'s') && !strchr(usr->perm,'a'))
	{	
		pubmsg(usr,"OpForceMove: You are not Operator");
		usr->reason=strdup("OpForceMove: You are not Operator");
		user_set_state(usr,STATE_QUIT);
		return;			
	}

	strtok_r(par,":",&__strtok_temp__);			//$Who:
	nick=strtok_r(NULL,"$",&__strtok_temp__);	//nick
	strtok_r(par,":",&__strtok_temp__);			//$Where:
	addr=strtok_r(NULL,"$",&__strtok_temp__);	//addr
	strtok_r(par,":",&__strtok_temp__);			//$Msg
	msg=__strtok_temp__;
	
	if(!nick || !*nick || !addr || !*addr || !msg || !*msg) return;
	
	id=nick2id(nick);
	if(id<0)
	{
		pubmsg(usr,"OpForceMove: You tried to redirect '%s' who was not in userlist",nick);
		return;
	}
	
	if(strchr(usr->perm,'s')!=NULL) op_perm=2;
	else op_perm=1;
	
	usr_perm=0;
	if(strchr(user[id]->perm,'s')!=NULL) usr_perm=2;
	else if (strchr(user[id]->perm,'a')!=NULL) usr_perm=1;

	if(op_perm<=usr_perm)
	{
		pubmsg(usr,"OpForceMove: You tried to redirect '%s' without specific permissions",nick);
        log_write(FAC_USER,PR_ALERT,"'%s'@%s OpForceMove: tried to redirect '%s' without specific permissions",usr->nick,usr->ip,nick);
		return;
	}
			
    log_write(FAC_USER,PR_INFO,"'%s'@%s OpForceMove: successfully redirected '%s'",usr->nick,usr->ip,nick);
	privmsg(usr,NULL,"%s",msg);
	
	disttcpf(usr,"$ForceMove %s|",addr);

	user[id]->reason=strdup("OpForceMove: successfully redirected");
	user_set_state(user[id],STATE_QUIT);
}

void dc_UserIP(dc_param_t *param)
{
/*  
	This command is not fully supported because
	UserIP is sent everytime nicklist is sent
*/
}

void dc_not_implemented(dc_param_t *param)
{
	char *line = param->line;

    log_write(FAC_USER,PR_INFO,"Not Implemented: '%s|'",line);
}

void dc_garbage(dc_param_t *param)
{
/*	userrec_t *usr = param->usr;

	user_set_state(usr,STATE_QUIT);
	usr->reason=strdup("sent garbage");
*/
}

void dc_chat(dc_param_t *param)
{
	userrec_t *usr = param->usr;
	char *cmd = param->cmd,
		 *line = param->line,
		 *par = param->param;

	if(!user_tst_state(usr,STATE_REGISTERED)) return;
		 
	if (cmd[0]!='<' || cmd[strlen(cmd)-1]!='>')
	{
		dc_garbage(param);
		return;		
	}
	
	cmd++;
	cmd[strlen(cmd)-1]='\000';
	
	if (strcmp(cmd,usr->nick))
	{
		pubmsg(usr,"Public: Fake nick is not allowed");
		usr->reason=strdup("Public: Fake nick is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;
	}
	
//	we need to add | which was previously parsed out
	strcat(par,"|");
	
	chat_cmd_exec(usr, par, line);
}

/*
void dc_cmd_exec(*usr,*line)

.description:
 runs the DC given command

.args:
	userrec_t *usr - userrec of the user, who gives the command
	char *line - the line following the command
{{{*/
void dc_cmd_exec( userrec_t *usr, char *line)
{
	cmd_t dc_set[]=
	{
		{ "ConnectToMe", 		(funct_t)dc_ConnectToMe			},
		{ "GetINFO", 			(funct_t)dc_GetINFO				},
		{ "GetNickList", 		(funct_t)dc_GetNickList			},
		{ "Key", 				(funct_t)dc_Key					},
		{ "Kick", 				(funct_t)dc_Kick				},
		{ "MultiConnectToMe", 	(funct_t)dc_MultiConnectToMe	},
		{ "MultiSearch", 		(funct_t)dc_MultiSearch			},
		{ "MyINFO", 			(funct_t)dc_MyINFO				},
		{ "MyPass", 			(funct_t)dc_MyPass				},
		{ "OpForceMove", 		(funct_t)dc_OpForceMove			},
		{ "Quit", 				(funct_t)dc_Quit				},
		{ "RevConnectToMe", 	(funct_t)dc_RevConnecToMe		},
		{ "SR", 				(funct_t)dc_SR					},
		{ "Search", 			(funct_t)dc_Search				},
		{ "Supports", 			(funct_t)dc_Supports			},
		{ "To:", 				(funct_t)dc_To					},
		{ "UserIP", 			(funct_t)dc_UserIP				},
		{ "ValidateNick", 		(funct_t)dc_ValidateNick		},
		{ "Version", 			(funct_t)dc_Version				},
		{ "",					(funct_t)dc_not_implemented		}
	};
	
	int n_dc_set=sizeof(dc_set)/sizeof(cmd_t);

	dc_param_t dc_param;

	char *cmd=NULL, //stores the command
		 *par=NULL, //stores the parameter
		 *line_copy=NULL, //stores the copy of line
		 *temp=NULL;

	my_duplicate(line,&line_copy);
	temp=line_copy;

	cmd=strsep(&temp," |");
	if (!temp) goto leave;

	par=strsep(&temp,"|");
		
	dc_param.usr=usr;
	dc_param.param=par;
	dc_param.line=line;
	dc_param.cmd=cmd;

	switch(cmd[0])
	{
		//chat
		case '<':
					dc_chat(&dc_param);
					break;		
		//dc
		case '$':	
					cmd++;
					bin_cmd_exec(dc_set, n_dc_set, cmd, &dc_param);
					break;		
		//garbage
		default:
					dc_garbage(&dc_param);
					break;					
	}

leave:
	my_free(line_copy);

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
