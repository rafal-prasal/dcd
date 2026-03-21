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

#include <memory.h>
#include <ctype.h>

#include <inttypes.h>

/* -- system dependencies -- */

#if defined( WIN32 )
/* prevent inclusion of winsock.h */
#define _WINSOCKAPI_
#endif /* WIN32 */

/* include POSIX threads header files */
#include <pthread.h>

#if defined( WIN32 )
#define WIN32_LEAN_AND_MEAN

/* include Windows base header files */
#pragma pack( 4 )
#include <process.h>
#include <windows.h>

/* include Windows sockets header files	*/
#include <winsock2.h>

/* include Windows TCP/IP header files	*/
#include <ws2tcpip.h>

#else /* WIN32 */

/* include Unix base header files	*/
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/signal.h>
#include <unistd.h>

#include <sys/time.h>

#include <syslog.h>
#include <tcpd.h>

#include <pwd.h>
#include <grp.h>

/* include Unix sockets header files	*/
#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <net/if.h>

/* include Unix TCP/IP header files	*/
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#endif /* !WIN32 */

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
