# $Id: configure.ac,v 1.129 2007/04/05 21:46:49 rav Exp $
# Process this file with autoconf to produce a configure script.
AC_INIT([dcd], [1.1.1], [dc-hub@ds.pg.gda.pl])
AC_CONFIG_SRCDIR([src/main.c])

AM_INIT_AUTOMAKE
AC_PREREQ(2.50)
CFLAGS="-O2 ${CFLAGS}"

AC_LANG(C)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

dnl Checks for functions in libraries.
AC_CHECK_LIB(c, bind, [], 
	AC_CHECK_LIB(socket, bind, [], [ exit 1 ]))
		
AC_CHECK_LIB(c, inet_aton, [],
		AC_CHECK_LIB(resolv, inet_aton, [], [ exit 1 ]))

AC_CHECK_LIB(c, inet_ntoa, [],
		AC_CHECK_LIB(nsl, inet_ntoa, [], [ exit 1 ]))

################
### Check for missing functions on some systems
###

AC_CHECK_FUNC(daemon, [], AC_DEFINE(INCLUDED_DAEMON))
AC_CHECK_FUNC(strsep, [], AC_DEFINE(INCLUDED_STRSEP))

#######################
dnl Check for arguments

################
### libpthreads:
###

dnl Checks for libpthreads
AC_ARG_WITH(pthreads, 
	    AC_HELP_STRING([--with-pthreads],[Use pthreads. REQUIRED.]), [ 
	     if test "$withval" = "yes" ; then
		     INCLUDEPTHREAD=1
	     else
		     INCLUDEPTHREAD=0
		     AC_MSG_ERROR(****Library libpthread support must be turn on****); 
	     fi ],[INCLUDEPTHREAD=1])

if test "$INCLUDEPTHREAD"=1 ; then
	AC_MSG_CHECKING([for pthread_create in -lpthread])
	saved_LIBS="$LDFLAGS $LIBS"
	LIBS="-lpthread $saved_LIBS"
	AC_TRY_LINK([#include <pthread.h>],
		    [pthread_create((void*)0,(void*)0,(void*)0,(void*)0)],
		    [AC_MSG_RESULT([yes]); AC_DEFINE(HAVE_MITPTHREADS)],
		    [AC_MSG_RESULT([no]); LIBS="$saved_LIBS"])
	AC_CHECK_LIB(c_r,pthread_create,
		     [LIBS="$LIBS -pthread"
		      AC_DEFINE(HAVE_MITPTHREADS) ]
		     )
else
	AC_MSG_ERROR(****Library not found: pthread. You MUST to have libpthreads****);
fi

################
### Libwrap:
###
## TODO:
## - checks for static libraries

dnl check for tcp wrapper support
AC_ARG_WITH(libwrap-path,
	    AC_HELP_STRING([--with-libwrap-path[=LIBPATH]],[Compile with libwrap (tcp_wrappers) support.]),
	     [if test "$with_libwrap_path" != "no"; then
		     if test "$with_libwrap_path" != "yes"; then
			     CPPFLAGS="${CPPFLAGS} -I$with_libwrap_path/include"
			     LDFLAGS="${LDFLAGS} -L$with_libwrap_path/lib"
		     fi

		     LDFLAGS="$LDFLAGS -lwrap"
		    
		     AC_CHECK_HEADER(tcpd.h, [],[ AC_MSG_ERROR(Asked to use libwrap but I couldn't find tcpd.h.) ])
		     AC_MSG_CHECKING([for TCP wrappers library -lwrap])
	     else
		     AC_MSG_ERROR(****Asked to disable libwrap support but It's not possible in this version****);
	     fi],
	     [LDFLAGS="$LDFLAGS -lwrap"
	      AC_CHECK_HEADER(tcpd.h, [],[ AC_MSG_ERROR(Asked to use libwrap but I couldn't find tcpd.h.) ])
	      AC_MSG_CHECKING([for TCP wrappers library -lwrap])

			AC_DEFINE(USE_LIBWRAP)
	      AC_DEFINE(PROCESS_OPTIONS)
	      ])
AC_MSG_RESULT(yes)

AC_CHECK_FUNC(yp_get_default_domain, [], 
	AC_CHECK_LIB(wrap, yp_get_default_domain, [], 
		AC_CHECK_LIB(nsl, yp_get_default_domain, [], 
			[ AC_DEFINE(INCLUDED_YP_GET_DEFAULT_DOMAIN,"1") 
			  AC_MSG_RESULT( using own yp_get_default_domain... yes )] )))

dnl SCNu64 
AC_MSG_CHECKING([SCNu64 definition])
AC_RUN_IFELSE(	[AC_LANG_PROGRAM(	[[
										#include <stdio.h>
										#include <inttypes.h>
									]],
									[[
										uint64_t i=UINT64_C(100000000000); 
										char buf[]="100000000000"; 
										sscanf(buf,"%"SCNu64,&i); 
										exit(i!=100000000000);
									]])],
				AC_MSG_RESULT(ISOC99),
				[ AC_RUN_IFELSE(	[AC_LANG_PROGRAM(	[[
														#include <stdio.h>
														#include <inttypes.h>
													]],
													[[
														uint64_t i=100000000000; 
														char buf[]="100000000000"; 
														sscanf(buf,"%qu",&i); 
														exit(i!=100000000000);
													]])],
								AC_DEFINE_UNQUOTED(SCNu64,"qu") AC_MSG_RESULT(%qu),
								[AC_RUN_IFELSE(	[AC_LANG_PROGRAM(	[[
																		#include <stdio.h>
																		#include <inttypes.h>
																	]],
																	[[
																		uint64_t i=100000000000; 
																		unsigned long long i; 
																		char buf[]="100000000000"; 
																		sscanf(buf,"%llu",&i); 
																		exit(i!=100000000000);
																	]])],
												AC_DEFINE(SCNu64,"%llu") AC_MSG_RESULT(%llu),
												[AC_MSG_ERROR([unknown])]
								)]
				)]
)

dnl PRIu64 
AC_MSG_CHECKING([PRIu64 definition])
AC_RUN_IFELSE(	[AC_LANG_PROGRAM(	[[
										#include <stdio.h>
										#include <inttypes.h>
									]],
									[[
										uint64_t i=UINT64_C(100000000000); 
										char buf[]="100000000000";
										char tst[]="            ";
										sprintf(tst,"%"PRIu64,i); 
										exit(strcmp(buf,tst));
									]])],
				AC_MSG_RESULT(ISOC99),
				[ AC_RUN_IFELSE(	[AC_LANG_PROGRAM(	[[
														#include <stdio.h>
													]],
													[[
														uint64_t i=100000000000; 
														char buf[]="100000000000";
														char tst[]="            ";
														sprintf(tst,"%qu",i); 
														exit(strcmp(buf,tst));
													]])],
								AC_DEFINE_UNQUOTED(PRIu64,"qu") AC_MSG_RESULT(%qu),
								[AC_RUN_IFELSE(	[AC_LANG_PROGRAM(	[[
																		#include <stdio.h>
																	]],
																	[[
																		uint64_t i=100000000000; 
																		char buf[]="100000000000";
																		char tst[]="            ";
																		sprintf(tst,"%llu",i); 
																		exit(strcmp(buf,tst));
																	]])],
												AC_DEFINE(PRIu64,"%llu") AC_MSG_RESULT(%llu),
												[AC_MSG_ERROR([unknown])]
								)]
				)]
)

####################
### Config location:
###
dnl Where is config file for dcd
AC_ARG_WITH(config-dir, 
	    AC_HELP_STRING([--with-config-dir=DIR],[Set config file path (default: sysconfdir/dcd)]),
	      if test "x$with_config_dir" = xyes; then
		      DCD_CONFIGDIR="${sysconfdir}/dcd"
	      else
		      if test "x$with_config_dir" = xno; then
			      AC_MSG_ERROR(Config directory required)
		      else
			      DCD_CONFIGDIR="$with_config_dir"
		      fi
	      fi,
	      DCD_CONFIGDIR="${sysconfdir}/dcd")

AC_DEFINE_DIR(CONFIGDIR, DCD_CONFIGDIR)
AC_SUBST(CONFIGDIR, $DCD_CONFIGDIR)

####################
### Logs location:
###
dnl Where are log files for dcd:
#AC_ARG_WITH(log-dir,
#	    AC_HELP_STRING([--with-log-dir=DIR],[Set log file path (default: /var/log/dcd)]),
#	    if test "x$with_log_dir" = xyes; then
#		    DCD_LOG_DIR="/var/log/dcd"
#	    else
#		    if test "x$with_log_dir" = xno; then
#			    AC_MSG_ERROR(Log directory required)
#		    else
#			    DCD_LOG_DIR="$with_log_dir"
#		    fi
#	    fi,
#	    DCD_LOG_DIR="/var/log/dcd")

#AC_DEFINE_DIR(LOG_DIR, DCD_LOG_DIR)
#AC_SUBST(LOG_DIR, $DCD_LOG_DIR)

############################
### Config default user:
###
dnl What is the default dcd user
AC_ARG_WITH(user,
	    AC_HELP_STRING([--with-user=USER],[Set default user of dcd (default: daemon)]),
	    DCD_DEFAULT_USER="$with_user", DCD_DEFAULT_USER="daemon")
AC_SUBST(DEFAULT_USER, $DCD_DEFAULT_USER)
AC_DEFINE_UNQUOTED(DEFAULT_USER, "$DCD_DEFAULT_USER")

############################
### Config default group:
###
dnl What is the default dcd group
AC_ARG_WITH(group,
	    AC_HELP_STRING([--with-group=GROUP],[Set default group of dcd (default: daemon)]),
	    DCD_DEFAULT_GROUP="$with_group", DCD_DEFAULT_GROUP="daemon")
AC_SUBST(DEFAULT_GROUP, $DCD_DEFAULT_GROUP)
AC_DEFINE_UNQUOTED(DEFAULT_GROUP, "$DCD_DEFAULT_GROUP")

############################
### Checks for header files:
###

AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h errno.h fcntl.h netdb.h netinet/in.h pthread.h stdlib.h string.h sys/ioctl.h sys/signal.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

############################
### Set buildtime:
###

dnl Setting build time...
AC_MSG_CHECKING([build time...])
CURRENT_TIME="`date -u`"
echo "$CURRENT_TIME"
AC_DEFINE_UNQUOTED(BUILD_TIME, "$CURRENT_TIME")

#################
### Output files:
###

AC_OUTPUT([config.h
	   README
	   Makefile
	   src/Makefile
	   doc/Makefile
	   doc/pl/Makefile
	   contrib/Makefile
	   contrib/logrotate.dcd
	   contrib/PLD/Makefile
	   contrib/PLD/dcd.init
	   contrib/PLD/dcd.spec
	   contrib/MandrakeLinux/Makefile
	   contrib/MandrakeLinux/dcd.init
	   contrib/MandrakeLinux/dcd.spec
	   conf/Makefile
	   conf/dcd.conf
	   src/dcd.adduser])
