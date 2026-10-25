/* $Id: misc.c,v 1.228 2007/02/19 20:59:18 rav Exp $ */

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
extern config_t conf;
extern int NUSERS;
extern int NHUBS;
extern char *__progname;
extern const char *reserved_nicks[];
extern FILE* file_net;
extern FILE* file_proto;
extern FILE* file_wrng;
extern FILE* file_con;
extern FILE* file_chat;
extern char DAEMON;
extern pthread_mutex_t mutex_inet_ntoa;
extern pthread_mutex_t mutex_myinfo;

extern time_t last_penalties_update;

/* controlled free() {{{ */
void my_free1(void *p)
{
	void **ptr=(void**) p;
	if (ptr && *ptr)
	{
		free(*ptr);
		*ptr=NULL;
	}
} /* }}} */

/* controlled realloc() {{{ */
void *my_realloc(void *ptr,size_t size)
{
	void *tmp=NULL;

	if (size>MAXBLOCK)
	{
		log_write(FAC_DAEMON,PR_ALERT,"my_realloc() failed: MAXBLOCK already reached");
		my_free(ptr);
		return NULL;
	}

	tmp=realloc(ptr,size);

	if (tmp==NULL)
	{
		log_write(FAC_DAEMON,PR_CRITICAL,"my_realloc() failed: %s",strerror(errno));
		exit(0);
	}

	return tmp;
} /* }}} */

/* controlled malloc() {{{ */
void *my_malloc(size_t size)
{
	return my_realloc(NULL,size);
} /* }}} */

char *my_inet_ntoa(struct in_addr addr)
{
	char *buf=NULL;

	pthread_mutex_lock(&mutex_inet_ntoa);
	my_duplicate(inet_ntoa(addr),&buf);
	pthread_mutex_unlock(&mutex_inet_ntoa);

	return buf;
}

void my_duplicate(char *str, char **duplicate)
{
	my_free(*duplicate);
	*duplicate=strdup(str);

	if (!*duplicate) log_write(FAC_DAEMON,PR_ALERT,"my_duplicate() failed: %s",strerror(errno));
}

/* get_progname() - from openssh {{{ */
char *get_progname(char *argv0)
{
#ifdef HAVE___PROGNAME
	extern char *__progname;
	return __progname;
#else
	char *p;
	if (argv0==NULL) return "unknown";

	p=strrchr(argv0,'/');
	if (p==NULL) p=argv0; else p++;
	return p;
#endif
} /* }}} */


/* randomize()	- feed a random numbers generator {{{ */
void randomize( void )
{
	unsigned int seed=0;
	size_t got=0;

	FILE * rdev = fopen( "/dev/urandom", "r" );
	if ( rdev != NULL )
	{
		/* consume a full unsigned int worth of entropy: a single byte
		   left the lock salt far too predictable */
		unsigned char buf[sizeof(seed)];

		got=fread(buf,1,sizeof(buf),rdev);
		fclose( rdev );

		while (got--) seed=(seed<<8)|buf[got];
	}

	seed += getpid() + time( NULL );	/* mix even when urandom failed */
	srand( seed ); /* flawfinder: ignore */
} /* }}} */

/* lockgen() - lock generator: 'salt Pk=privkey' {{{ */
void lock_gen(char *lock)
{
	int i,
		n;
	n=sprintf(lock,"%s","EXTENDEDPROTOCOL"); /* flawfinder: ignore */
	for(i=n;i<SALT_LEN-1;i++) lock[i]=33+rand()%(60);
	lock[SALT_LEN-1]=0;

} /* }}} */


int protocol_char(char c)
{
    return( c == 0 
			|| c == 5 
			|| c == 124 
			|| c == 96 
			|| c == 126 
			|| c == 36);
}
	
void lock_key(userrec_t *usr, char *lock)
{
	int i, 
		offset;

	char tmp[SALT_LEN],
		 *key,
		 save;
				    
	save = 5;
	for(i = 0; i<SALT_LEN-1; i++) 
	{
		tmp[i] = lock[i] ^ save;
		tmp[i] = (((tmp[i] & 0x0F) << 4) | (tmp[i] & 0xF0) >> 4);
		save = lock[i];
	}	
	tmp[0] ^= tmp[i - 1];

	key = my_malloc(10*(SALT_LEN-1)+1);
	offset = 0;
	
	for(i = 0; i<SALT_LEN-1; i++) 
	{
		if(protocol_char(tmp[i])) offset += snprintf(&key[offset],11,"/%%DCN%03i%%/", tmp[i]);
		else key[offset++] = tmp[i];
	}

	key[offset] = 0;
	
	usr->key=key;
}

/* my_vsnprintf() - allocate a sufficiently large string and print into it {{{ */
/* simplified version */
char *my_vsprintf(const char *fmt, va_list ap)
{
	int n=-1, size=8192;
	char *p=NULL;

	if ((p=my_malloc((size_t)size))==NULL) return NULL;

	while (1)
	{
		va_list ap2;

		/* each vsnprintf() call consumes ap: re-arm it per attempt,
		   otherwise the growth loop reads freed/garbage stack args */
		va_copy(ap2, ap);
		n=vsnprintf(p,(size_t)size,fmt,ap2); /* flawfinder: ignore */
		va_end(ap2);

		if (n!=-1 && n<size) break;

		if (n==-1) size*=2; //glibc 2.0
		else size=n+1;		//glibc >2.1

		if ((p=my_realloc(p,(size_t)size))==NULL) return NULL;
	}

	return p;
} /* }}} */


char *my_sprintf(const char *fmt, ...)
{
	char *p=NULL;
	va_list args;

	va_start(args, fmt);
	p=my_vsprintf(fmt,args);
	va_end(args);

	return p;
} /* }}} */

/* nick -> user[] index hash {{{
 *
 * Open addressing with linear probing and backward-shift deletion (no
 * tombstones), mirroring the FNV-1a style of cmd.c. The table is sized
 * to the next power of two >= 2*MAXUSERS, so with at most MAXUSERS
 * entries the load factor stays <= 0.5 and a probe always hits an empty
 * slot. Entries hold the user[] index; userlist_del() re-homes the
 * index of the user it moves into the freed slot.
 *
 * The hash mirrors the user table and is guarded by the same
 * mutex_users; every nick2id()/user_set_nick() caller already holds it
 * (worker loop, UDP handler, console, cleaner).
 */

#define NICK_HASH_BITS 14
#define NICK_HASH_SIZE (1u << NICK_HASH_BITS)

static char *nick_hash_key[NICK_HASH_SIZE];	/* NULL = empty slot */
static int nick_hash_id[NICK_HASH_SIZE];

/* ASCII case-folded FNV-1a; the equality test below still uses
   strcasecmp, so non-ASCII nicks keep their existing (locale)
   semantics - a fold mismatch only costs a probe hop */
static unsigned nick_hash(const char *s)
{
	unsigned h=2166136261u;

	while (*s)
	{
		unsigned char c=(unsigned char)*s++;
		if (c>='A' && c<='Z') c=(unsigned char)(c+('a'-'A'));
		h=(h^c)*16777619u;
	}

	return h;
}

/* insert or update; nick must be a live string (the key aliases it) */
static void nick_hash_add(char *nick, int id)
{
	unsigned i=nick_hash(nick)&(NICK_HASH_SIZE-1);

	while (nick_hash_key[i])
	{
		if (!strcasecmp(nick_hash_key[i],nick))
		{
			nick_hash_id[i]=id;
			return;
		}
		i=(i+1)&(NICK_HASH_SIZE-1);
	}

	nick_hash_key[i]=nick;
	nick_hash_id[i]=id;
}

/* remove by nick; no-op when absent */
static void nick_hash_del(char *nick)
{
	unsigned hole,j,i=nick_hash(nick)&(NICK_HASH_SIZE-1);

	while (nick_hash_key[i])
	{
		if (!strcasecmp(nick_hash_key[i],nick)) break;
		i=(i+1)&(NICK_HASH_SIZE-1);
	}

	if (!nick_hash_key[i]) return;

	nick_hash_key[i]=NULL;
	hole=i;

	/* backward shift: pull every following entry one slot back toward
	   its home slot, keeping every probe chain intact */
	for (i=(i+1)&(NICK_HASH_SIZE-1); nick_hash_key[i]; i=(i+1)&(NICK_HASH_SIZE-1))
	{
		j=nick_hash(nick_hash_key[i])&(NICK_HASH_SIZE-1);

		if ((i<=j && (j<hole || hole<=i)) || (j<hole && hole<=i)) continue;

		nick_hash_key[hole]=nick_hash_key[i];
		nick_hash_id[hole]=nick_hash_id[i];
		nick_hash_key[i]=NULL;
		hole=i;
	}
}

static int nick_hash_find(char *nick)
{
	unsigned i=nick_hash(nick)&(NICK_HASH_SIZE-1);

	while (nick_hash_key[i])
	{
		if (!strcasecmp(nick_hash_key[i],nick)) return nick_hash_id[i];
		i=(i+1)&(NICK_HASH_SIZE-1);
	}

	return -1;
}

/* re-home a user's index after userlist_del() moved it into another slot */
void nick_hash_rehome(char *nick, int id)
{
	if (!nick) return;
	nick_hash_add(nick,id);
}

/* set (or clear) a user's nick, keeping the nick hash in sync; the
   replacement frees the old string, so the old key is dropped first.
   Callers must hold mutex_users. */
void user_set_nick(userrec_t *usr, char *nick)
{
	if (usr->nick) nick_hash_del(usr->nick);

	my_duplicate(nick,&usr->nick);

	if (usr->nick) nick_hash_add(usr->nick,usr->id);
}

/* drop a nick key before its string is freed (disconnect path) */
void nick_hash_forget(userrec_t *usr)
{
	if (usr->nick) nick_hash_del(usr->nick);
}
/* }}} */

/* nick2id() - convert user's nick to table index {{{ */
int nick2id(char *nick)
{
	if (!nick) return -1;

	return nick_hash_find(nick);
} /* }}} */

/* ip2hub() - convert user's nick to table index {{{ */
int ip2hub(char *ip)
{
	int i;
	for(i=0;i<NHUBS;i++)
		if (!strcmp(hub[i]->ip,ip)) return i;

	return -1;
} /* }}} */


/* validnick() - check if a given nick is a valid one 

returns one of NICK_* communicate

{{{ */
char validnick(userrec_t *usr,char *nick)
{
	int n=0;

	if (!nick || !*nick) return NICK_IS_EMPTY;

	/* if nick is not too long */
	if (strlen(nick)+1>NICK_LEN) return NICK_IS_TOO_LONG;
	
	/* if nick contains not valid characters */
	if (strlen(nick)>0 && nick[0]=='#') return NICK_IS_WITH_INVALID_CHARACTERS;
	
	for(n=0;n<(int)strlen(nick);n++)
		if ( (!conf.allow_non_us_ascii_nicks && !isprint(nick[n]))
			 || isspace(nick[n]) 
			 || nick[n]=='$' 
			 || nick[n]==':' 
			 || nick[n]=='<' 
			 || nick[n]=='>' 
			 || nick[n]=='?') return NICK_IS_WITH_INVALID_CHARACTERS;
		

	/* if nick is already in use */
	if (nick2id(nick)!=-1) return NICK_IS_ALREADY_USED;
	
	/* if nick requiers password/ usr->password will contain password to check*/
	if (!usr->cons && validlogin(usr,nick,NULL)) return NICK_REQUIRES_PASSWORD;
	
	/* if nick is not one of constatntly reserved nicks */
	n=0;
	while(reserved_nicks[n][0])
		if (!strcasecmp(nick,reserved_nicks[n++])) return NICK_IS_RESERVED;

	/* if nick matches any of allowed patterns */
	if (!usr->cons && *(conf.conf_nallow) && !nick_allowed(usr,nick)) return NICK_IS_NOT_IN_PATTERNS;

	/* user nick is valid */
	return NICK_IS_OK;
} /* }}} */

/* validkey() - check for a valid $Key sequence - FIXME {{{ */
char validkey(userrec_t *usr, char *key)
{
	int ec=0;

	if (!key
		|| !*key) goto leave;

	ec=!strcmp(usr->key,key);

leave:
	my_free(usr->key);

	return ec;
	
} /* }}} */

int compare_myinfo(myinfo_t *myinfo0, myinfo_t *myinfo1)
{
	int ecode=1;

	pthread_mutex_lock(&mutex_myinfo);

	ecode=strcmp(myinfo1->cache,myinfo0->cache);
			 		
	pthread_mutex_unlock(&mutex_myinfo);		

	return ecode;
}

char *flag_myinfo(myinfo_t *myinfo)
{
	static char *flags[]={
						  "",			//0
						  "Normal",		//1
						  "Away",		//2
						  "Away",		//3
						  "Server; uptime > 2 hours, > 2 GB shared, upload > 200 MB",		//4
						  "Server; uptime > 2 hours, > 2 GB shared, upload > 200 MB",		//5
						  "Server Away; uptime > 2 hours, > 2 GB shared, upload > 200 MB",	//6
						  "Server Away; uptime > 2 hours, > 2 GB shared, upload > 200 MB",	//7
						  "Fireball; upload > 100 kB/s",		//8
						  "Fireball; upload > 100 kB/s",		//9
						  "Fireball Away; upload > 100 kB/s",	//10
						  "Fireball Away; upload > 100 kB/s"	//11
						  };

	unsigned char f=(unsigned char)myinfo->flag;

	if (f>=sizeof(flags)/sizeof(*flags)) return flags[0];

	return flags[f];
}

void move_myinfo(myinfo_t *from, myinfo_t *to)
{
	pthread_mutex_lock(&mutex_myinfo);

//	*to=*from;

	strcpy(to->desc,from->desc); /* flawfinder: ignore */
	strcpy(to->ctype,from->ctype); /* flawfinder: ignore */
	to->flag=from->flag;		
	strcpy(to->email,from->email); /* flawfinder: ignore */
	to->share=from->share;
	strcpy(to->cache,from->cache); /* flawfinder: ignore */

	pthread_mutex_unlock(&mutex_myinfo);
}

/* fill_myinfo() - fill user's record with data received in $MyINFO {{{ */
int fill_myinfo(userrec_t *usr, myinfo_t *myinfo, char *info, char *line)
{
	char *tmp=info,
		 *all,
		 *nick,
		 *desc,
		 *space,
		 *ctype,
		 *email,
		 *share;

	unsigned char flag;
		 			 
	all=strsep(&tmp," ");
	if (!tmp || strcmp(all,"$ALL")) return MyINFO_ALL;

	nick=strsep(&tmp," ");
	if (!tmp || strcmp(nick,usr->nick)) return MyINFO_NICK;

	desc=strsep(&tmp,"$");
	if(!tmp || strlen(desc)>DESC_LEN-1) return MyINFO_DESC;

	space=strsep(&tmp,"$");
	if(!tmp || *space!=' ') return MyINFO_SPACE;

	ctype=strsep(&tmp,"$");
	if(!tmp || strlen(ctype)<1 || strlen(ctype)>CTYPE_LEN-1) return MyINFO_SPEED;

	flag=ctype[strlen(ctype)-1];
	if (flag<1 || flag>11) return MyINFO_FLAG;
	
	ctype[strlen(ctype)-1]='\000';
		
	email=strsep(&tmp,"$");
	if(!tmp || strlen(email)>EMAIL_LEN-1) return MyINFO_EMAIL;

	share=strsep(&tmp,"$");
	if(!tmp || strlen(share)>SHARE_LEN-1) return MyINFO_SHARE;

	/* cache holds the rebuilt full line: anything longer than the
	   struct's cache (e.g. trailing garbage after the last '$') would
	   overflow myinfo_t */
	if (strlen(line)>=sizeof(myinfo->cache)) return MyINFO_LINE;

	strcpy(myinfo->desc,desc); /* flawfinder: ignore */
	strcpy(myinfo->email,email); /* flawfinder: ignore */
	strcpy(myinfo->ctype,ctype); /* flawfinder: ignore */
	myinfo->flag=flag;
	sscanf(share,"%"SCNu64,&(myinfo->share)); /* flawfinder: ignore */
	strcpy(myinfo->cache,line); /* flawfinder: ignore */
	return MyINFO_OK;
} /* }}} */

/* check_timeout() - check clients' times {{{ */
void check_timeout(userrec_t *usr) {
	time_t tv;

	if ((!conf.idle_timeout && !conf.register_timeout)) return;
	time(&tv);

	if (conf.idle_timeout&&(usr->cons==0) && tv-usr->idle>conf.idle_timeout)
	{
		usr->reason=strdup("idle timeout");		
		user_set_state(usr, STATE_QUIT);
				
		return;
	}

	if (conf.register_timeout && tv-usr->idle>conf.register_timeout)
	{
		if (conf.minslots && user_tst_state(usr,STATE_SR))
		{
			pubmsg(usr,"Your client did not respond to slots checking");
			pubmsg(usr,"Sorry, You must leave");
			pubmsg(usr,"dcgui-qt/dctc - Please share a file of name 'fakesearch.txt' with more than 0 bytes inside!");
			pubmsg(usr,"It also could be a connection problem");
			
			usr->reason=strdup("slot checking timout");
			user_set_state(usr, STATE_QUIT);
		}

		if (!user_tst_state(usr,STATE_REGISTERED|STATE_QUIT))
		{
			pubmsg(usr,"REGISTRATION TIMEOUT");
			usr->reason=strdup("registration timeout");
			user_set_state(usr, STATE_QUIT);
		}
	}

} /* }}} */


/* make_path() - compose direct path to file {{{ */
void make_path(char *dir,char **file)
{
	char *tmp=NULL,*p,*newp;

	if (!*file || !**file) return;

	my_duplicate(*file,&tmp);

	if (!tmp) return;

	p=strrchr(tmp,'/');
	if (p) p++; else p=tmp;
	newp=(char *)my_realloc(*file,strlen(dir)+strlen(p)+2);
	if (!newp)
	{
		*file=NULL; /* my_realloc() has already freed it */
		my_free(tmp);
		return;
	}
	*file=newp;
	sprintf(*file,"%s%s%s",dir,(dir[strlen(dir)-1]=='/')?"":"/",p); /* flawfinder: ignore */
	my_free(tmp);
} /* }}} */

/* make_paths() - compose direct paths to files for further use {{{ */
void make_paths(config_t *conf)
{
	/* path to chat log file */
//	make_path(conf->log_dir,&(conf->log_chat));

	/* path to welcome message file */
	make_path(CONFIGDIR,&(conf->conf_welcome));

	/* path to motd file */
	make_path(CONFIGDIR,&(conf->conf_motd));

	/* path to bans file */
	make_path(CONFIGDIR,&(conf->conf_banned));

	/* path to dcd users file */
	make_path(CONFIGDIR,&(conf->conf_cusers));

	/* path to console host access file */
	make_path(CONFIGDIR,&(conf->conf_callow));

	/* path to allowed nicks file */
	make_path(CONFIGDIR,&(conf->conf_nallow));

	/* path to penalties file */
	make_path(CONFIGDIR,&(conf->conf_penalties));

	/* path to rules file */
	make_path(CONFIGDIR,&(conf->conf_rules));
	
	make_path(CONFIGDIR,&(conf->conf_hublinks));
	
	make_path(CONFIGDIR,&(conf->conf_usercommands));

} /* }}} */

/* uptime() - return string representing uptime {{{ */
void uptime(time_t starttime, char *str)
{
	time_t up;
	long d;
	int h,m,s;

	time(&up);
	up-=starttime;
	if (up<0) up=0;
	d=up/(3600*24);
	up%=3600*24;
	h=up/3600;
	up%=3600;
	m=up/60;
	s=up%60;
	if (d) snprintf(str,32,"%ld day%s, %02d:%02d:%02d",d,(d==1)?"":"s",h,m,s);
	else snprintf(str,32,"%02d:%02d:%02d",h,m,s);
} /* }}} */

/* toggle_flag() - toggle configuration flag {{{ */
inline void toggle_flag(config_t *conf,userrec_t *usr,unsigned char flag)
{
	if (conf->flags&flag) conf->flags&=~flag; else conf->flags|=flag;
	disttcpf(usr,"Option has been turned %s.\r\n",(conf->flags&flag)?"on":"off");
} /* }}} */
int add2buf(char **buf, char *string)
{
	char *tmp;
	int blen=0,len;

	if (!string || !*string) return 0;

	if (*buf) blen=strlen(*buf);

	len=blen+1+strlen(string);

	/* grow in place when possible: avoids a full copy of *buf on every append */
	tmp=(char *)my_realloc(*buf,len);

	if(!tmp)
	{
		*buf=NULL; /* my_realloc() has already freed it */
		return 0;
	}

	memcpy(tmp+blen,string,strlen(string)+1);

	*buf=tmp;

	return len;
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

