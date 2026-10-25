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
 *   GET /            - web console client (HTML + console.js + console.css)
 *   GET /healthcheck - 200 OK when the hub core is responsive, 500 otherwise
 *   GET /ws          - WebSocket upgrade bridged to the local console port
 *
 * The web console reuses the telnet console protocol end-to-end: each
 * WebSocket client gets a loopback TCP connection to listen_port_cons and
 * the raw protocol bytes are relayed between the WebSocket frames and the
 * console socket. Telnet IAC option triplets emitted by the console are
 * stripped before delivery. One single thread (this one) owns all web
 * state; no locking is needed except a trylock of mutex_users for the
 * health probe.
 * ----------------------------------------------------------------
 * }}} */

#include "pch.h"

#include <openssl/evp.h>

#include "web_assets.h"

extern config_t conf;
extern int listen_http;
extern pthread_mutex_t mutex_users;

#define WEB_MAX_CLIENTS 8
#define WEB_REQ_MAX 8192
#define WEB_RBUF_MAX 8192
#define WEB_OUT_MAX (128*1024)
#define WEB_CONSQ_MAX 4096
#define WEB_FRAME_MAX (64*1024)
#define WEB_POLL_MS 1000

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

	int closing;
} webc_t;

static webc_t webc[WEB_MAX_CLIENTS];

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
	if (c->outn+len>WEB_OUT_MAX)
	{
		c->closing=1;	/* dead client: stop buffering */
		return;
	}

	memcpy(c->out+c->outn,data,len);
	c->outn+=len;
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

	if (!strcmp(path,"/") || !strcmp(path,"/index.html"))
	{
		web_send_static(c,200,"OK","text/html; charset=utf-8",WEB_HTML);
		return;
	}

	if (!strcmp(path,"/console.css"))
	{
		web_send_static(c,200,"OK","text/css; charset=utf-8",WEB_CSS);
		return;
	}

	if (!strcmp(path,"/console.js"))
	{
		web_send_static(c,200,"OK","application/javascript; charset=utf-8",WEB_JS);
		return;
	}

	if (!strcmp(path,"/ws"))
	{
		hk=strstr(c->req,"Sec-WebSocket-Key:");
		wskey[0]=0;

		if (hk) sscanf(hk,"Sec-WebSocket-Key: %79s",wskey);

		if (!wskey[0])
		{
			web_send_static(c,400,"Bad Request","text/plain; charset=utf-8","400 missing Sec-WebSocket-Key\n");
			return;
		}

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

		web_websocket(c,wskey);
		return;
	}

	web_send_static(c,404,"Not Found","text/plain; charset=utf-8","404 Not Found\n");
}

/* pump one client's socket pair; returns 0 to release the slot */
static int web_service(webc_t *c,short ev_c,short ev_k)
{
	/* flush the outbound queue first */
	if (ev_c&POLLOUT && c->outn)
	{
		ssize_t w=write(c->cfd,c->out,c->outn);

		if (w>0)
		{
			memmove(c->out,c->out+w,c->outn-(size_t)w);
			c->outn-=(size_t)w;
		}
		else if (w<0 && errno!=EAGAIN && errno!=EINTR)
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

	if (c->closing && c->outn==0) return 0;

	return 1;
}

void listen_thread_http(void *args)
{
	struct pollfd pfd[1+2*WEB_MAX_CLIENTS];

	(void)args;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

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
				if (c->cfd!=-1) close(c->cfd);
				if (c->consfd!=-1) close(c->consfd);
				memset(c,0,sizeof(*c));
			}
		}
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
