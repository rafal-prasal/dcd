/* $Id: cmd.udp.c,v 1.19 2007/02/19 20:59:18 rav Exp $ */

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

extern Tpenalty *penalties[];	/* table of penalties */
extern userrec_t *user[];		/* users table */
extern hub_t *hub[];			/* hubs table */

extern config_t conf;			/* configuration data */

extern int rehash;

extern pthread_mutex_t mutex_myinfo;

/* $Search <ip>:<port> <sizerestricted>?<ismaxsize>?<size>?<datatype>?<searchpattern> */
void udp_Search(udp_param_t *param)
{
	int id_hub;
	
	char *ip=param->ip,
		 *line=param->line,
		 *par=param->param,
		 *ip_user=NULL;
		 
		 		 
	if( !NHUBS
		|| !conf.allow_search) return;

	id_hub=ip2hub(ip);
	
	ip_user=strsep(&par,":");
			
	if( !par 
		|| id_hub<0
		|| difftime(time(NULL),hub[id_hub]->timeout)>120.0
		|| !strcmp(ip_user,"Hub"))
			return;
	
	hub[id_hub]->timeout=time(NULL);
	
	disttcp(NULL,line); 
}

/* $SR <source_nick> <result>0x05<free_slots>/<total_slots>0x05<hub_name> (<hub_ip:listening_port>)0x05<target_nick> */
void udp_SR(udp_param_t *param)
{
	userrec_t *usr;
	
	int id;
	
	char *ip=param->ip,
		 *par=param->param,
		 *nick=NULL,
		 *s_slots;

	if(conf.minslots<=0) return;		 

	nick=strsep(&par," ");
	s_slots=strsep(&par,"\005");
	s_slots=strsep(&par,"/");
	s_slots=strsep(&par,"\005");
		
	if( !par
		|| (id=nick2id(nick))<0) return;
																																																																																																																					
	usr=user[id];
																																																																																																																									
	if (!user_tst_state(usr,STATE_SR) 
		|| strchr(usr->perm,'s') 
		|| strchr(usr->perm,'r') 
		|| (usr->ip 
			&& strcmp(usr->ip,ip))) return;

	usr->slots=strtol(s_slots, &par, 10);
	user_set_state(usr,(usr->state^STATE_SR)|STATE_SLOTS_TESTED);
}

/* $ConnectToMe <RemoteNick> <SenderIp>:<SenderPort> */
void udp_ConnectToMe(udp_param_t *param)
{
	int id_hub, id_usr;
	
	char *ip=param->ip,
		 *line=param->line,
		 *par=param->param,
		 *nick=NULL;

	if( !NHUBS
		|| !conf.allow_downloads) return;		 		 

	id_hub=ip2hub(ip);
	
	if( id_hub<0
		|| difftime(time(NULL),hub[id_hub]->timeout)>120.0) 
	 		return;

	hub[id_hub]->timeout=time(NULL);
	
	nick=strsep(&par," ");
	
	if(!par) return;
		
	id_usr=nick2id(nick);

	if(!id_usr) return;

	disttcp(user[id_usr],line);
}

/* $Up <pass_in> <hubip>:<hubport> */
void udp_Up(udp_param_t *param)
{
	int id_hub;
	
	char *ip=param->ip,
		 *par=param->param,
		 *pass_in=NULL,
		 *hubip=NULL,
		 *hubport=NULL;

	if(!NHUBS) return;

	pass_in=strsep(&par," ");
	hubip=strsep(&par,":");
	hubport=strsep(&par,"|");

	if(!par) return;

	id_hub=ip2hub(ip);
	
	if( id_hub<0 
		|| strcmp(pass_in,hub[id_hub]->pass_in)
		|| strcmp(hubip,ip)
		|| strcmp(hubport,hub[id_hub]->port)
		)
	{
        log_write(FAC_ACCESS,PR_ALERT,"HUBNET %s is not linked",ip);
		return;
	}

	if (difftime(time(NULL),hub[id_hub]->timeout)>120.0) 
        log_write(FAC_ACCESS,PR_INFO,"HUBNET %s:%s is linked",hub[id_hub]->ip,hub[id_hub]->port);

	distudp(hub[id_hub],hub[id_hub]->UpToo_cache);

	hub[id_hub]->timeout=time(NULL);
}

/* $UpToo <pass_in> <hubip>:<hubport> */
void udp_UpToo(udp_param_t *param)
{
	int id_hub;
	
	char *ip=param->ip,
		 *par=param->param,
		 *pass_in=NULL,
		 *hubip=NULL,
		 *hubport=NULL;

	if(!NHUBS) return;

	pass_in=strsep(&par," ");
	hubip=strsep(&par,":");
	hubport=strsep(&par,"|");

	if (!par) return;

	id_hub=ip2hub(ip);
		
	if( id_hub<0 
		|| strcmp(pass_in,hub[id_hub]->pass_in)
		|| strcmp(hubport,hub[id_hub]->port)
		|| strcmp(hubip,ip)) 
	{
        log_write(FAC_ACCESS,PR_ALERT,"HUBNET %s is not linked",ip);
        return;
	}

	if (difftime(time(NULL),hub[id_hub]->timeout)>120.0) 
        log_write(FAC_ACCESS,PR_INFO,"HUBNET %s:%s is linked",hub[id_hub]->ip,hub[id_hub]->port);
		
	hub[id_hub]->timeout=time(NULL);
}

void udp_garbage(udp_param_t *param)
{

}


void udp_not_implemented(udp_param_t *param)
{

}

/*
void udp_cmd_exec(*usr,*line)

.description:
 runs the DC-udp given command

.args:
	int sock, socket
	char *line - the line following the command
{{{*/
void udp_cmd_exec(char *ip, char *line)
{
	cmd_t udp_set[]=
	{
		{ "ConnectToMe", 		(funct_t)udp_ConnectToMe		},
		{ "SR", 				(funct_t)udp_SR					},
		{ "Search", 			(funct_t)udp_Search				},
		{ "Up",					(funct_t)udp_Up					},
		{ "UpToo",	 			(funct_t)udp_UpToo				},
		{ "",					(funct_t)udp_not_implemented	}
	};
	
	int n_udp_set=sizeof(udp_set)/sizeof(cmd_t);

	udp_param_t udp_param;

	char *cmd=NULL, //stores the command
		 line_copy[MAX_RECEIVE_ONCE], //stores the copy of line
		 *tmp=line_copy;

	if(NHUBS)
	{
		strncpy(tmp,line,MAX_RECEIVE_ONCE-1);
		tmp[MAX_RECEIVE_ONCE-1]='\0';
	}
	else tmp=line;

	cmd=strsep(&tmp," ");

	if (!tmp) return;

	switch(cmd[0])
	{
		//udp
		case '$':	
					cmd++;
					udp_param.ip=ip;
					udp_param.param=tmp;
					udp_param.line=line;
					udp_param.cmd=cmd;
					
					bin_cmd_exec(udp_set, n_udp_set, cmd, &udp_param);
					break;		
		//garbage
		default:
					udp_garbage(&udp_param);
					break;					
	}	

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
