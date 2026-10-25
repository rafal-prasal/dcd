/* $Id: pch.h,v 1.27 2007/02/19 20:59:19 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __PCH_H_INCLUDED__ )
#define __PCH_H_INCLUDED__

#define MAXUSERS 5000
#define FD_SETSIZE MAXUSERS+30

/* include ANSI C/C++ header files */
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include <errno.h>
#include <stdio.h>

#include <stdlib.h>

#include <ctype.h>

#include <time.h>

#include <inttypes.h>

/* include POSIX threads header files */
#include <pthread.h>

/* include POSIX base header files	*/
#include <sys/ioctl.h>
#include <fcntl.h>

#include <signal.h>
#include <unistd.h>

#include <sys/select.h>
#include <poll.h>

#include <sys/time.h>

#include <syslog.h>

#include <pwd.h>
#include <grp.h>

/* include POSIX sockets header files	*/
#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <net/if.h>

/* include POSIX TCP/IP header files	*/
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

/* include local header files		*/

#include "config.h"
#include "main.h"
#include "cmd.h"
#include "cmd.dc.h"
#include "cmd.opt_line.h"
#include "cmd.user.h"
#include "cmd.conf.h"
#include "cmd.udp.h"
#include "communicates.h"
#include "console.h"
#include "match.h"  
#include "net.h"
#include "penalties.h"
#include "conf.h"
#include "misc.h"
#include "sha512.h"
#include "stats.h"
#include "compatibility.h"
#include "log.h"

#endif /* __vXhz__PCH_H__INCLUDED__	*/

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
