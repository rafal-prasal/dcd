/* syslog-forward - bridge dcd's syslog output to the container's stdout
 *
 * dcd always logs through syslog(3) and detaches its stderr when it
 * daemonizes, so a container would lose every log line. This helper binds
 * the datagram /dev/log socket, reads the messages glibc sends there
 * (priority arrives in the SO_PRIORITY control message) and writes one
 * formatted line per message to stdout.
 *
 * usage: syslog-forward [socket-path]   (default: /dev/log)
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <syslog.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#define MAX_MSG 65536

static volatile sig_atomic_t stop=0;

static void on_signal(int sig)
{
	(void)sig;
	stop=1;
}

static void log_exit(const char *what)
{
	fprintf(stderr,"syslog-forward: %s: %s\n",what,strerror(errno));
	exit(1);
}

static const char *severity_name(int prio)
{
	switch(prio&LOG_PRIMASK)
	{
		case LOG_EMERG:   return "emerg";
		case LOG_ALERT:   return "alert";
		case LOG_CRIT:    return "crit";
		case LOG_ERR:     return "error";
		case LOG_WARNING: return "warn";
		case LOG_NOTICE:  return "notice";
		case LOG_INFO:    return "info";
		default:          return "debug";
	}
}

static const char *facility_name(int prio)
{
	switch(prio&LOG_FACMASK)
	{
		case LOG_KERN:     return "kern";
		case LOG_USER:     return "user";
		case LOG_MAIL:     return "mail";
		case LOG_DAEMON:   return "daemon";
		case LOG_AUTH:     return "auth";
		case LOG_SYSLOG:   return "syslog";
		case LOG_LPR:      return "lpr";
		case LOG_NEWS:     return "news";
		case LOG_UUCP:     return "uucp";
		case LOG_CRON:     return "cron";
		case LOG_AUTHPRIV: return "authpriv";
		case LOG_FTP:      return "ftp";
		default:           return "local";
	}
}

int main(int argc,char *argv[])
{
	const char *path=(argc>1)?argv[1]:"/dev/log";
	char buf[MAX_MSG];
	char cbuf[64];
	int fd;
	struct sockaddr_un sa;

	struct sigaction sa_term;

	memset(&sa_term,0,sizeof(sa_term));
	sa_term.sa_handler=on_signal;
	sigemptyset(&sa_term.sa_mask);
	sigaction(SIGTERM,&sa_term,NULL);
	sigaction(SIGINT,&sa_term,NULL);

	unlink(path);

	fd=socket(AF_UNIX,SOCK_DGRAM,0);
	if (fd<0) log_exit("socket()");

	memset(&sa,0,sizeof(sa));
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,path,sizeof(sa.sun_path)-1);

	if (bind(fd,(struct sockaddr *)&sa,sizeof(sa))) log_exit("bind()");

	/* dcd drops root and connects to /dev/log as an unprivileged user */
	if (chmod(path,0666)) log_exit("chmod()");

	setvbuf(stdout,NULL,_IOLBF,0);

	while(!stop)
	{
		struct iovec iov={buf,sizeof(buf)};
		struct msghdr msg;
		struct cmsghdr *cm;
		int prio=LOG_USER|LOG_NOTICE,len;
		struct timespec ts;
		struct tm tm_;
		char stamp[32];

		memset(&msg,0,sizeof(msg));
		memset(cbuf,0,sizeof(cbuf));
		msg.msg_iov=&iov;
		msg.msg_iovlen=1;
		msg.msg_control=cbuf;
		msg.msg_controllen=sizeof(cbuf);

		len=recvmsg(fd,&msg,MSG_CMSG_CLOEXEC);
		if (len<0)
		{
			if (errno==EINTR||errno==EAGAIN||errno==ECONNRESET) continue;
			log_exit("recvmsg()");
		}

		for (cm=CMSG_FIRSTHDR(&msg); cm; cm=CMSG_NXTHDR(&msg,cm))
			if (cm->cmsg_level==SOL_SOCKET && cm->cmsg_type==SO_PRIORITY
				&& CMSG_LEN(sizeof(int))<=cm->cmsg_len)
				memcpy(&prio,CMSG_DATA(cm),sizeof(int));

		/* some senders (RFC3164-style, util-linux logger) put the priority
		   in a "<PRI>" payload prefix instead of / in addition to the cmsg */
		if (len>4 && buf[0]=='<' && (unsigned char)buf[1]>='0' && (unsigned char)buf[1]<='9')
		{
			char *end;
			long p=strtol(buf+1,&end,10);
			if (end>buf+1 && end<buf+len && *end=='>')
			{
				prio=(int)p;
				len-=(int)(end+1-buf);
				memmove(buf,end+1,(size_t)len);
			}
		}

		clock_gettime(CLOCK_REALTIME,&ts);
		gmtime_r(&ts.tv_sec,&tm_);
		strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",&tm_);

		printf("%s %s/%s: ",stamp,facility_name(prio),severity_name(prio));
		fwrite(buf,1,(size_t)len,stdout);
		putc('\n',stdout);
	}

	close(fd);
	unlink(path);
	return 0;
}
