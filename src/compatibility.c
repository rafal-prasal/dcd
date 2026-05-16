/* $Id: compatibility.c,v 1.2 2007/04/05 21:46:49 rav Exp $ */

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

#ifdef INCLUDED_STRSEP
char *strsep(char **pp, const char *delim)
{
	char *p, *q;
           
	if ((p = *pp) == NULL)	return NULL;

	if ((q = strpbrk(p, delim)) != NULL)
	{
		*pp = q + 1;
		*q = '\0';
	}
	else *pp = 0;

  return p;
}
#endif

#ifdef INCLUDED_DAEMON
/* daemon() - fall into background {{{ */
int daemon(int nochdir,int noclose)
{
	pid_t ret;
#ifdef TIOCNOTTY
	int fd;
#endif

	ret=fork();
	if (ret) exit(0);
	if (ret<0) return (int)ret;
	if (noclose==0)
	{
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
#ifdef TIOCNOTTY
		/* disconnect from the controlling tty */
		fd=open("/dev/tty",O_RDWR|O_NOCTTY);
		if (fd>=0)
		{
			(void)ioctl(fd,TIOCNOTTY,NULL);
			close(fd);
		}
#endif
	}
#ifdef HAVE_SETSID
	setsid();
#endif
	if (nochdir==0) chdir("/");
	return 0;
} /* }}} */
#endif /* INCLUDED_DAEMON */
																								
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

