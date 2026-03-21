sbin_PROGRAMS	= dcd 
dcd_INCLUDES	= conf.h ../config.h cmd.h console.h main.h match.h misc.h net.h pch.h penalties.h cmd.user.h cmd.opt_line.h cmd.dc.h cmd.conf.h communicates.h compatibility.h cmd.udp.h log.h
dcd_SOURCES	= conf.c console.c main.c match.c misc.c net.c penalties.c cmd.c cmd.user.c cmd.opt_line.c cmd.dc.c cmd.conf.c compatibility.c cmd.udp.c log.c

sbin_SCRIPTS = dcd.adduser

EXTRA_DIST	= $(dcd_INCLUDES)
CLEANFILES	= *~ *.rej

INCLUDES	= -I../ -I/usr/include/ptlib/ -I/usr/include/
AM_CPPFLAGS	= @CPPFLAGS@ -Wall

#AUTOMAKE_OPTIONS=foreign

