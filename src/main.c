/* $Id: main.c,v 1.504 2007/05/30 06:15:17 rav Exp $ */

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

/* reserved_nicks[] - nicks that can't be owned by normal users {{{ */
const char *reserved_nicks[]=
{
	"Hub",
	"Hub-Security",
	"Vandel\\Debug",
	"Client",
	"" /* indicates end of list */
}; /* }}} */

typedef union{
		void *voidp;
		int integer;
}int_voidp_t;

int NPENAL; /* penalties count */
time_t last_penalties_update=0;

int NUSERS; /* user count */
int NLOGINS; /* user accounts */
int NPATTERNS; /* nick patterns */
int NHUBS; /* amount of hubs */

int maxfd;

int rehash=0; // determines if #rehash command was used

Tpenalty *penalties[MAXPENALTIES]; /* table of penalties */

userrec_t *user[MAXUSERS];		/* users table */
loginrec_t *login[MAXUSERS];	/* accounts table */
char *pattern[MAXUSERS];			/* nick patterns table */
hub_t *hub[MAXHUBS]; /* hubs table */


config_t conf;		/* configuration data */

pthread_t listen_th_main[MAX_N_LISTEN_MAIN];	/* listen thread handle for main service */
pthread_t th_user_manager[MAX_N_USER_MANAGER] ;/* handles managing user's communicates */

pthread_t listen_th_cons;	/* listen thread handle for remote console */
pthread_t listen_th_udp;		/* listen thread handle for minslots checking */
pthread_t th_cleaner;

struct sockaddr_in local_main,local_udp, local_cons;

so_opts_t listen_opts,user_opts;

int signal_p=0;

pthread_mutex_t mutex_userlist;		/* securing userlist */
pthread_mutex_t mutex_inet_ntoa; // securing inet_ntoa

pthread_mutex_t mutex_user_manager[MAX_N_USER_MANAGER];

pthread_mutex_t mutex_is_managed; // secures that user is served only by one thread
int managed[MAX_N_USER_MANAGER]; // sets which thread is working on which user

pthread_mutex_t mutex_th_cleaner;
pthread_mutex_t mutex_th_udp;

pthread_mutex_t mutex_penalties;

pthread_mutex_t mutex_myinfo;


int listen_main;			/* main listening socket */
int listen_udp;				/* search result listening socket */
int listen_cons;			/* console listening socket */
time_t starttime;			/* start time in sec */

/* logging */
FILE* file_net;
FILE* file_proto;
FILE* file_wrng;
FILE* file_con;
FILE* file_chat;

int hub_start;

int test_penalty_counter=0;

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif

void lock_threads()
{
	int i;

	pthread_mutex_lock(&mutex_th_udp);
	pthread_mutex_lock(&mutex_th_cleaner);
	for(i=0;i<MAX_N_USER_MANAGER;i++) pthread_mutex_lock(&mutex_user_manager[i]);
}

void unlock_threads()
{
	int i;
	
	for(i=0;i<MAX_N_USER_MANAGER;i++) pthread_mutex_unlock(&mutex_user_manager[i]);
	pthread_mutex_unlock(&mutex_th_cleaner);
	pthread_mutex_unlock(&mutex_th_udp);
}

void make_sockets()
{
	/* setup and bind the listening socket */
	if ((listen_main=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
       	log_write(FAC_DAEMON,PR_CRITICAL,"socket() failed - terminating: %s.", strerror(errno));

		exit(1);
	}

	if (setsockopts(listen_main,&listen_opts)) 
	{
       	log_write(FAC_DAEMON,PR_CRITICAL,"setsockopts() failed - terminating: %s.", strerror(errno));
		exit(1);
	}

	memset(&local_main,0,sizeof(struct sockaddr_in));
	local_main.sin_family=AF_INET;
	local_main.sin_port=htons(conf.listen_port_main);
	inet_aton(conf.listen_interface,&local_main.sin_addr);

	if (bind(listen_main,(struct sockaddr *)&local_main,sizeof(struct sockaddr))<0)
	{
       	log_write(FAC_DAEMON,PR_CRITICAL,"bind() failed - terminating: %s.", strerror(errno));
		exit(1);
	}

	if (listen(listen_main,SOMAXCONN)<0)
	{
       	log_write(FAC_DAEMON,PR_CRITICAL,"listen() failed - terminating: %s.", strerror(errno));
		exit(1);
	}

	/* setup and bind the listening socket */
	if ((listen_udp=socket(AF_INET,SOCK_DGRAM,0))==-1)
	{
       	log_write(FAC_DAEMON,PR_CRITICAL,"socket() failed - terminating: %s.", strerror(errno));
		exit(1);
	}

	if (setsockopts(listen_udp,&listen_opts))
    {
        log_write(FAC_DAEMON,PR_CRITICAL,"setsockopts() failed.");
        exit(1);
    }

	memset(&local_udp,0,sizeof(struct sockaddr_in));
	local_udp.sin_family=AF_INET;
	local_udp.sin_port=htons(conf.listen_port_udp);

	inet_aton(conf.listen_interface,&local_udp.sin_addr);

	if (bind(listen_udp,(struct sockaddr *)&local_udp,sizeof(struct sockaddr))<0)
	{
       	log_write(FAC_DAEMON,PR_CRITICAL,"bind() failed - terminating: %s.", strerror(errno));
		exit(1);
	}

	if (conf.listen_port_cons)
	{
		/* setup and bind the remote console listening socket */
		if ((listen_cons=socket(AF_INET,SOCK_STREAM,0))==-1)
		{
           	log_write(FAC_DAEMON,PR_CRITICAL,"socket() failed - terminating: %s.", strerror(errno));

			exit(1);
		}

		if (setsockopts(listen_cons,&listen_opts)) 
		{
           	log_write(FAC_DAEMON,PR_CRITICAL,"setsockopts() failed - terminating: %s.", strerror(errno));
			exit(1);
		}
		
		memset(&local_cons,0,sizeof(struct sockaddr_in));

		local_cons.sin_family=AF_INET;
		local_cons.sin_port=htons(conf.listen_port_cons);
		inet_aton(conf.listen_interface,&local_cons.sin_addr);

		if (bind(listen_cons,(struct sockaddr *)&local_cons,sizeof(struct sockaddr))<0)
		{
           	log_write(FAC_DAEMON,PR_CRITICAL,"bind() failed - terminating: %s.", strerror(errno));
			exit(1);
		}

		if (listen(listen_cons,5)<0)
		{
           	log_write(FAC_DAEMON,PR_CRITICAL,"listen() failed - terminating: %s.", strerror(errno));
			exit(1);
		}
	}
}

int user_set_state(userrec_t *usr, int state)
{
	//if user quits his state cannot be changed
	if (user_tst_state(usr,STATE_QUIT)) return 0;
	
	if (state==STATE_QUIT)
	{    
	    if (user_tst_state(usr,STATE_REGISTERED|STATE_LOGGEDIN)) usr->state=STATE_QUIT_SHOW;
	    else usr->state=STATE_QUIT;
	}
	else usr->state=state;

	return 0;
}

int user_tst_state(userrec_t *usr, int state)
{
	return ((usr->state)&state)>0;
}


int user_set_supports(userrec_t *usr, int supports)
{
	usr->supports=usr->supports|supports;
	
	return 0;
}

int user_tst_supports(userrec_t *usr, int supports)
{

	return ((usr->supports)&supports)>0;
}

void userlist_add(userrec_t *usr)
{
	pthread_mutex_lock( &mutex_userlist );
	user[NUSERS]=usr;
	user[NUSERS]->id=NUSERS;
	NUSERS++;	
	pthread_mutex_unlock( &mutex_userlist );
}

void userlist_del(userrec_t *usr)
{
	int id;

	pthread_mutex_lock( &mutex_userlist );
	id=usr->id;
	user[id]=user[--NUSERS];
	user[id]->id=id;
	user[NUSERS]=NULL;
	pthread_mutex_unlock( &mutex_userlist );
}

/* adduser() - allocate and initialize new DC user {{{ */
void adduser(int sock,char *remote,char *local)
{
	char lock[SALT_LEN];
	userrec_t *tmp=NULL,tmpik;

	if (setsockopts(sock,&user_opts)) 
	{
        log_write(FAC_NETWORK,PR_ERROR,"setsockopts() failed for Unknown@%s: %s.", remote, strerror(errno));
		my_free(remote);
		my_free(local);
		close(sock);
		goto leave;		
	}

	/* we use select() anyway, but let's care for blocking send() */
	if (fcntl(sock,F_SETFL,O_NONBLOCK)<0)
	{
        log_write(FAC_NETWORK,PR_ERROR,"fcntl() failed for Unknown@%s: %s.", remote, strerror(errno));

		my_free(remote);
		my_free(local);
		close(sock);
		goto leave;
	}



	if((NUSERS>=conf.userlimit)||(NUSERS+1>=MAXUSERS)) /* '+1' reserves 1 slot for admin console */
	{
		tmpik.sock=sock;
		tmpik.cons=0;
		tmpik.nick=NULL;
		tmpik.ip=remote;

		disttcpf(&tmpik,"$Lock AAAAAAAAAABBBBBBBBBB Pk=01234|$HubName %s|",conf.hubname);

		pubmsg(&tmpik,"adduser: Hub is full");

		if (conf.redirect_switch&REDIRECT_SWITCH_HUB_IS_FULL)
		{
			pubmsg(&tmpik,"adduser: You are being redirected to %s",conf.redirect_hub_is_full);
			disttcpf(&tmpik,"$ForceMove %s|",conf.redirect_hub_is_full);

    	    log_write(FAC_USER,PR_INFO,"'Unknown'@%s disconnect: successfully redirected because hub is full",remote);
		}else
            log_write(FAC_USER,PR_INFO,"'Unknown'@%s disconnect: hub is full",remote);

		my_free(remote);
		my_free(local);
		close(sock);
		goto leave;
	}


	tmp=(userrec_t *)my_malloc(sizeof(userrec_t));
	memset(tmp,0,sizeof(userrec_t));

	user_set_state(tmp,STATE_WELCOME);

	/* getting user's ip addresses */
	tmp->ip=remote;
	tmp->con_ip=local;
	tmp->nick=NULL;

	tmp->buf=NULL;

	tmp->sock=sock;
	tmp->slots=0;
	tmp->slots_old=0;


	lock_gen(lock);							
	disttcpf(tmp,"$Lock %s Pk=DCONNECT..DAEMON|$HubName %s|",lock,conf.hubname);
	
	if(!conf.allow_broken_key) lock_key(tmp,lock);

	tmp->idle=time(NULL);
	tmp->last_search=0;

	tmp->perm=NULL;
	tmp->password=NULL;

	userlist_add(tmp);

leave:
	return;
} /* }}} */


/* deluser() - free normal or console user {{{ */
void deluser(userrec_t *usr)
{
	if (!usr) return;

	userlist_del(usr);

	if (user_tst_state(usr,STATE_SHOW_QUIT)) disttcpf(NULL,"$Quit %s|",usr->nick);
		
	disconnect(usr);
} /* }}} */

/* listen_thread_main() - listen for new connections to main service {{{ */
void listen_thread_main(void *voidp)
{
	int new_sock;
	socklen_t remote_size,size_addr=sizeof(struct sockaddr_in);
	struct sockaddr_in remote,addr;
	int No;
	int_voidp_t parameter;

	char *remote_ip,*local_ip;

	parameter.voidp=voidp;
	No=parameter.integer;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

	remote_size=sizeof(struct sockaddr_in);
	/* accept connections */
	while(1)
	{
		if ((new_sock=accept(listen_main,(struct sockaddr *)&remote,&remote_size))<0)
		{
			if (errno!=EINTR)  log_write(FAC_NETWORK,PR_ERROR,"accept() failed: %s.", strerror(errno));
			continue;
		}

		getsockname(new_sock,(struct sockaddr *)&addr,&size_addr);

	    log_write(FAC_ACCESS, PR_ALERT,"Connection to DC from %s:%d to %s.",remote_ip=my_inet_ntoa(remote.sin_addr),
															 ntohs(remote.sin_port),
															 local_ip=my_inet_ntoa(addr.sin_addr));
		adduser(new_sock,remote_ip, local_ip);
	}

	pthread_exit(0); /* never reached */
} /* }}} */

// this function is used to chect the minslots
void listen_thread_udp(void *args)
{
	unsigned int remote_size;

	int	n;
	struct sockaddr_in remote;

	char *ip=NULL, 
		  buffer[MAX_RECEIVE_ONCE];

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	
	remote_size=sizeof(struct sockaddr_in);

	while(1)
	{
		if(conf.minslots<=0 && !NHUBS)
		{
			sleep(1);
			continue;
		}
	
		n=recvfrom(listen_udp,buffer,MAX_RECEIVE_ONCE-1,0,(struct sockaddr*)&remote,&remote_size);
		if (n<=0) continue;

		buffer[n]='\000';
	
		pthread_mutex_lock(&mutex_th_udp);

		ip=my_inet_ntoa(remote.sin_addr);
        log_write(FAC_PROTOCOL,PR_INFO,"[UDP] RECV %s '%s'",ip, buffer);

		udp_cmd_exec(ip, buffer);

		my_free(ip);

		pthread_mutex_unlock(&mutex_th_udp);

	}

	pthread_exit(0); /* never reached */
} /* }}} */

/* finish() - signal handler and program termination {{{ */
void finish(int sig)
{
	int i;

	if ((sig!=SIGKILL || sig!=SIGSEGV) && signal_p) return;
	signal_p=1;

	if (sig!=SIGPIPE) log_write(FAC_DAEMON,PR_ALERT,"SIG%s%s%s%s%s%s received - %s%s%s%s",	
																		sig==SIGHUP?"HUP":"",
																		sig==SIGKILL?"KILL":"",
																		sig==SIGTERM?"TERM":"",
																		sig==SIGQUIT?"QUIT":"",
																		sig==SIGSEGV?"SEGV":"",
																		sig==SIGINT?"INT":"",
																		
																		sig==SIGHUP?"reconfiguring":"",
																		sig==SIGSEGV?"crashing":"",
																		sig==SIGKILL?"fast terminating":"",
																		(sig==SIGTERM||sig==SIGQUIT||sig==SIGINT)?"cleaning and terminating":"");
																		
	switch(sig)
	{
		/* rehash on SIGHUP */
		case SIGHUP:
					/* blocking working threads */

					lock_threads();

					parse_config(0,&conf);
					make_paths(&conf);					
					log_open();
					parse_hublist();
                    
					signal_p=0;
					unlock_threads();
					return;

		case SIGSEGV:
					close(listen_main);
					if (conf.listen_port_cons) close(listen_cons);
					if (conf.minslots)close(listen_udp);					

					log_close();
					exit(SIGSEGV);

		case SIGKILL:
					close(listen_main);
					if (conf.listen_port_cons) close(listen_cons);
					if (conf.minslots)close(listen_udp);					
					exit(SIGKILL);

		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
					lock_threads();

					for(i=0;i<conf.n_listen_main;i++) pthread_cancel(listen_th_main[i]);
					for(i=0;i<conf.n_user_manager;i++) pthread_cancel(th_user_manager[i]);

					pthread_cancel(listen_th_udp);
					if (conf.listen_port_cons) pthread_cancel(listen_th_cons);
					pthread_cancel(th_cleaner);

					for (i=0;i<NUSERS;i++) disconnect(user[i]);

					close(listen_main);
					if (conf.listen_port_cons) close(listen_cons);
					if (conf.minslots)close(listen_udp);					

					exit(sig);

		/* ignore any other signal */
		default:
				signal_p=0;
				return;
	}
} /* }}} */

/* buf2line() - extract single request line from buf {{{ */
char *buf2cmd(userrec_t *usr)
{
	char delim_con[2]="\n",
		 delim_cmd[2]="|",
		 *delim=delim_cmd,
		 *line=NULL,
		 *cmd=NULL, 
		 *tmp=NULL,
		 *temp=NULL;
		 
		 int len;
	
	if (!usr->buf || !*(usr->buf)) goto leave;

	if (usr->cons) delim=delim_con;
	
	temp=usr->buf;

	line=strsep(&temp,delim);
	
	if (!temp) return NULL;
		
	len=strlen(line);

	cmd=my_malloc(len+2);
	strcpy(cmd,line);

	cmd[len]=delim[0];
	cmd[len+1]='\000';
	
	if (temp && *temp) my_duplicate(temp,&tmp);
	my_free(usr->buf);
	usr->buf=tmp;

	if (usr->cons && user_tst_state(usr, STATE_KEY|STATE_PASSWORD) && !cmd) user_set_state(usr, STATE_QUIT);

leave:
	return cmd;
} /* }}} */



void user_msg2buf(userrec_t *usr, char *message)
{
	int len;
	len=add2buf(&usr->buf, message);
	
	if(len>MAX_USER_BUFFER)
	{
		user_set_state(usr,STATE_QUIT);
		usr->reason=strdup("user_msg2buf: client was too talkative");
	}
}

/* manage_sock() - proces requests remaining on a socket {{{ */
void manage_sock( userrec_t *usr)
{
	char *line=NULL;
	int counter=conf.max_commands;

	while((!conf.max_commands || (conf.max_commands && counter--)) 
			&& (line=(char *)buf2cmd(usr))) //makes a line from buf
	{		
        log_write(FAC_PROTOCOL,PR_INFO,"[TCP] RECV '%s'@%s '%s'",usr->nick?usr->nick:"Unknown",usr->ip,line);

		if (!usr->cons) dc_cmd_exec(usr,line);
		else if (manage_cons(usr,line)) break;

		my_free(line);
	}

	my_free(line);

} /* }}} */

int is_managed(int No,int usr_no)
{
	int RC=0, i;

	pthread_mutex_lock(&mutex_is_managed);

	if (usr_no>=0)
	{
		for (i=0;!RC && i<MAX_N_USER_MANAGER; i++)
			if (managed[i]==usr_no) RC=1;

		if (!RC && No>=0) managed[No]=usr_no;
		
	} else managed[No]=-1;
		
	pthread_mutex_unlock(&mutex_is_managed);
	return RC;
}


void test_slots( userrec_t *usr)
{	
	if (usr->slots>999 || usr->slots<0)
	{
		pubmsg(usr,"More than 999 slots is not allowed");		
		usr->reason=strdup("test_slots: more than 999 slots is not allowed");
		user_set_state(usr,STATE_QUIT);
		return;
	}

	if(usr->slots>=conf.minslots)
	{
		user_set_state(usr,STATE_REGISTERED);
		dc_myinfo(NULL,usr);
		disttcp_userip(usr);

		if(usr->slots==usr->slots_old) return;
						
//		log_write(FAC_USER,PR_INFO,"'%s'@%s: test_slots: minslots checked",usr->nick,usr->ip);
		pubmsg(usr,"The minimal amount of slots reached");
		pubmsg(usr,"-----------------------------------");
		
		if (conf.msg_motd && !usr->slots_old) 
		{	
			pubmsg(usr,"\r\n%s", conf.msg_motd);
			pubmsg(usr,"-----------------------------------");
		}
		
		if (!usr->slots_old 
			&& conf.msg_usercommands 
			&& user_tst_supports(usr,SUPPORTS_UserCommand))		
				disttcp(usr,conf.msg_usercommands);
		 				
		usr->slots_old=usr->slots;
		return;
	}

	pubmsg(usr,"The minimum number of upload slots is %d.",conf.minslots);

	if (conf.redirect_switch&REDIRECT_SWITCH_MINSLOTS)
	{
		pubmsg(usr,"You are being redirected to %s",conf.redirect_minslots);
		disttcpf(usr,"$ForceMove %s|",conf.redirect_minslots);
	}
	else pubmsg(usr,"Update its number and reconnect");

	usr->reason=strdup("not enough upload slots");
	user_set_state(usr,STATE_QUIT);
}


void user_manager(void *voidp)
{
	int i,No,maxsock;
	int_voidp_t parameter;
	struct timeval timeout;
	char message[MAX_RECEIVE_ONCE];

	fd_set _sockfdset;
	fd_set *sockfdset=&_sockfdset;

	parameter.voidp=voidp;
	No=parameter.integer;

	FD_ZERO(sockfdset);

	while(1)
	{
		if (conf.minimal_sleep_time) usleep(conf.minimal_sleep_time);		

		if(!NUSERS)
		{
			usleep(5000);
			continue;
		}

		pthread_mutex_lock( &mutex_user_manager[No]);

       	timeout.tv_sec=0;
		timeout.tv_usec=5000;

		maxsock=-1;
		FD_ZERO(sockfdset);
		for(i=0;i<NUSERS;i++) 
			if (user[i] && !user_tst_state(user[i],STATE_QUIT))
			{
				FD_SET(user[i]->sock,sockfdset);
				if(user[i]->sock>maxsock) maxsock=user[i]->sock;
			}

		select(maxsock+1,sockfdset,NULL,NULL,&timeout);

		for(i=0;i<NUSERS;i++)
		{
			if (user[i]  && !user_tst_state(user[i],STATE_QUIT) && !is_managed(No,i))
			{
				if (conf.minslots && user_tst_state(user[i],STATE_SLOTS_TESTED)) test_slots(user[i]);

				if (user_tst_state(user[i],STATE_WELCOME))
				{				
				    if (conf.msg_welcome) 
				    {
						pubmsg(user[i],"\r\n%s",conf.msg_welcome);
						pubmsg(user[i],"-----------------------------------");
				    }

				    user_set_state(user[i],STATE_CONNECTED);
				}

				if( FD_ISSET(user[i]->sock,sockfdset) 
					&& my_recv(user[i], conf.max_receive_once, message)) user_msg2buf(user[i],message);

				if (user[i]->buf) manage_sock(user[i]);
			    
				is_managed(No,-1);
			}
		}
		pthread_mutex_unlock( &mutex_user_manager[No] );
	}

	exit(1); /* never reached */
}


/* init_var() - install the signal handler and initialize variables {{{ */
void init_var(char *argv0)
{
	struct passwd *user_data;
	struct group *group_data;

	/* clear main variables */
	NUSERS=0;
	NHUBS=0;
	NPENAL=0;
	NLOGINS=0;
	NPATTERNS=0;

	/* install signal handlers (sigaction() with SA_RESTART keeps the
	   portable, glibc-signal()-style semantics we rely on) */
	{
		struct sigaction sa;

		memset(&sa,0,sizeof(sa));
		sa.sa_handler=finish;
		sa.sa_flags=SA_RESTART;
		sigemptyset(&sa.sa_mask);

		sigaction(SIGPIPE,&sa,NULL);
		sigaction(SIGHUP,&sa,NULL);
		sigaction(SIGINT,&sa,NULL);
		sigaction(SIGQUIT,&sa,NULL);
		sigaction(SIGTERM,&sa,NULL);
		sigaction(SIGSEGV,&sa,NULL);
	}

	memset(penalties,0,MAXPENALTIES*sizeof(Tpenalty *));
	memset(user,0,MAXUSERS*sizeof(userrec_t *));
	memset(&conf,0,sizeof(config_t));

	/* set up default config values */

	conf.conf_main=(char *)my_malloc(strlen(CONFIGDIR)+strlen(CONF_MAIN)+2); /* one byte for '/' and one for '\0' */
	sprintf(conf.conf_main, "%s%s%s", CONFIGDIR, (CONFIGDIR[strlen(CONFIGDIR)-1]=='/')?"":"/", CONF_MAIN);

	conf.conf_welcome=strdup(CONF_WELCOME);
	conf.conf_motd=strdup(CONF_MOTD);
	conf.conf_banned=strdup(CONF_BANNED);
	conf.conf_cusers=strdup(CONF_CUSERS);
	conf.conf_callow=strdup(CONF_CALLOW);
	conf.conf_nallow=strdup(CONF_NALLOW);
	conf.conf_penalties=strdup(CONF_PENALTIES);
	conf.conf_hublinks=strdup(CONF_HUBLINKS);
	conf.conf_usercommands=strdup(CONF_USERCOMMANDS);
	conf.conf_rules=strdup(CONF_RULES);

	conf.msg_welcome=NULL;
	conf.msg_motd=NULL;
	conf.msg_rules=NULL;
	conf.msg_usercommands=NULL;

    conf.log_via_syslog=1;
    conf.log_priority=1023;

	conf.listen_interface=strdup(LISTEN_INTERFACE);
	conf.admin_contact=strdup(ADMIN_CONTACT);
	conf.listen_port_main=LISTEN_PORT_MAIN;
	conf.listen_port_cons=LISTEN_PORT_CONS;

	conf.redirect_switch=REDIRECT_SWITCH_OFF;
	conf.redirect_hub_is_full=strdup(REDIRECT_HUB_IS_FULL);
	conf.redirect_minshare=strdup(REDIRECT_MINSHARE);
	conf.redirect_minslots=strdup(REDIRECT_MINSLOTS);

	conf.penalties_update_interval=PENALTIES_UPDATE_INTERVAL;
	conf.search_interval=SEARCH_INTERVAL;

	conf.allow_non_us_ascii_nicks=ALLOW_NON_US_ASCII_NICKS;

	conf.allow_downloads=ALLOW_DOWNLOADS;
	conf.allow_search=ALLOW_SEARCH;

	conf.n_listen_main=N_LISTEN_MAIN;
	conf.n_user_manager=N_USER_MANAGER;
	conf.max_commands=MAX_COMMANDS;
	conf.max_receive_once=MAX_RECEIVE_ONCE;

	user_data=getpwnam(DEFAULT_USER);
	if(!user_data)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"User `%s' not found - terminating.",DEFAULT_USER);
		exit(1);
	}

	conf.uid=user_data->pw_uid;
	conf.user=strdup(DEFAULT_USER);

	group_data=getgrnam(DEFAULT_GROUP);

	if(!group_data)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"Group `%s' not found - terminating.",DEFAULT_GROUP);
		exit(1);
	}
	
	conf.group=strdup(DEFAULT_GROUP);

	conf.gid=group_data->gr_gid;

	conf.hubname=strdup(HUBNAME);

	conf.userlimit=USERLIMIT;
	conf.allow_broken_key=ALLOW_BROKEN_KEY;
	conf.allow_chat=ALLOW_CHAT;
	conf.allow_passive=ALLOW_PASSIVE;
	conf.allow_passive=ALLOW_FORWARDING;
	conf.registered_only=REGISTERED_ONLY;
	conf.minslots=0;
	conf.minshare=0;


	conf.std_penalty_duration=STD_PENALTY_DURATION;

	conf.idle_timeout=PING_TIMEOUT;
	conf.register_timeout=REGISTER_TIMEOUT;

	conf.minimal_sleep_time=MINIMAL_SLEEP_TIME;

	conf.max_chat_length=MAX_CHAT_LENGTH;
	conf.kick_max_chat_length=KICK_MAX_CHAT_LENGTH;
	/* end of default config values */

	/* logging file handlers */
	file_net=NULL;
	file_proto=NULL;
	file_wrng=NULL;
	file_chat=NULL;


	/* socket options*/
	listen_opts.so_debug=0;
	listen_opts.so_broadcast=0;
	listen_opts.so_reuseaddr=1;
	listen_opts.so_keepalive=1;

	listen_opts.so_linger.l_onoff=1;
	listen_opts.so_linger.l_linger=0;

	listen_opts.so_oobinline=1;
	listen_opts.so_sndbuf=VAL_SO_SNDBUF;
	listen_opts.so_rcvbuf=VAL_SO_RCVBUF;
	listen_opts.so_dontroute=0;
	listen_opts.so_rcvlowat=1;
	listen_opts.so_rcvtimeo=0;
	listen_opts.so_sndlowat=1;
	listen_opts.so_sndtimeo=0;

	user_opts.so_debug=0;
	user_opts.so_broadcast=0;
	user_opts.so_reuseaddr=0;
	user_opts.so_keepalive=1;

	user_opts.so_linger.l_onoff=1;
	user_opts.so_linger.l_linger=0;

	user_opts.so_oobinline=1;
	user_opts.so_sndbuf=VAL_SO_SNDBUF;
	user_opts.so_rcvbuf=VAL_SO_RCVBUF;
	user_opts.so_dontroute=0;
	user_opts.so_rcvlowat=1;
	user_opts.so_rcvtimeo=0;
	user_opts.so_sndlowat=1;
	user_opts.so_sndtimeo=0;

	/* other */
	__progname=get_progname(argv0);
	randomize();
	time(&starttime);
} /* }}} */


void cleaner()
{
	int i;

	time_t time_;

	/* main loop - finish only when error or killed with a signal */
	while(1)
	{
		sleep(CLEANER_INTERVAL);
		lock_threads();

		parse_messages(1,&conf.msg_motd, conf.conf_motd);
		parse_messages(1,&conf.msg_welcome, conf.conf_welcome);
		parse_messages(1,&conf.msg_rules, conf.conf_rules);
		parse_messages(0,&conf.msg_usercommands, conf.conf_usercommands);
		
		parse_logins();
		parse_nick_patterns();
		
		time(&time_);
		if (NPENAL 
			&& conf.penalties_update_interval 
			&& difftime(time_,last_penalties_update)>(double)conf.penalties_update_interval)
		{
			last_penalties_update=time_;
			penalties_write();
		}

		disttcp(NULL,"$|");
		
		for(i=0;i<NUSERS;i++)
			if (user[i])
			{
				check_timeout(user[i]);
				
				/* host is banned permanently*/
				if (validhost(user[i],conf.conf_banned,1)) 
				{
					user[i]->reason=strdup("is permanently banned");
					user_set_state(user[i],STATE_QUIT);
				}

				if (user_tst_state(user[i],STATE_QUIT)) deluser(user[i--]);
			}

		for(i=0; i<NHUBS; i++)		
			if(difftime(time(NULL), hub[i]->timeout)>120.0
				&& difftime(time(NULL), hub[i]->timeout_retry)>120.0)
			{
				log_write(FAC_ACCESS,PR_INFO,"HUBNET try to link with %s:%s", hub[i]->ip, hub[i]->port);
				distudp(hub[i],hub[i]->Up_cache);
				hub[i]->timeout_retry=time(NULL);
			}

		unlock_threads();

	}
}

void make_threads()
{
	int_voidp_t i;
	
	/* create listening threads for main service */
	for(i.integer=0;i.integer<MAX_N_LISTEN_MAIN;i.integer++)
	{

		if (i.integer<conf.n_listen_main &&
			(listen_main==-1 || pthread_create( &listen_th_main[i.integer], NULL, (void *)listen_thread_main, i.voidp) ))
		{
            log_write(FAC_DAEMON,PR_CRITICAL,"Couldn't create the main service thread - terminating: %s.", strerror(errno));
			exit(1);
		}
	}

	/* create users managers threads */
	for(i.integer=0;i.integer<MAX_N_USER_MANAGER;i.integer++)
	{
		managed[i.integer]=-1;

		if (pthread_mutex_init(&mutex_user_manager[i.integer],0))
		{
			 log_write(FAC_DAEMON,PR_CRITICAL,"pthread_mutex_init() failed: %s", strerror(errno));
			exit(1);
		}

		if (i.integer<conf.n_user_manager &&
			(pthread_create( &th_user_manager[i.integer], NULL, (void *)user_manager, i.voidp) ))
		{
			 log_write(FAC_DAEMON,PR_CRITICAL,"pthread_create() failed: %s", strerror(errno));
			exit(1);
		}
	}

	/* create cleaner thread */
	if ((pthread_create( &th_cleaner, NULL, (void *)cleaner, NULL)!=0))
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"pthread_create() failed: %s", strerror(errno));
		exit(1);
	}

	/* create minslots checking thread */
	if (listen_udp==-1 || pthread_create( &listen_th_udp, NULL, (void *)listen_thread_udp, NULL)!=0)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"pthread_create() failed: %s", strerror(errno));
		exit(1);
	}

	/* create listening thread for remote console */
	if (conf.listen_port_cons && (listen_cons==-1 || pthread_create( &listen_th_cons, NULL, (void *)listen_thread_cons, NULL)!=0))
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"pthread_create() failed: %s", strerror(errno));
		exit(1);
	}
}


void initialize_semaphores()
{

	if (pthread_mutex_init(&mutex_userlist, 0)
		|| pthread_mutex_init(&mutex_penalties, 0)
		|| pthread_mutex_init(&mutex_is_managed, 0)
		|| pthread_mutex_init(&mutex_inet_ntoa, 0)
		|| pthread_mutex_init(&mutex_th_cleaner,0)
		|| pthread_mutex_init(&mutex_th_udp,0)
		|| pthread_mutex_init(&mutex_myinfo,0))
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"pthread_mutex_init() failed: %s", strerror(errno));
		exit(1);
	}
}

/* main() - main program {{{ */
int main(int argc,char *argv[])
{
	log_open();

#ifdef USE_GETTEXT
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE_NAME, LOCALE_DIR);
	textdomain(PACKAGE_NAME);
#endif

	/* init variables */
	init_var(argv[0]);
	maxfd=(int)sysconf(_SC_OPEN_MAX);
	if (maxfd<0) maxfd=FD_SETSIZE;

	if(maxfd>FD_SETSIZE) maxfd=FD_SETSIZE;

	/* parse command-line options */
	opt_line(argc,argv);

	/* read config file */
	if (parse_config(1,&conf)) exit(1);
	make_paths(&conf);

	make_sockets();

	/* set uid/gid to proper user/group */
	if (setgid(conf.gid) && !geteuid())
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"setgid(%d) failed: %s.",conf.gid,strerror(errno));
		exit(1);
	}
	if (setuid(conf.uid) && !geteuid())
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"setuid(%d) failed: %s.",conf.uid,strerror(errno));
		exit(1);
	}

	log_open();

	parse_messages(1,&conf.msg_motd, conf.conf_motd);
	parse_messages(1,&conf.msg_welcome, conf.conf_welcome);
	parse_messages(1,&conf.msg_rules, conf.conf_rules);
	parse_messages(0,&conf.msg_usercommands, conf.conf_usercommands);

	parse_logins();
	parse_nick_patterns();
	parse_hublist();

	initialize_semaphores();

	/* daemonize */
	if (daemon(0,0)<0)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"Couldn't fall into background: %s.", strerror(errno));
		exit(1);
	}

    log_write(FAC_DAEMON,PR_INFO,"Daemon started.");

	penalties_read();
	make_threads();

	while(1)
	{
		sleep(1);
		if(rehash)
		{
			finish(SIGHUP);
			rehash=0;
		}
	}

	exit(1); /* never reached */
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
