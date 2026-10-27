/* $Id$ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * Embedded HTTP endpoint (listen_port_http):
 *
 *   GET /            - web chat page (default endpoint)
 *   GET /console     - web console page
 *   GET /healthcheck - 200 OK when the hub core is responsive, 500 otherwise
 *   GET /ws          - WebSocket upgrade bridged to the local console port
 *   GET /chatws      - WebSocket upgrade to the embedded web chat protocol
 *
 * The HTML pages are configuration files in the config directory:
 * dcd.webchat / dcd.webconsole, each self-contained with inlined CSS/JS.
 * They are re-read when their mtime changes; when a file is missing the
 * hub serves its built-in default copy (web_assets.h).
 *
 * The web console reuses the telnet console protocol end-to-end: each
 * WebSocket client gets a loopback TCP connection to listen_port_cons and
 * the raw protocol bytes are relayed between the WebSocket frames and the
 * console socket. Telnet IAC option triplets emitted by the console are
 * stripped before delivery.
 *
 * The web chat is a JSON protocol speaking directly to the hub core (see
 * the chat section below): login against dcd.users, public chat fed by a
 * disttcp() multicast tap, private messages via dc_To()/direct $To:, and a
 * presence poller that snapshots the user table under mutex_users.
 *
 * One single thread (this one) owns all web client state; hub worker
 * threads only append chat events to the per-client output queues under
 * mutex_out and read the chat registry under mutex_webchat. The lock
 * order is always mutex_users -> mutex_webchat -> mutex_out.
 * ----------------------------------------------------------------
 * }}} */

#include "pch.h"

#include <openssl/evp.h>
#include <sys/stat.h>
#include <limits.h>

#include "web_assets.h"

extern config_t conf;
extern int listen_http;
extern pthread_mutex_t mutex_users;
extern userrec_t *user[MAXUSERS];
extern int NUSERS;

#define WEB_MAX_CLIENTS 12
#define WEB_REQ_MAX 8192
#define WEB_RBUF_MAX 8192
#define WEB_OUT_MAX (128*1024)
#define WEB_CONSQ_MAX 4096
#define WEB_FRAME_MAX (64*1024)
#define WEB_POLL_MS 1000

/* chat: JSON frame sizes, registry and presence snapshot caps */
#define WEB_CHAT_MAX WEB_MAX_CLIENTS
#define WEB_ULIST_MAX 512
#define WEB_CHAT_TXT_MAX 1024		/* message text cap (bytes) */
#define WEB_JSON_MAX 8192			/* chat JSON event cap */
#define WEB_CHAT_RATE 4			/* messages per second per session */
#define WEB_CHAT_LOGIN_MAX 5		/* login attempts per session */

#define ST_REQ 0
#define ST_WS 1

static const char WS_GUID[]="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

typedef struct
{
	int used;
	int state;
	int cfd;
	int consfd;

	char req[WEB_REQ_MAX];
	size_t reqn;

	unsigned char rbuf[WEB_RBUF_MAX];
	size_t rbufn;

	unsigned char out[WEB_OUT_MAX];
	size_t outn;

	char consq[WEB_CONSQ_MAX];
	size_t consqn;

	/* chat client state (owned by the web thread) */
	int chatmode;			/* speaks the chat JSON protocol */
	int clogged;			/* chat login completed */
	char cnick[NICK_LEN+2];
	char cperm[32];
	int login_attempts;
	int rl_msgs;			/* rate limit: msgs in the rl_win second */
	time_t rl_win;

	int closing;			/* web-thread only teardown request */
	int obuffull;			/* out queue overflow (set atomically by
					   web_enqueue from worker threads) */
} webc_t;

static webc_t webc[WEB_MAX_CLIENTS];

/* guards every c->out/c->outn queue: the web thread flushes them while hub
   workers append chat events (web_chat_broadcast/web_chat_deliver_pm) */
static pthread_mutex_t mutex_out=PTHREAD_MUTEX_INITIALIZER;

/* --- page files ---------------------------------------------------------
 * dcd.webchat / dcd.webconsole in the config directory (self-contained
 * HTML with inlined CSS/JS), live re-read on mtime change; the built-in
 * copies composed from web_assets.h are the fallback when a file is
 * missing. Only the web thread touches these. */

#define WEB_PAGE_MAX 262144

typedef struct
{
	char path[PATH_MAX];
	char *buf;			/* file copy; NULL = serve built-in */
	size_t len;
	time_t mtime;
	int miss_logged;
} webpage_t;

static webpage_t pg_chat,pg_cons;
static char *fb_chat=NULL,*fb_cons=NULL;	/* built-in fallback copies */

static void web_page_path(webpage_t *pg,const char *name)
{
	char *sl=conf.conf_main?strrchr(conf.conf_main,'/'):NULL;

	if (sl)
		snprintf(pg->path,sizeof(pg->path),"%.*s/%s",
			(int)(sl-conf.conf_main),conf.conf_main,name);
	else
		snprintf(pg->path,sizeof(pg->path),"%s",name);
}

static void web_page_check(webpage_t *pg)
{
	struct stat st;
	FILE *fp;
	char *nb;
	size_t got;

	if (stat(pg->path,&st)!=0)
	{
		if (!pg->miss_logged && !pg->buf)
		{
			log_write(FAC_DAEMON,PR_INFO,"Web page file %s not present, serving built-in copy.",pg->path);
			pg->miss_logged=1;
		}
		return;
	}

	if (pg->buf && st.st_mtime==pg->mtime) return;

	if (st.st_size<1 || st.st_size>WEB_PAGE_MAX)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Web page %s has invalid size %ld, keeping current copy.",pg->path,(long)st.st_size);
		pg->mtime=st.st_mtime;
		return;
	}

	fp=fopen(pg->path,"rb");
	if (!fp)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Can't open web page file %s: %s.",pg->path,strerror(errno));
		return;
	}

	nb=(char *)my_malloc((size_t)st.st_size+1);
	got=fread(nb,1,(size_t)st.st_size,fp);
	fclose(fp);

	if (got!=(size_t)st.st_size)
	{
		log_write(FAC_DAEMON,PR_ERROR,"Short read of web page file %s.",pg->path);
		my_free(nb);
		pg->mtime=st.st_mtime;
		return;
	}

	nb[got]=0;

	if (pg->buf) my_free(pg->buf);
	pg->buf=nb;
	pg->len=got+1;
	pg->mtime=st.st_mtime;
	log_write(FAC_DAEMON,PR_INFO,"Web page loaded from %s (%ld bytes).",pg->path,(long)got);
}

static const char *web_page_body(webpage_t *pg,const char *fallback)
{
	return pg->buf ? pg->buf : fallback;
}

/* health probe: the user-table mutex is the single lock every hub worker
   holds; workers may hold it across a poll cycle, so wait up to 2s.
   Timing out on a lock that every worker releases periodically means the
   core is deadlocked: report unhealthy */
int web_health_ok(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME,&ts);
	ts.tv_sec+=2;

	if (pthread_mutex_timedlock(&mutex_users,&ts)==0)
	{
		pthread_mutex_unlock(&mutex_users);
		return 1;
	}

	return 0;
}

/* base64 over short fixed inputs (SHA1 digest) */
static void b64_encode(const unsigned char *in,int inlen,char *out)
{
	static const char tbl[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int i,o=0;

	for (i=0;i<inlen;i+=3)
	{
		unsigned v=(unsigned)in[i]<<16;
		int rem=inlen-i;

		if (rem>1) v|=(unsigned)in[i+1]<<8;
		if (rem>2) v|=(unsigned)in[i+2];

		out[o++]=tbl[(v>>18)&0x3f];
		out[o++]=tbl[(v>>12)&0x3f];
		out[o++]=rem>1?tbl[(v>>6)&0x3f]:'=';
		out[o++]=rem>2?tbl[v&0x3f]:'=';
	}

	out[o]=0;
}

/* RFC6455 frame codec (see web.h) */
int ws_parse_frame(const unsigned char *src,size_t n,struct ws_frame *f)
{
	size_t i=2;

	if (n<2) return 0;

	if (src[0]&0x70) return -1;	/* rbits must be zero */

	f->fin=(src[0]>>7)&1;
	f->opcode=src[0]&0x0f;
	f->masked=(src[1]>>7)&1;
	f->len=src[1]&0x7f;

	/* text, close, ping, pong only; no fragmentation */
	if (f->opcode!=0x1 && f->opcode!=0x8 && f->opcode!=0x9 && f->opcode!=0xA)
		return -1;

	if (f->len==126)
	{
		if (n<4) return 0;
		f->len=((unsigned long long)src[2]<<8)|src[3];
		i=4;
	}
	else if (f->len==127)
	{
		int k;

		if (n<10) return 0;

		f->len=0;
		for (k=0;k<8;k++) f->len=(f->len<<8)|src[2+k];
		i=10;

		if (f->len>>32) return -1;	/* absurd length */
	}

	if (f->masked)
	{
		if (n<i+4) return 0;
		memcpy(f->mask,src+i,4);
		i+=4;
	}

	f->hdrlen=i;

	return 1;
}

size_t ws_write_frame(unsigned char *dst,size_t dstsz,int opcode,
	const unsigned char *payload,size_t len)
{
	size_t hdr;

	if (len<126) hdr=2;
	else if (len<=0xFFFF) hdr=4;
	else hdr=10;

	if (dstsz<hdr+len) return 0;

	dst[0]=(unsigned char)(0x80|(opcode&0x0f));

	if (len<126) dst[1]=(unsigned char)len;
	else if (len<=0xFFFF)
	{
		dst[1]=126;
		dst[2]=(unsigned char)(len>>8);
		dst[3]=(unsigned char)len;
	}
	else
	{
		int k;

		dst[1]=127;
		for (k=0;k<8;k++) dst[2+k]=(unsigned char)(len>>(56-8*k));
	}

	if (payload && len) memcpy(dst+hdr,payload,len);

	return hdr+len;
}

/* drop telnet IAC option triplets (FF FB/FC/FD xx) the console emits */
static size_t strip_iac(const unsigned char *in,size_t n,unsigned char *out)
{
	size_t i=0,o=0;

	while (i<n)
	{
		if (in[i]==0xFF && i+2<n &&
			(in[i+1]==0xFB || in[i+1]==0xFC || in[i+1]==0xFD))
			i+=3;
		else
			out[o++]=in[i++];
	}

	return o;
}

static void web_enqueue(webc_t *c,const void *data,size_t len)
{
	pthread_mutex_lock(&mutex_out);

	if (c->outn+len>WEB_OUT_MAX)
		__atomic_store_n(&c->obuffull,1,__ATOMIC_RELAXED);	/* dead client */
	else
	{
		memcpy(c->out+c->outn,data,len);
		c->outn+=len;
	}

	pthread_mutex_unlock(&mutex_out);
}

static void web_http_response(webc_t *c,int code,const char *status,
	const char *type,const char *body,size_t blen)
{
	char hdr[256];
	int n;

	n=snprintf(hdr,sizeof(hdr),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Cache-Control: no-store\r\n"
		"Connection: close\r\n\r\n",
		code,status,type,blen);

	if (n>0) web_enqueue(c,hdr,(size_t)n);
	if (body && blen) web_enqueue(c,body,blen);

	c->state=ST_WS;		/* flush queue then close (closing set by caller
				   unless the connection continues) */
}

static void web_send_static(webc_t *c,int code,const char *status,
	const char *type,const char *body)
{
	c->closing=1;
	web_http_response(c,code,status,type,body,strlen(body));
}

static void web_send_health(webc_t *c)
{
	char body[128];
	int n;

	if (web_health_ok())
	{
		n=snprintf(body,sizeof(body),
			"OK online=%lu max_online=%lu logins=%lu\n",
			stats_online(),stats_max_online(),stats_total_logins());
		c->closing=1;
		web_http_response(c,200,"OK","text/plain; charset=utf-8",body,(size_t)n);
	}
	else
	{
		c->closing=1;
		web_http_response(c,500,"Internal Server Error","text/plain; charset=utf-8",
			"HUB UNHEALTHY: user table locked\n",34);
	}
}

static int web_console_connect(void)
{
	int fd;
	struct sockaddr_in sa;
	struct in_addr iface;

	if (!conf.listen_port_cons) return -1;

	fd=socket(AF_INET,SOCK_STREAM,0);
	if (fd==-1) return -1;

	memset(&sa,0,sizeof(sa));
	sa.sin_family=AF_INET;
	sa.sin_port=htons(conf.listen_port_cons);

	if (inet_aton(conf.listen_interface,&iface) && iface.s_addr!=INADDR_ANY)
		sa.sin_addr=iface;
	else
		sa.sin_addr.s_addr=htonl(INADDR_LOOPBACK);

	if (connect(fd,(struct sockaddr *)&sa,sizeof(sa)))
	{
		close(fd);
		return -1;
	}

	if (fcntl(fd,F_SETFL,O_NONBLOCK)<0)
	{
		close(fd);
		return -1;
	}

	return fd;
}

static void web_websocket(webc_t *c,const char *wskey)
{
	unsigned char sha[EVP_MAX_MD_SIZE];
	size_t shan=0;
	char joined[128],b64[32],resp[256];
	size_t kl=wskey?strlen(wskey):0;
	int n;

	if (!wskey || !kl || kl>64 || kl+sizeof(WS_GUID)-1>=sizeof(joined))
	{
		web_send_static(c,400,"Bad Request","text/plain; charset=utf-8","400 Bad WebSocket key\n");
		return;
	}

	memcpy(joined,wskey,kl);
	memcpy(joined+kl,WS_GUID,sizeof(WS_GUID)-1);

	if (EVP_Q_digest(NULL,"SHA1",NULL,joined,kl+sizeof(WS_GUID)-1,sha,&shan)!=1
		|| shan!=20)
	{
		web_send_static(c,500,"Internal Server Error","text/plain; charset=utf-8","500 digest failure\n");
		return;
	}

	b64_encode(sha,(int)shan,b64);

	n=snprintf(resp,sizeof(resp),
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n\r\n",b64);

	if (n>0) web_enqueue(c,resp,(size_t)n);

	c->state=ST_WS;
}

/* --- embedded web chat ------------------------------------------
 *
 * Logged-in chat sessions are registered in wchat[] (mutex_webchat).
 * Hub worker threads, already holding mutex_users, look the registry up
 * to route public-chat taps and PMs and append JSON events to the
 * session out queues under mutex_out. The web thread owns client chat
 * state, presence snapshots and the login path. Lock order:
 * mutex_users -> mutex_webchat -> mutex_out.
 * ---------------------------------------------------------------- */

typedef struct
{
	int used;
	int slot;
	char nick[NICK_LEN+2];
	char perm[32];
} webchat_t;

static webchat_t wchat[WEB_CHAT_MAX];
static pthread_mutex_t mutex_webchat=PTHREAD_MUTEX_INITIALIZER;
static int chat_n=0;

int web_chat_active(void)
{
	return __atomic_load_n(&chat_n,__ATOMIC_RELAXED)!=0;
}

/* fetch a single-level JSON string value; 1 when present and not truncated */
static int jget(const char *json,const char *key,char *out,size_t n)
{
	char pat[48];
	const char *p;
	size_t i=0;
	int trunc=0;

	snprintf(pat,sizeof(pat),"\"%s\"",key);
	p=strstr(json,pat);
	if (!p) return 0;

	p=strchr(p+strlen(pat),':');
	if (!p) return 0;
	p++;

	while (*p==' '||*p=='\t') p++;
	if (*p!='"') return 0;
	p++;

	while (*p && *p!='"')
	{
		char ch=*p++;

		if (ch=='\\' && *p)
		{
			char c2=*p++;
			if (c2=='n') ch='\n';
			else if (c2=='r') ch='\r';
			else if (c2=='t') ch='\t';
			else ch=c2;
		}

		if (i+1<n) out[i++]=ch;
		else trunc=1;
	}

	out[i]=0;
	return (*p=='"' && !trunc);
}

/* append s to dst JSON-escaped (quotes, backslashes and control chars);
   bytes >= 0x80 pass through (valid UTF-8 stays valid) */
static size_t jesc(const char *s,char *dst,size_t cap)
{
	size_t o=0;

	if (!cap) return 0;
	dst[0]=0;

	while (*s && o+6<cap)
	{
		unsigned char ch=(unsigned char)*s++;

		if (ch=='"' || ch=='\\')
		{
			dst[o++]='\\';
			dst[o++]=ch;
		}
		else if (ch<0x20 || ch==0x7f)
			o+=(size_t)snprintf(dst+o,cap-o,"\\u%04x",ch);
		else
			dst[o++]=ch;
	}

	dst[o]=0;
	return o;
}

/* drop protocol-breaking characters from a chat message (the hub line
   separator '|' plus control bytes) */
static void txt_sanitize(char *s)
{
	char *r=s;

	for (;*s;s++)
	{
		unsigned char ch=(unsigned char)*s;
		if (ch<0x20 || ch==0x7f || ch=='|') continue;
		*r++=(char)ch;
	}

	*r=0;
}

/* nick rules mirror validnick(): printable US-ASCII, no protocol chars */
static int web_nick_ok(const char *nick)
{
	size_t n;

	if (!nick || !*nick) return 0;
	if (strlen(nick)+1>NICK_LEN) return 0;
	if (nick[0]=='#') return 0;

	for (n=0;nick[n];n++)
	{
		unsigned char ch=(unsigned char)nick[n];
		if (!isprint(ch) || isspace(ch) || ch=='$' || ch==':' || ch=='<'
			|| ch=='>' || ch=='?')
			return 0;
	}

	return 1;
}

static int perm_is_op(const char *perm)
{
	return (perm && (strchr(perm,'s')||strchr(perm,'a'))) ? 1 : 0;
}

/* caller must hold mutex_webchat */
static webchat_t *webchat_find(const char *nick)
{
	int i;

	for (i=0;i<WEB_CHAT_MAX;i++)
		if (wchat[i].used && !strcasecmp(wchat[i].nick,nick)) return &wchat[i];

	return NULL;
}

static void chat_send_slot(int slot,const char *json)
{
	unsigned char fb[WEB_JSON_MAX+16];
	size_t n=ws_write_frame(fb,sizeof(fb),0x1,(const unsigned char *)json,strlen(json));

	if (n) web_enqueue(&webc[slot],fb,n);
}

static void chat_send(webc_t *c,const char *json)
{
	chat_send_slot((int)(c-webc),json);
}

/* web thread only: send to every logged-in chat session */
static void chat_broadcast_all(const char *json)
{
	int i;

	for (i=0;i<WEB_MAX_CLIENTS;i++)
		if (webc[i].used && webc[i].chatmode && webc[i].clogged)
			chat_send(&webc[i],json);
}

static void web_chat_err(webc_t *c,const char *msg)
{
	char em[WEB_CHAT_TXT_MAX*6+1],json[WEB_JSON_MAX];

	jesc(msg,em,sizeof(em));
	snprintf(json,sizeof(json),"{\"a\":\"err\",\"m\":\"%s\"}",em);
	chat_send(c,json);
}

/* hub tap (worker holds mutex_users): public chat multicast line
   "<nick> text|" -> {"a":"pub",...} on every chat session */
void web_chat_broadcast(const char *line)
{
	char nick[NICK_LEN+2],text[WEB_CHAT_TXT_MAX+1];
	char en[(NICK_LEN+2)*6+1],et[WEB_CHAT_TXT_MAX*6+1],json[WEB_JSON_MAX];
	const char *gt;
	size_t nl;
	int i;

	if (line[0]!='<') return;

	gt=strchr(line,'>');
	if (!gt || gt==line+1 || (size_t)(gt-line-1)>(size_t)NICK_LEN || gt[1]!=' ')
		return;

	nl=(size_t)(gt-line-1);
	memcpy(nick,line+1,nl);
	nick[nl]=0;

	snprintf(text,sizeof(text),"%s",gt+2);
	nl=strlen(text);
	if (nl && text[nl-1]=='|') text[nl-1]=0;

	if (!*text) return;

	jesc(nick,en,sizeof(en));
	jesc(text,et,sizeof(et));
	snprintf(json,sizeof(json),"{\"a\":\"pub\",\"n\":\"%s\",\"t\":\"%s\"}",en,et);

	pthread_mutex_lock(&mutex_webchat);
	for (i=0;i<WEB_CHAT_MAX;i++)
		if (wchat[i].used) chat_send_slot(wchat[i].slot,json);
	pthread_mutex_unlock(&mutex_webchat);
}

/* worker holds mutex_users (validnick): is nick claimed by a chat session? */
int web_chat_nick_used(const char *nick)
{
	int used;

	pthread_mutex_lock(&mutex_webchat);
	used=(webchat_find(nick)!=NULL);
	pthread_mutex_unlock(&mutex_webchat);

	return used;
}

/* worker holds mutex_users ($To:): deliver a PM to a chat session;
   1 when a session owns the nick */
int web_chat_deliver_pm(const char *to,const char *from,const char *msg)
{
	webchat_t *w;
	char en[(NICK_LEN+2)*6+1],et[WEB_CHAT_TXT_MAX*6+1],json[WEB_JSON_MAX];
	int rc=0;

	while (*msg==' ') msg++;	/* dc_To keeps the "$<nick> msg" separator space */

	pthread_mutex_lock(&mutex_webchat);

	w=webchat_find(to);
	if (w)
	{
		jesc(from,en,sizeof(en));
		jesc(msg,et,sizeof(et));
		snprintf(json,sizeof(json),"{\"a\":\"pm\",\"n\":\"%s\",\"t\":\"%s\"}",en,et);
		chat_send_slot(w->slot,json);
		rc=1;
	}

	pthread_mutex_unlock(&mutex_webchat);

	return rc;
}

/* presence snapshot: registered non-console hub users + chat sessions */
typedef struct { char nick[NICK_LEN+2]; int op; } ulist_t;

static ulist_t ul_cur[WEB_ULIST_MAX],ul_prev[WEB_ULIST_MAX];
static int ul_prev_n=0;

static void chat_snapshot(ulist_t *out,int *outn)
{
	int i,n=0;

	pthread_mutex_lock(&mutex_users);

	for (i=0;i<NUSERS && n<WEB_ULIST_MAX;i++)
	{
		userrec_t *u=user[i];

		if (!u || !u->nick || u->cons || !user_tst_state(u,STATE_REGISTERED))
			continue;

		snprintf(out[n].nick,sizeof(out[n].nick),"%s",u->nick);
		out[n].op=perm_is_op(u->perm);
		n++;
	}

	pthread_mutex_unlock(&mutex_users);

	pthread_mutex_lock(&mutex_webchat);

	for (i=0;i<WEB_CHAT_MAX && n<WEB_ULIST_MAX;i++)
	{
		if (!wchat[i].used) continue;

		snprintf(out[n].nick,sizeof(out[n].nick),"%s",wchat[i].nick);
		out[n].op=perm_is_op(wchat[i].perm);
		n++;
	}

	pthread_mutex_unlock(&mutex_webchat);

	*outn=n;
}

/* send the full user list to one session (web thread) */
static void chat_send_users(webc_t *c)
{
	static char json[WEB_JSON_MAX];
	ulist_t cur[WEB_ULIST_MAX];
	size_t o;
	int n=0,i;

	chat_snapshot(cur,&n);

	o=(size_t)snprintf(json,sizeof(json),"{\"a\":\"users\",\"u\":[");

	for (i=0;i<n && o<sizeof(json)-80;i++)
	{
		char en[(NICK_LEN+1)*6+1];

		jesc(cur[i].nick,en,sizeof(en));
		o+=(size_t)snprintf(json+o,sizeof(json)-o,"%s[\"%s\",%d]",
			i? ",":"" ,en,cur[i].op);
	}

	snprintf(json+o,sizeof(json)-o,"]}");
	chat_send(c,json);
}

/* presence diff -> join/leave events (web thread, every ~2s) */
static void chat_poll(void)
{
	char json[WEB_JSON_MAX],en[(NICK_LEN+1)*6+1];
	int cn=0,i,j;

	chat_snapshot(ul_cur,&cn);

	for (i=0;i<cn;i++)
	{
		int found=0;

		for (j=0;j<ul_prev_n;j++)
			if (!strcasecmp(ul_cur[i].nick,ul_prev[j].nick)) { found=1; break; }

		if (found) continue;

		jesc(ul_cur[i].nick,en,sizeof(en));
		snprintf(json,sizeof(json),"{\"a\":\"join\",\"n\":\"%s\",\"op\":%d}",
			en,ul_cur[i].op);
		chat_broadcast_all(json);
	}

	for (j=0;j<ul_prev_n;j++)
	{
		int found=0;

		for (i=0;i<cn;i++)
			if (!strcasecmp(ul_cur[i].nick,ul_prev[j].nick)) { found=1; break; }

		if (found) continue;

		jesc(ul_prev[j].nick,en,sizeof(en));
		snprintf(json,sizeof(json),"{\"a\":\"leave\",\"n\":\"%s\"}",en);
		chat_broadcast_all(json);
	}

	ul_prev_n=cn;
	memcpy(ul_prev,ul_cur,sizeof(ulist_t)*(size_t)cn);
}

static int chat_rate_ok(webc_t *c)
{
	time_t now=time(NULL);

	if (now!=c->rl_win)
	{
		c->rl_win=now;
		c->rl_msgs=0;
	}

	if (c->rl_msgs>=WEB_CHAT_RATE) return 0;
	c->rl_msgs++;

	return 1;
}

static void chat_login(webc_t *c,const char *json)
{
	char nick[NICK_LEN+2],pass[129],perm[16];
	char en[(NICK_LEN+2)*6+1],ev[WEB_JSON_MAX];
	int op;

	if (c->login_attempts++>=WEB_CHAT_LOGIN_MAX)
	{
		c->closing=1;
		return;
	}

	if (!jget(json,"n",nick,sizeof(nick)) || !jget(json,"p",pass,sizeof(pass)))
	{
		web_chat_err(c,"malformed login");
		return;
	}

	if (!web_nick_ok(nick))
	{
		web_chat_err(c,"invalid nick");
		return;
	}

	/* one critical section (users outer, webchat inner) so a hub client
	   cannot claim the nick between the account check and the registration */
	pthread_mutex_lock(&mutex_users);

	if (!login_verify(nick,pass,perm,sizeof(perm)))
	{
		pthread_mutex_unlock(&mutex_users);
		web_chat_err(c,"invalid nick or password");
		return;
	}

	if (nick2id(nick)!=-1)
	{
		pthread_mutex_unlock(&mutex_users);
		web_chat_err(c,"nick already in use");
		return;
	}

	pthread_mutex_lock(&mutex_webchat);

	if (c->clogged || webchat_find(nick))
	{
		pthread_mutex_unlock(&mutex_webchat);
		pthread_mutex_unlock(&mutex_users);
		web_chat_err(c,"already logged in");
		return;
	}

	{
		int i;

		for (i=0;i<WEB_CHAT_MAX;i++)
			if (!wchat[i].used) break;

		if (i==WEB_CHAT_MAX)
		{
			pthread_mutex_unlock(&mutex_webchat);
			pthread_mutex_unlock(&mutex_users);
			web_chat_err(c,"chat is full");
			return;
		}

		wchat[i].used=1;
		wchat[i].slot=(int)(c-webc);
		snprintf(wchat[i].nick,sizeof(wchat[i].nick),"%s",nick);
		snprintf(wchat[i].perm,sizeof(wchat[i].perm),"%s",perm);
	}

	pthread_mutex_unlock(&mutex_webchat);
	pthread_mutex_unlock(&mutex_users);

	snprintf(c->cnick,sizeof(c->cnick),"%s",nick);
	snprintf(c->cperm,sizeof(c->cperm),"%s",perm);
	c->clogged=1;
	__atomic_fetch_add(&chat_n,1,__ATOMIC_RELAXED);

	log_write(FAC_ACCESS,PR_INFO,"Web chat login: %s.",nick);

	op=perm_is_op(perm);
	jesc(nick,en,sizeof(en));
	snprintf(ev,sizeof(ev),"{\"a\":\"ok\",\"n\":\"%s\",\"op\":%d}",en,op);
	chat_send(c,ev);

	chat_send_users(c);

	/* baseline the presence diff for sessions that joined/quit while
	   the hub had no chat clients */
	chat_snapshot(ul_prev,&ul_prev_n);
}

static void chat_pub(webc_t *c,const char *text)
{
	char line[WEB_CHAT_TXT_MAX+2*NICK_LEN+8];

	if (!chat_rate_ok(c))
	{
		web_chat_err(c,"slow down");
		return;
	}

	if (!conf.allow_chat)
	{
		web_chat_err(c,"public chat disabled");
		return;
	}

	snprintf(line,sizeof(line),"<%s> %s|",c->cnick,text);

	pthread_mutex_lock(&mutex_users);
	disttcp(NULL,line);	/* hub multicast: the web tap feeds every
				   chat session, including this one, mirroring
				   the hub's sender-included echo */
	pthread_mutex_unlock(&mutex_users);
}

static void chat_pm(webc_t *c,const char *to,const char *text)
{
	char en[(NICK_LEN+2)*6+1],et[WEB_CHAT_TXT_MAX*6+1],json[WEB_JSON_MAX];
	int id,delivered=0;

	if (!chat_rate_ok(c))
	{
		web_chat_err(c,"slow down");
		return;
	}

	if (!web_nick_ok(to))
	{
		web_chat_err(c,"invalid nick");
		return;
	}

	pthread_mutex_lock(&mutex_users);

	id=nick2id((char *)to);

	if (id>=0)
	{
		if (!user[id]->cons)
		{
			/* standard NMDC private message to a hub user
			   (privmsg formats "$To:.. From:.. $<..> ..") */
			privmsg(user[id],c->cnick,"%s",text);
			delivered=1;
		}
	}
	else
		delivered=web_chat_deliver_pm(to,c->cnick,text);

	pthread_mutex_unlock(&mutex_users);

	if (!delivered)
	{
		web_chat_err(c,"no such nick");
		return;
	}

	jesc(to,en,sizeof(en));
	jesc(text,et,sizeof(et));
	snprintf(json,sizeof(json),"{\"a\":\"pm\",\"n\":\"%s\",\"t\":\"%s\",\"me\":1}",
		en,et);
	chat_send(c,json);
}

/* one chat WS text frame (web thread) */
static void chat_input(webc_t *c,const char *payload,size_t len)
{
	char buf[WEB_JSON_MAX],t[WEB_CHAT_TXT_MAX+1],a[8];

	if (len>=sizeof(buf)) len=sizeof(buf)-1;
	memcpy(buf,payload,len);
	buf[len]=0;

	if (!c->clogged)
	{
		if (jget(buf,"a",a,sizeof(a)) && !strcmp(a,"login"))
			chat_login(c,buf);
		else
			web_chat_err(c,"login required");

		return;
	}

	if (!jget(buf,"a",a,sizeof(a))) return;

	if (!strcmp(a,"pub"))
	{
		if (!jget(buf,"t",t,sizeof(t)))
		{
			web_chat_err(c,"malformed message");
			return;
		}

		txt_sanitize(t);
		if (*t) chat_pub(c,t);
	}
	else if (!strcmp(a,"pm"))
	{
		char n[NICK_LEN+2];

		if (!jget(buf,"t",t,sizeof(t)) || !jget(buf,"n",n,sizeof(n)))
		{
			web_chat_err(c,"malformed message");
			return;
		}

		txt_sanitize(t);
		if (*t) chat_pm(c,n,t);
	}
	else if (!strcmp(a,"login"))
		web_chat_err(c,"already logged in");
}

/* respond to a fully received HTTP header block */
static void web_handle_request(webc_t *c)
{
	char path[1024];
	char wskey[80], *hk;

	if (sscanf(c->req,"GET %1023s HTTP/1.",path)!=1)
	{
		web_send_static(c,405,"Method Not Allowed","text/plain; charset=utf-8","405 Method Not Allowed\n");
		return;
	}

	if (!strcmp(path,"/healthcheck"))
	{
		web_send_health(c);
		return;
	}

	if (!strcmp(path,"/") || !strcmp(path,"/index.html")
		|| !strcmp(path,"/chat") || !strcmp(path,"/chat.html"))
	{
		web_send_static(c,200,"OK","text/html; charset=utf-8",web_page_body(&pg_chat,fb_chat));
		return;
	}

	if (!strcmp(path,"/console") || !strcmp(path,"/console.html"))
	{
		web_send_static(c,200,"OK","text/html; charset=utf-8",web_page_body(&pg_cons,fb_cons));
		return;
	}

	if (!strcmp(path,"/ws") || !strcmp(path,"/chatws"))
	{
		int ischat=(!strcmp(path,"/chatws"));

		hk=strstr(c->req,"Sec-WebSocket-Key:");
		wskey[0]=0;

		if (hk) sscanf(hk,"Sec-WebSocket-Key: %79s",wskey);

		if (!wskey[0])
		{
			web_send_static(c,400,"Bad Request","text/plain; charset=utf-8","400 missing Sec-WebSocket-Key\n");
			return;
		}

		if (!ischat)
		{
			if (!conf.listen_port_cons)
			{
				web_send_static(c,503,"Service Unavailable","text/plain; charset=utf-8","503 console port disabled\n");
				return;
			}

			c->consfd=web_console_connect();

			if (c->consfd==-1)
			{
				web_send_static(c,503,"Service Unavailable","text/plain; charset=utf-8","503 console connection refused\n");
				return;
			}

			log_write(FAC_ACCESS,PR_INFO,"Web console upgrade (console via loopback:%u).",
				conf.listen_port_cons);
		}
		else
		{
			log_write(FAC_ACCESS,PR_INFO,"Web chat upgrade.");
		}

		web_websocket(c,wskey);

		if (ischat) c->chatmode=1;
		return;
	}

	web_send_static(c,404,"Not Found","text/plain; charset=utf-8","404 Not Found\n");
}

/* pump one client's socket pair; returns 0 to release the slot */
static int web_service(webc_t *c,short ev_c,short ev_k)
{
	/* flush the outbound queue first (workers may have appended chat
	   events since the last cycle, hence the output-queue lock) */
	if (ev_c&POLLOUT && c->outn)
	{
		ssize_t w;

		pthread_mutex_lock(&mutex_out);
		w=write(c->cfd,c->out,c->outn);

		if (w>0)
		{
			memmove(c->out,c->out+w,c->outn-(size_t)w);
			c->outn-=(size_t)w;
		}

		pthread_mutex_unlock(&mutex_out);

		if (w<0 && errno!=EAGAIN && errno!=EINTR)
			return 0;
	}

	/* flush the console input queue */
	if (c->consfd!=-1 && ev_k&POLLOUT && c->consqn)
	{
		ssize_t w=write(c->consfd,c->consq,c->consqn);

		if (w>0)
		{
			memmove(c->consq,c->consq+w,c->consqn-(size_t)w);
			c->consqn-=(size_t)w;
		}
		else if (w<0 && errno!=EAGAIN && errno!=EINTR)
			c->closing=1;
	}

	if (c->state==ST_REQ && ev_c&POLLIN)
	{
		ssize_t r=read(c->cfd,c->req+c->reqn,WEB_REQ_MAX-c->reqn-1);

		if (r==0) return 0;

		if (r<0)
		{
			if (errno==EAGAIN || errno==EINTR) goto after_read;
			return 0;
		}

		c->reqn+=(size_t)r;
		c->req[c->reqn]=0;

		if (strstr(c->req,"\r\n\r\n")) web_handle_request(c);
		else if (c->reqn>=WEB_REQ_MAX-1) return 0;
	}

after_read:

	if (c->state==ST_WS && !c->closing && ev_c&POLLIN)
	{
		ssize_t r=read(c->cfd,c->rbuf+c->rbufn,WEB_RBUF_MAX-c->rbufn);

		if (r==0) c->closing=1;
		else if (r<0)
		{
			if (errno!=EAGAIN && errno!=EINTR) c->closing=1;
		}
		else
		{
			c->rbufn+=(size_t)r;

			for (;;)
			{
				struct ws_frame f;
				int pr=ws_parse_frame(c->rbuf,c->rbufn,&f);
				unsigned char *pl;
				size_t fl;
				int k;

				if (pr<=0) break;
				if (f.len>WEB_FRAME_MAX || c->rbufn<f.hdrlen+f.len) break;

				pl=c->rbuf+f.hdrlen;
				fl=(size_t)f.len;

				if (f.masked)
					for (k=0;k<4;k++)
					{
						size_t j;
						for (j=k;j<fl;j+=4) pl[j]^=f.mask[j&3];
					}

				if (f.opcode==0x8) c->closing=1;
				else if (f.opcode==0x9)
				{
					unsigned char pong[WEB_RBUF_MAX];
					size_t n=ws_write_frame(pong,sizeof(pong),0xA,pl,fl);
					if (n) web_enqueue(c,pong,n);
				}
				else if (f.opcode==0x1 && c->chatmode)
				{
					chat_input(c,(const char *)pl,fl);
				}
				else if (f.opcode==0x1 && c->consfd!=-1)
				{
					if (c->consqn+fl<=WEB_CONSQ_MAX)
					{
						memcpy(c->consq+c->consqn,pl,fl);
						c->consqn+=fl;
					}
					else c->closing=1;
				}

				c->rbufn-=f.hdrlen+fl;
				memmove(c->rbuf,c->rbuf+f.hdrlen+fl,c->rbufn);
			}

			if (c->rbufn>=WEB_RBUF_MAX) return 0;
		}
	}

	if (c->state==ST_WS && !c->closing && c->consfd!=-1 && ev_k&POLLIN)
	{
		unsigned char raw[4096],clean[4096],frame[4200];
		ssize_t r=read(c->consfd,raw,sizeof(raw));
		size_t cn,fn;

		if (r==0) c->closing=1;
		else if (r<0)
		{
			if (errno!=EAGAIN && errno!=EINTR) c->closing=1;
		}
		else
		{
			cn=strip_iac(raw,(size_t)r,clean);
			fn=ws_write_frame(frame,sizeof(frame),0x1,clean,cn);
			if (fn) web_enqueue(c,frame,fn);
		}
	}

	if (__atomic_load_n(&c->obuffull,__ATOMIC_RELAXED) || c->closing) 
	{
		if (c->closing) { if (c->outn==0) return 0; }
		else if (c->outn==0) return 0;
	}

	return 1;
}

void listen_thread_http(void *args)
{
	struct pollfd pfd[1+2*WEB_MAX_CLIENTS];
	int poll_ticks=0;

	(void)args;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

	/* built-in default pages (fallback when the config files are absent)
	   and the config-directory page files themselves */
	fb_cons=my_sprintf("%s<style>\n%s\n</style>\n%s<script>\n%s\n</script>\n</body>\n</html>\n",
		WEB_HTML_A,WEB_CSS,WEB_HTML_B,WEB_JS);
	fb_chat=my_sprintf("%s<style>\n%s\n</style>\n%s<script>\n%s\n</script>\n</body>\n</html>\n",
		WEB_CHAT_HTML_A,WEB_CHAT_CSS,WEB_CHAT_HTML_B,WEB_CHAT_JS);

	web_page_path(&pg_chat,CONF_WEBCHAT);
	web_page_path(&pg_cons,CONF_WEBCONSOLE);
	web_page_check(&pg_chat);
	web_page_check(&pg_cons);

	for (;;)
	{
		int n=0,i;

		pfd[n].fd=listen_http;
		pfd[n].events=POLLIN;
		pfd[n++].revents=0;

		for (i=0;i<WEB_MAX_CLIENTS;i++)
		{
			webc_t *c=&webc[i];

			if (!c->used) continue;

			pfd[n].fd=c->cfd;
			pfd[n].events=POLLIN|((c->outn)?POLLOUT:0);
			pfd[n++].revents=0;

			if (c->consfd!=-1)
			{
				pfd[n].fd=c->consfd;
				pfd[n].events=POLLIN|((c->consqn)?POLLOUT:0);
				pfd[n++].revents=0;
			}
		}

		if (poll(pfd,(unsigned)n,WEB_POLL_MS)<0)
		{
			if (errno!=EINTR) log_write(FAC_NETWORK,PR_ERROR,"web poll() failed: %s.",strerror(errno));
			continue;
		}

		if (pfd[0].revents&POLLIN)
		{
			int new_sock=accept(listen_http,NULL,NULL);

			if (new_sock!=-1)
			{
				int slot=-1;

				for (i=0;i<WEB_MAX_CLIENTS;i++)
					if (!webc[i].used) { slot=i; break; }

				if (slot==-1)
				{
					const char busy[]="503 too many web clients\n";
					/* best effort */
					if (write(new_sock,busy,sizeof(busy)-1)<0) {}
					close(new_sock);
				}
				else
				{
					webc_t *c=&webc[slot];

					memset(c,0,sizeof(*c));

					c->used=1;
					c->state=ST_REQ;
					c->cfd=new_sock;
					c->consfd=-1;

					fcntl(new_sock,F_SETFL,O_NONBLOCK);
				}
			}
		}

		for (i=0;i<WEB_MAX_CLIENTS;i++)
		{
			webc_t *c=&webc[i];
			short ev_c=0,ev_k=0;
			int j;

			if (!c->used) continue;

			for (j=1;j<n;j++)
				if (pfd[j].fd==c->cfd) ev_c=pfd[j].revents;

			for (j=1;j<n;j++)
				if (pfd[j].fd==c->consfd) ev_k=pfd[j].revents;

			if (!web_service(c,ev_c,ev_k) || (ev_c&(POLLERR|POLLHUP)))
			{
				if (c->clogged)
				{
					pthread_mutex_lock(&mutex_webchat);

					for (j=0;j<WEB_CHAT_MAX;j++)
						if (wchat[j].used && wchat[j].slot==i)
						{
							wchat[j].used=0;
							__atomic_fetch_sub(&chat_n,1,__ATOMIC_RELAXED);
							break;
						}

					pthread_mutex_unlock(&mutex_webchat);

					log_write(FAC_ACCESS,PR_INFO,"Web chat disconnect: %s.",c->cnick);
				}

				if (c->cfd!=-1) close(c->cfd);
				if (c->consfd!=-1) close(c->consfd);
				memset(c,0,sizeof(*c));
			}
		}

		/* presence poll: ~every 2s when at least one chat session lives */
		if (web_chat_active() && ++poll_ticks>=2)
		{
			poll_ticks=0;
			chat_poll();
		}

		/* live re-read of the page files (mtime stat, once per cycle) */
		web_page_check(&pg_chat);
		web_page_check(&pg_cons);
	}
}

/* VIM Settings {{{
* Local variables:
* tab-width: 4
* c-basic-offset: 4
* End:
* vim600: sw=4 ts=4 sts=4 cindent fdm=marker
* vim<600: sw=4 ts=4
* }}} */
