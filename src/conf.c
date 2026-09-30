/* $Id: conf.c,v 1.168 2007/02/19 20:59:18 rav Exp $ */

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

extern config_t conf;

extern int NPENAL; 
extern int NLOGINS;
extern int NPATTERNS;
extern int NHUBS;

extern Tpenalty *penalties[MAXPENALTIES];
extern loginrec_t *login[MAXUSERS];
extern char *pattern[MAXUSERS];
extern hub_t *hub[MAXHUBS];

/* strdupdelim() - copy the string formed by two pointers to a new location {{{ */
char *strdupdelim(const char *beg,const char *end) {
	char *res=(char *)my_malloc(end-beg+1);

	memcpy(res,beg,end-beg);
	res[end-beg]=0;
	return res;
} /* }}} */

/* read_whole_line() - read a line from FP and return the pointer to allocated storage. {{{ */
char *read_whole_line(FILE *fp, int line_style)
{
	int length=0;
	int bufsize=82;
	char *line=(char *)my_malloc(bufsize);

	while(fgets(line+length,bufsize-length,fp))
	{
		length+=strlen(line+length);

		if (length==0) continue;
		if (line_style==LINE_NORMAL && line[length-1]=='\n') break;		
		if (line_style==LINE_PUNISHMENT && length>=2 && line[length-2]=='\004' && line[length-1]=='\n') break;
		bufsize<<=1;

		line=(char *)my_realloc(line,bufsize);
	}

	if (length==0||ferror(fp))
	{
		my_free (line);
		return NULL;
	}

	if (length+1<bufsize) line=(char *)my_realloc(line,length+1);
	return line;
} /* }}} */

/* parse_config() - parse config file {{{ */
char parse_config(int start, config_t *conf)
{
	char *line,tmp[PATH_MAX];
	FILE *fp;

	/* opening config {{{ */
	/* we look for config in location taken from configure or from command-line option */
	if (!realpath(conf->conf_main,tmp))
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't find config file (%s): %s.",conf->conf_main, strerror(errno));
		return 1;
	}

	my_duplicate(tmp,&conf->conf_main);

	fp=fopen(conf->conf_main,"rb");

	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open config file (%s): %s.",conf->conf_main, strerror(errno));
		return 1;
	}
	/* }}} */

	while((line=read_whole_line(fp,LINE_NORMAL)))
	{
		conf_cmd_exec(start,line);
		my_free(line);
	}
	
	my_free(line);
	fclose(fp);
	return 0;
} /* }}} */


/* parse_raw() - parses rawfile {{{ */
void parse_messages(int raw, char **result, char *msgfile)
{
	char *line,
		 *tmp;

	FILE *fp;

	my_free(*result);

	fp=fopen(msgfile,"rb");

	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open message file (%s): %s.",msgfile,strerror(errno));
		return;
	}

	while((line=read_whole_line(fp,LINE_NORMAL)))
	{		
		tmp=strpbrk(line,"\r\n");
		if (tmp) *tmp='\000';

		add2buf(result,line);

		if (raw && tmp) add2buf(result,"\r\n");
		
	    my_free(line);
	}

	fclose(fp);
} /* }}} */

void free_login(loginrec_t *ulogin)
{
	my_free(ulogin->perm);
	my_free(ulogin->nick);
	my_free(ulogin->email);
	my_free(ulogin->desc);
	my_free(ulogin->password);
}

void logins_free()
{
	int i;
	
	for(i=0; i<NLOGINS; i++)
	{
		free_login(login[i]);
		my_free(login[i]);
	}
	
	NLOGINS=0;
}

int logincmp(const void *l1, const void *l2)
{
	loginrec_t	*u1 = *(loginrec_t **)l1,
				*u2 = *(loginrec_t **)l2;
		
	return strcasecmp(u2->nick, u1->nick);
}

int nicklogincmp(const void *_nick, const void *_u)
{
	loginrec_t *u = *(loginrec_t **)_u;
	char *nick= (char *)_nick;
		
	return strcasecmp(u->nick,nick);	
}

loginrec_t *nick2login(char *nick)
{	
	loginrec_t **_result, *result;
	
	_result= (loginrec_t **)bsearch((void*) nick, (void *) login, NLOGINS, sizeof(loginrec_t *), nicklogincmp);

	if (!_result) result=NULL;
	else result = *_result;

	return result;
}

void parse_logins()
{
	FILE *fp;

	char *tmp, 
		 *p,
		 *line;
	
	loginrec_t *ulogin;
	
	logins_free();

	fp=fopen(conf.conf_cusers,"rb");

	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open users file (%s): %s.",conf.conf_cusers,strerror(errno));
		return;
	}

	while(NLOGINS<MAXUSERS && (line=read_whole_line(fp,LINE_NORMAL)))
	{
		p = line;
		while(*p && isspace(*p)) ++p;

		if (!*p || *p=='#' )
		{
			my_free(line);
			continue;
		}

		// login
		tmp=strpbrk(p,":\r\n");
		if (!tmp) goto next;

		*(tmp++)=0;

		ulogin=my_malloc(sizeof(loginrec_t));
	
		if (!ulogin) goto leave;

		memset(ulogin,0,sizeof(loginrec_t));				
		ulogin->nick=strdup(p);

		// passwd
		p = tmp;
		tmp=strpbrk(p,":\r\n");
		if (!tmp) goto add;
		*(tmp++)=0;

		ulogin->password=strdup(p);

		// e-mail
		p = tmp;
		tmp=strpbrk(p,":\r\n");
		if (!tmp) goto add;
		*(tmp++)=0;

		ulogin->email=strdup(p);

		// desc
		p = tmp;
		tmp=strpbrk(p,":\r\n");
		if (!tmp) goto add;
		*(tmp++)=0;
		
		ulogin->desc=strdup(p);

		// perm
		p = tmp;
		tmp=strpbrk(p,":\r\n");
		if (!tmp) goto add;
		*(tmp++)=0;

		ulogin->perm=strdup(p);

	add:
		login[NLOGINS]=ulogin;
		NLOGINS++;

	next:		
		my_free(line);
	}

leave:
	fclose(fp);
	my_free(line);

	if (NLOGINS) qsort((void *)login,NLOGINS,sizeof(loginrec_t *),logincmp);

}

/* validlogin() - verify login and password {{{ */
int validlogin(userrec_t *usr,char *nick, char *pass)
{
	int EC = 0;
	loginrec_t *ulogin;
	
	ulogin=nick2login(nick);

	my_duplicate("n",&usr->perm);
	
	if (ulogin)
	{		
		if (usr->cons) 
		{
			if (!password_match(ulogin->password, pass)) goto leave;
			
			snprintf(usr->myinfo.email,EMAIL_LEN-1,"%s",ulogin->email);
			snprintf(usr->myinfo.desc,DESC_LEN-1,"%s",ulogin->desc);

		}
		else if (ulogin->password) my_duplicate(ulogin->password,	&usr->password);

		if (ulogin->perm) my_duplicate(ulogin->perm,	&usr->perm);
		EC = 1;
	}

leave:

	return EC;
} /* }}} */

/* validhost(usr, access_file, deeper){{{
.description:
	verifies host access

.args
	userrec_t *usr	userrec
	char *access_file	file name
	int deeper		if !=0 then use include: lines in the acces_file

.return values:
	0:	access file does not exists,
	host is not listed in the file

	1:	host is listed in the file
*/

char validhost(userrec_t *usr,char *access_file, int deeper)
{
	const char *sep=":\r\n";

	char *line=NULL,*p=NULL;
	char *fpath=NULL;
	FILE *fp;
	char *__strtok_temp__;

	/* open the access_file */
	fp=fopen(access_file,"rb");

	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open allow/deny file (%s): %s.",access_file,strerror(errno));
		return 0;
	}

	/* parse the line */
	while((line=read_whole_line(fp,LINE_NORMAL))) /* reads the lines */
	{
		p=line; /* p contains the line */

		while(*p&&isspace(*p)) ++p;
		if (!*p||*p=='#')
		{
			my_free(line);
			continue;
		} /* skip empty and # lines; goes to the next line*/

		p=strtok_r(p,sep,&__strtok_temp__); /* read the first token of the line*/

		if (!p)
		{
			my_free(line);
			continue;
		}

		/* check for include: */
		if (deeper && !strcasecmp(p,"include"))	/* deeper!=0 */
		{
			/* parse the include:>filename< */
			p=strtok_r(NULL,sep,&__strtok_temp__); /* get the included file name */

			if (p)
			{
				my_duplicate(p,&fpath);

				make_path(CONFIGDIR,&(fpath));

				if (strcasecmp(fpath,access_file))/* file name should != NULL neither access_file */
				{
					if (validhost(usr,fpath,0))
					{
						my_free(fpath);
						goto leave;
					}//valid
				}else log_write(FAC_DAEMON,PR_WARNING,"Including the same file is not allowed");

				my_free(fpath);

			} else log_write(FAC_DAEMON,PR_WARNING,"Including the same file is not allowed");

		} else/* if ip mathes one of the listed in file ips */
			if (match(p,usr->ip))
			{
				p=strtok_r(NULL,sep,&__strtok_temp__);	 /*getting reason of ban */

				if(!usr->cons)	pubmsg(usr,"%s",p?p:"Your host is permanently banned.");

				goto leave;
			}

		my_free(line);

	}//while

	my_free(line);
	fclose(fp);

	return 0;

leave:

	my_free(line);
	fclose(fp);

	return 1;

} /* }}} */


void free_nick_patterns()
{
	int i;
	
	for(i=0; i<NPATTERNS; i++) my_free(pattern[i]);	
	NPATTERNS=0;
}

int nick_allowed(userrec_t *usr, char *nick)
{
	int i;
	
	for(i=0; i<NPATTERNS; i++)
		if (match(pattern[i],nick)) return 1;
	
	if (!NPATTERNS) return 1;
	
	return 0;
}

/* parse_nick_patterns() - verify nick access {{{ */
void parse_nick_patterns()
{
	const char *sep=" \t\r\n";
	char *line=NULL,*p=NULL;
	FILE *fp;
	char *__strtok_temp__;

	free_nick_patterns();

	fp=fopen(conf.conf_nallow,"rb");

	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open nicks allow file (%s): %s.",conf.conf_nallow,strerror(errno));
		return;
	}

	while(NPATTERNS<MAXUSERS && (line=read_whole_line(fp,LINE_NORMAL)))
	{
		p=line;

		while(*p&&isspace(*p)) ++p;

		if (!*p||*p=='#')
		{
			my_free(line);
			continue;
		}

		p=strtok_r(p,sep,&__strtok_temp__);
		if (!p) goto leave;

		pattern[NPATTERNS++]=strdup(p);

		my_free(line);
	}

leave:
	my_free(line);
	fclose(fp);
} /* }}} */

/* penalties_read() - read penalties list from file to memory {{{ */
void penalties_read()
{
	const char *sep=":\r\n";
	char *line=NULL,
		 *p=NULL,
		 *s_ip=NULL, 
		 *s_penalties=NULL, 
		 *s_start_date=NULL,
		 *s_op=NULL,
		 *s_nick=NULL,
		 *s_duration=NULL,
		 *s_reason=NULL,
		 *__strtok_temp__=NULL;

	FILE *fp=NULL;
	
	long int i=-1, 
			 i_penalties,
			 i_duration,
			 i_start_date;

	fp=fopen(conf.conf_penalties,"rb");

	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open file (%s): %s.",conf.conf_penalties,strerror(errno));
		return;
	}

	while((line=read_whole_line(fp,LINE_PUNISHMENT)))
	{
		p=line;
		i++;

		while(*p && isspace(*p)) ++p;

		if (!*p || *p=='#')
		{
			my_free(line);
			continue;
		}

		s_ip=strtok_r(line,sep,&__strtok_temp__);
		s_nick=strtok_r(NULL,sep,&__strtok_temp__);
		s_op=strtok_r(NULL,sep,&__strtok_temp__);
		s_penalties=strtok_r(NULL,sep,&__strtok_temp__);
		s_start_date=strtok_r(NULL,sep,&__strtok_temp__);
		s_duration=strtok_r(NULL,sep,&__strtok_temp__);
		s_reason=strtok_r(NULL,"\004",&__strtok_temp__);

		if (!s_ip 
			|| !s_penalties 
			|| !s_duration 
			|| !s_reason 
			|| !sscanf(s_start_date,"%ld",&i_start_date) 
			|| !sscanf(s_duration,"%ld",&i_duration) 
			|| !sscanf(s_penalties,"%ld",&i_penalties))
		{
			log_write(FAC_DAEMON,PR_WARNING,"%s(%d): Syntax Error",conf.conf_penalties,i);
			my_free(line);
			continue;
		}

		user_set_penalty(s_ip ,s_nick ,s_op ,i_penalties ,i_start_date ,i_duration ,s_reason ,1);

		my_free(line);
	}

	my_free(line);
	fclose(fp);

} /* }}} */

/* penalties_write() - write penalties list from memory to file {{{ */
void penalties_write()
{
	FILE *fp;
	long i,duration;

	fp=fopen(conf.conf_penalties,"w+");	//clearing the file while opening

	if (!fp)
	{

		log_write(FAC_DAEMON,PR_ERROR,"Can't write penalties file (%s): %s.",conf.conf_penalties,strerror(errno));
		return;
	}

	for(i=0;i<NPENAL;i++)
	{
		if(!penalties[i]) continue;
		
		if ((duration=user_exp_penalty(penalties[i]))>0) 
		{
			fprintf(fp,"%s:%s:%s:%d:%ld:%ld:%s\004\n",penalties[i]->ip,
										penalties[i]->nick,
										penalties[i]->op,
										penalties[i]->penalty,
										penalties[i]->start_date,
										penalties[i]->end_date-penalties[i]->start_date,
										penalties[i]->reason);
			continue;
		}

		delpenalty(i);
		i--;
	}

	fclose(fp);

	return;
} /* }}} */


void free_hub(hub_t *_hub)
{
	my_free(_hub->ip);
	my_free(_hub->port);
	my_free(_hub->pass_in);
	my_free(_hub->pass_out);
	my_free(_hub->interface);

	my_free(_hub->Up_cache);
	my_free(_hub->UpToo_cache);

	close(_hub->sock);		
}


void free_hubs()
{
	int i;
	for (i=0; i<NHUBS; i++)
	{
		free_hub(hub[i]);	
	}
	NHUBS=0;
}

void parse_hublist()
{
	FILE *fp=NULL;

	char *p, 
		 *line,
		 *ip,
		 *port,
		 *pass_in,
		 *pass_out;

	hub_t *hub_tmp;

	int ret;

	free_hubs();

	fp=fopen(conf.conf_hublinks,"rb");
	
	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open file (%s): %s.",conf.conf_hublinks,strerror(errno));
		return;
	}
	
	while(NHUBS<MAXHUBS && (line=read_whole_line(fp,LINE_NORMAL)))	
	{

		p = line;
		while(*p && isspace(*p)) ++p;

		if (!*p || *p=='#' )
		{
			my_free(line);
			continue;
		}
		
		ip=strsep(&p,":");
		port=strsep(&p,":");
		pass_in=strsep(&p,":");
		pass_out=strsep(&p,"\r\n");
		
		if (!p)
		{
			my_free(line);
			continue;
		}
		
		hub_tmp=(hub_t *)my_malloc(sizeof(hub_t));
		
		hub[NHUBS]=hub_tmp;
		
		hub[NHUBS]->ip=strdup(ip);
		hub[NHUBS]->port=strdup(port);
		hub[NHUBS]->pass_in=strdup(pass_in);
		hub[NHUBS]->pass_out=strdup(pass_out);
		hub[NHUBS]->timeout=time(NULL)-120;
		hub[NHUBS]->timeout_retry=time(NULL)-120;

		hub[NHUBS]->Up_cache=my_sprintf("$Up %s %s:%d|", pass_out, conf.listen_interface, conf.listen_port_main);
		hub[NHUBS]->UpToo_cache=my_sprintf("$UpToo %s %s:%d|", pass_out, conf.listen_interface, conf.listen_port_main);

		if (inet_pton(AF_INET,ip,&hub[NHUBS]->udp.sin_addr)!=1)
		{
			log_write(FAC_DAEMON,PR_WARNING,"Invalid hub address '%s' in %s.",ip,conf.conf_hublinks);
			hub[NHUBS]->udp.sin_addr.s_addr=INADDR_NONE;
		}
		hub[NHUBS]->udp.sin_port        = htons(atoi(port));
		hub[NHUBS]->udp.sin_family      = AF_INET;	
		
		ret  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		
		hub[NHUBS]->sock =ret;									

		NHUBS++;						

		my_free(line);
	}	

	fclose(fp);

}


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
