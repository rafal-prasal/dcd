/* $Id: net.c,v 1.145 2007/03/01 13:41:23 rav Exp $ */

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

extern userrec_t *user[];
extern hub_t *hub[];

extern int NUSERS;
extern int NHUBS;

extern config_t conf;
extern int maxfd;

extern pthread_mutex_t mutex_myinfo;

int sockerr(char *operation, int error, userrec_t *usr)
{
	int ec=0;

    switch(error)
    {
		case EAGAIN:
			break;
			
		case EBADF:
		case ECONNRESET:
		case EFAULT:
		case ENOTCONN:
		case ENOTSOCK:
		case EPIPE:
		case ENETDOWN:
		case ENETUNREACH:
		case ENOBUFS:
#ifdef ENOSR
		case ENOSR:
#endif
				usr->reason=strdup("problem with socket");
   			    user_set_state(usr,STATE_QUIT);
				/* fall through - log the error and set ec=1 as well */
		default:
		
			log_write(FAC_NETWORK,PR_ERROR,"%s() failed %s@%s : %s",operation,usr->nick?usr->nick:"Unknown",usr->ip,strerror(error));
			ec=1;
			break;
	}

	return ec;
}

void sendtcp(userrec_t *usr, char *msg)
{
	int ret, msglen=strlen(msg);
	
	ret=send(usr->sock, msg, msglen, MSG_DONTWAIT|MSG_NOSIGNAL);

	if (ret==-1) sockerr("send", errno, usr);

	return;
}

void disttcp(userrec_t *usr, char *str)
{
	int i=0;

	if (usr)
	{
		log_write(FAC_PROTOCOL,PR_INFO,"[TCP] SENT %s '%s' ",usr->nick?usr->nick:"Unknown",str);
		sendtcp( usr, str);
		return;
	}

	/* multicasting only to non-console users and registered users */

	log_write(FAC_PROTOCOL,PR_INFO,"[TCP] SENT ALL '%s'",str);

	for ( i = 0; i < NUSERS; i++ )
		if (user[i] && user_tst_state(user[i],STATE_REGISTERED) && user[i]->cons==0) sendtcp(user[i], str);

}

/* disttcpf() - something like printf(), but to a socket {{{ */
void disttcpf(userrec_t * usr, const char * fmt, ... )
{
	va_list args;
	char *str=NULL;

	va_start( args, fmt );
	str=(char *)my_vsprintf(fmt,args);
	va_end( args );

	if (!str)
	{
		log_write(FAC_NETWORK,PR_ERROR,"disttcpf(): unable to create message");
		return;
	}
		
	disttcp(usr,str);

	my_free(str);

} /* }}} */

void disttcp_userip(userrec_t *usr)
{
	int i;
			
	for ( i = 0; i < NUSERS; i++ )
		if (user[i] 
			&& user_tst_state(user[i],STATE_REGISTERED) 
			&& user[i]->cons==0
			&& user_tst_supports(user[i],SUPPORTS_UserIP2)) sendtcp(user[i], usr->userip);		
}

void sendudp(hub_t *_hub, char *msg)
{
	int ret, 
		msglen=strlen(msg);

	size_t addr=sizeof(struct sockaddr_in);
		
	ret=sendto(_hub->sock, msg, msglen, 0, (struct sockaddr *)&_hub->udp ,addr );
				
//	if (ret<0) sockerr(errno,sock,"sendudp");
}

void distudp(hub_t *_hub, char *str)
{
	int i;

	if (_hub)
	{
		log_write(FAC_PROTOCOL,PR_INFO,"[UDP] SENT %s:%s '%s' ",_hub->ip,_hub->port,str);
		sendudp(_hub, str);
		return;
	}

	log_write(FAC_PROTOCOL,PR_INFO,"[UDP] SENT ALL '%s'",str);

	for ( i = 0; i < NHUBS; i++ )
		if (difftime(time(NULL),hub[i]->timeout)<120.0) sendudp(hub[i], str);
}
					
void distudpf(hub_t *_hub, const char *fmt, ...)
{
	va_list args;

	char *str=NULL;
											
	va_start( args, fmt );
	str=(char *)my_vsprintf(fmt,args);
	va_end( args );

	if (!str)
	{
		log_write(FAC_NETWORK,PR_ALERT,"disttcpf(): unable to create message");
		return;
	}
		
	distudp(_hub,str);

	my_free(str);
}

void dc_myinfo(userrec_t *to, userrec_t *usr)
{
    if (!user_tst_state(usr,STATE_REGISTERED)) return;
	    
	pthread_mutex_lock(&mutex_myinfo);

	disttcp(to,usr->myinfo.cache);

	pthread_mutex_unlock(&mutex_myinfo);
}


/* this is required, since after disconnect a socket is switched to a TIME_WAIT
 * state and the listening port remains occupied for several minutes */
/* setsockopts() {{{ */
int setsockopts(int sock, so_opts_t *option)
{

    return 	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,	&option->so_reuseaddr,	sizeof(option->so_reuseaddr))
			|| setsockopt(sock, SOL_SOCKET, SO_DEBUG,		&option->so_debug,		sizeof(option->so_debug))
			||	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,	&option->so_keepalive,	sizeof(option->so_keepalive))
			||	setsockopt(sock, SOL_SOCKET, SO_LINGER,		&option->so_linger,		sizeof(option->so_linger))
			||	setsockopt(sock, SOL_SOCKET, SO_OOBINLINE,	&option->so_oobinline,	sizeof(option->so_oobinline))
			||	setsockopt(sock, SOL_SOCKET, SO_SNDBUF,		&option->so_sndbuf,		sizeof(option->so_sndbuf))
			||	setsockopt(sock, SOL_SOCKET, SO_RCVBUF,		&option->so_rcvbuf,		sizeof(option->so_rcvbuf))
			||	setsockopt(sock, SOL_SOCKET, SO_DONTROUTE,	&option->so_dontroute,	sizeof(option->so_dontroute));
// 			||	setsockopt(sock, SOL_SOCKET, SO_RCVLOWAT,	&option->so_rcvlowat,	sizeof(option->so_rcvlowat))
//			||	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,	&option->so_rcvtimeo,	sizeof(option->so_rcvtimeo))
//			||	setsockopt(sock, SOL_SOCKET, SO_SNDLOWAT,	&option->so_sndlowat,	sizeof(option->so_sndlowat))
//			||	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,	&option->so_sndtimeo,	sizeof(option->so_sndtimeo));

} /* }}} */

/* pubmsg(*to,*fmt,...)
	.description.
		send message as "Hub" to to's public chat with fmt format

	.args.
		userrec_t *to	- IN -	the user record; if to==NULL - send the message to all users
		char *fmt			- IN - "line format"
		char ...			- IN - additional params

	.sample use.
		string="me"
		pubmsg(usr,"It's %s",string); // displays "it's me" to the user
		pubmsg(NULL,"Have no fear"); // displayse "Have no fear" to all users

 {{{ */

void pubmsg( userrec_t *to, const char *fmt, ...)
{
	char *str=NULL;
	va_list args;


	va_start( args, fmt );
	str=(char *)my_vsprintf(fmt,args);
	va_end(args);

	if (to && to->cons) disttcpf(to,"%s\r\n",str);
	else disttcpf(to,"<HUB> %s|",str);

	my_free(str);

}
/* }}} */

/* privmsg(*to,*from,*fmt,...)
	.description.
		send private message as 'from->nick'

	.args.
		userrec_t *to	- IN -	reciever's user record; if to==NULL - sends the priv message to all users (not tested yet)
		userrec_t *from- IN -	sender's user record; if from==NULL - sends the message as <HUB>
		char *fmt			- IN - "line format"
		char ...			- IN - additional params
{{{ */

void privmsg(userrec_t *to, char *from, const char *fmt, ...)
{
	int n;
	char *str=NULL, *frm=NULL;
	va_list args;
	va_start( args, fmt );


	str=(char *)my_vsprintf(fmt,args);

	va_end(args);

	if (!from) my_duplicate("HUB",&frm);
	else my_duplicate(from,&frm);

	if (!to)
	{
		for (n=0;n<NUSERS;n++)
			if (user[n])
			{
				if(!user[n]->cons) disttcpf( user[n],"$To: %s From: %s $<%s> %s|",user[n]->nick,frm,user[n]->nick,str);
				else disttcpf(user[n],"PM %s: %s",frm,str);
			}
	}
	else
		if (!to->cons) disttcpf(to,"$To: %s From: %s $<%s> %s|",to->nick,frm,frm,str);
		else disttcpf(to,"PM %s: %s",frm,str);

	my_free(frm);
	my_free(str);

} /* }}} */

/*
	int my_recv() - receives data from socket {{{
	it is used for getting the message from the socket
	returns
	0	- there is an error
	1	- everything went fine
*/
int my_recv(userrec_t *usr, int max_length, char *message)
{
	int ret=0,
		ec=0;



	message[0]=0;

	ret=recv(usr->sock, message, max_length, MSG_DONTWAIT);

	if (ret==-1 && sockerr("recv", errno, usr)) goto leave;
	
	ec=1;
	message[ret]=0;

	/* update last idle */
	if (user_tst_state(usr,STATE_REGISTERED)) usr->idle=time(NULL);

leave:

	return ec;
}
/* }}}*/

/* disconnect() - disconnect a user with optional reason message {{{ */
void disconnect(userrec_t *usr)
{

	log_write(FAC_ACCESS,PR_INFO,"'%s'@%s: disconnect: %s",(usr->nick)?usr->nick:"Unknown",usr->ip,(usr->reason)?usr->reason:"Connection closed.");
	
	my_free(usr->reason);	
	my_free(usr->ip);
	my_free(usr->ip);
	my_free(usr->password);
	my_free(usr->buf);
	my_free(usr->key);
	my_free(usr->nick);
	my_free(usr->con_ip);
	my_free(usr->ver);

	my_free(usr->perm);
	close(usr->sock);
	my_free(usr);

} /* }}} */

/* strip_telnet() - strip telnet codes - from eggdrop {{{ */
void strip_telnet(userrec_t *usr,char *buf,int *len)
{
	unsigned char *p = (unsigned char *) buf, *o = (unsigned char *) buf;
	int mark;
	int write_result;

	while (*p != 0)
	{
		while ((*p != TLN_IAC) && (*p != 0))
		*o++ = *p++;
		if (*p == TLN_IAC)
		{
			p++;
			mark = 2;
			if (!*p) mark = 1;		/* bogus */
			if ((*p >= TLN_WILL) && (*p <= TLN_DONT))
			{
				mark = 3;
				if (!*(p + 1)) mark = 2;		/* bogus */
			}

			if (*p == TLN_WILL)
			{
				/* WILL X -> response: DONT X */
				/* except WILL ECHO which we just smile and ignore */
				if (*(p + 1) != TLN_ECHO)
				{
					write_result=write(usr->sock, TLN_IAC_C TLN_DONT_C, 2);
					write_result=write(usr->sock, p + 1, 1);
				}
			}

			if (*p == TLN_DO)
			{
				/* DO X -> response: WONT X */
				/* except DO ECHO which we just smile and ignore */
				if (*(p + 1) != TLN_ECHO)
				{
					write_result=write(usr->sock, TLN_IAC_C TLN_WONT_C, 2);
					write_result=write(usr->sock, p + 1, 1);
				}
			}

			if (*p == TLN_AYT)
			{
				/* "are you there?" */
				/* response is: "hell yes!" */
				write_result=write(usr->sock, "\r\nHell, yes!\r\n", 14);
			}

			/* Anything else can probably be ignored */
			p += mark - 1;
			*len = *len - mark;
		}
	}

	*o = *p;

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
