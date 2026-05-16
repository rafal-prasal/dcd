/* $Id: console.c,v 1.68 2007/02/20 08:30:57 rav Exp $ */

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

extern userrec_t *user[MAXUSERS];	/* users table */
extern config_t conf;		/* configuration data */
extern pthread_t listen_th_cons;	/* listen thread handle for remote console */
extern pthread_mutex_t mutex;		/* provides synchronization between threads */
extern pthread_mutex_t mutex_listen_cons;
extern fd_set sockfdset;		/* descriptors */
extern int listen_cons;			/* console listening socket */
extern pthread_mutex_t mutex_inet_ntoa; // securing inet_ntoa

/* addcons() - allocate and initialize new console user {{{ */
void addcons(int sock,char *remote) {
	userrec_t *tmp;

	tmp=(userrec_t *)my_malloc(sizeof(userrec_t));
	memset(tmp,0,sizeof(userrec_t));

	user_set_state(tmp,STATE_CONNECTED);
	tmp->sock=sock;
	tmp->cons=1;
	tmp->ip=remote;

	tmp->myinfo.flag=1;

	if (!validhost(tmp, conf.conf_callow,1))
	{
		disconnect(tmp);
		return;
	}

	if (setsockopts(sock,0)) log_write(FAC_NETWORK,PR_ERROR,"setsockopts() failed.");

	/* we use select() anyway, but let's care for blocking send() */
	if (fcntl(sock,F_SETFL,O_NONBLOCK)<0) log_write(FAC_NETWORK,PR_ERROR,"fcntl() failed: %s.", strerror(errno));

	if (NUSERS>=MAXUSERS)
	{
		pubmsg(tmp,"UsersLimit already reached");
		disconnect(tmp);
		return;
	}

	tmp->nick=NULL;
	tmp->idle=time(NULL);
	
	strcpy(tmp->myinfo.ctype,"Console");

	disttcpf(tmp,"\r\nDConnect Daemon v%s\r\nlogin: ",PACKAGE_VERSION);

	user_set_state(tmp,STATE_KEY);

	userlist_add(tmp);
} /* }}} */

/* listen_thread_cons() - listen for new connections to remote console {{{ */
void listen_thread_cons(void *args) {
	unsigned int new_sock,remote_size;
	struct sockaddr_in remote;
	char *ip;
	fd_set fdset;

	FD_ZERO(&fdset);

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

	remote_size=sizeof(struct sockaddr_in);
	/* accept connections */
	while(1)
	{
		remote_size=sizeof(struct sockaddr_in);
		if ((new_sock=accept(listen_cons,(struct sockaddr *)&remote,&remote_size))<0)
		{
			if (errno!=EINTR) log_write(FAC_NETWORK,PR_ERROR,"accept() failed: %s.", strerror(errno));
			continue;
		}

		log_write(FAC_ACCESS,PR_INFO,"Connection to console from %s:%d.",ip=my_inet_ntoa(remote.sin_addr),ntohs(remote.sin_port));
		addcons(new_sock,ip);
	}


} /* }}} */

/* manage_cons() - proces console commands {{{ */
char manage_cons(userrec_t *usr,char *line) {
	const char *sep="\r\n";
	char 	*cmd=NULL,
			*tmp=line;
	int count;


	count=strlen(line);
	strip_telnet(usr,line,&count);
	line[count]=0;

	cmd=strsep(&tmp,sep);

    log_write(FAC_PROTOCOL,PR_INFO,"cmd '%s'",cmd);

	if (!tmp) return 0;
	
	if (!*cmd)
	{
		if (!user_tst_state(usr,STATE_LOGGEDIN|STATE_REGISTERED))
		{
			disttcpf(usr,"\377\374\001\r\n");
			usr->reason=strdup("console: broken registration");
			user_set_state(usr,STATE_QUIT);
			return 1;
		}
		disttcp(usr,"> "); return 0;
	}

	/* parse login */
	if (user_tst_state(usr,STATE_KEY))
	{
        switch(validnick(usr,cmd))
        {
			case NICK_IS_TOO_LONG:
					pubmsg(usr,"ValidateNick: nick is longer than limit of %d characters",NICK_LEN);
					usr->reason=strdup("console: ValidateNick: nick was too long");
					user_set_state(usr,STATE_QUIT);
			return 1;
																																																																																																																				
        	case NICK_IS_ALREADY_USED:
					pubmsg(usr,"ValidateNick: nick '%s' is already used",cmd);
					usr->reason=strdup("console: ValidateNick: nick is already used");
					user_set_state(usr,STATE_QUIT);
			return 1;

			case NICK_IS_RESERVED:
					pubmsg(usr,"ValidateNick: nick '%s' is constantly reserved",cmd);
					usr->reason=strdup("console: ValidateNick: nick is constantly reserved");
					user_set_state(usr,STATE_QUIT);
			return 1;

			case NICK_IS_WITH_INVALID_CHARACTERS:
					pubmsg(usr,"ValidateNick: nick '%s' Contains invalid characters",cmd);
					
					if(!conf.allow_non_us_ascii_nicks) 
						pubmsg(usr,"Nick must be isprint() and !isspace() in US ACII 7-bit");
					
					pubmsg(usr,"- '#' is not allowed as the first character");
					pubmsg(usr,"- '?','$',':','<','>' are also not allowed");
					usr->reason=strdup("console: ValidateNick: nick contains invalid characters");
					user_set_state(usr,STATE_QUIT);
			return 1;

			case NICK_IS_OK:
					my_duplicate(cmd,&usr->nick);
					user_set_state(usr,STATE_PASSWORD);
					disttcp(usr,"password:\377\373\001");
			return 0;
		}
	}

	/* parse password */
	if (user_tst_state(usr,STATE_PASSWORD))
	{
		disttcp(usr,"\377\374\001\r\n");

		if (!validlogin(usr,usr->nick,cmd))
		{
			usr->reason=strdup("console: invalid password");
			user_set_state(usr,STATE_QUIT);
			return 1;
		}

		if (usr->perm && !strchr(usr->perm,'c'))
		{
			usr->reason=strdup("console: not console user");
			user_set_state(usr,STATE_QUIT);
			return 1;
		}

		if (penalty_welcome(usr)) return 1;

		user_set_state(usr,STATE_LOGGEDIN);

		disttcpf(NULL,"$Hello %s|",usr->nick);

		dc_myinfo(NULL,usr);
		disttcp(usr,"\r\nLogged in. Type `help' for a command list.\r\n\r\n> ");


		user_set_state(usr,STATE_REGISTERED);
		return 0;
	}

	strcat(line,"\n");
	
	chat_cmd_exec(usr,line,line);
	disttcp(usr,"> ");
	return 0;
} /* }}} */

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

