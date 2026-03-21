sysconfigdir = @CONFIGDIR@

sysconfig_DATA = console.allow dcd.users dcd.banned dcd.conf dcd.motd dcd.welcome nicks.allow dcd.penalties dcd.rules dcd.hublinks dcd.usercommands

EXTRA_DIST = $(sysconfig_DATA) dcd.conf.in

AUTOMAKE_OPTIONS=foreign

CLEANFILES = *~

install-data-hook:
	chmod 751 $(DESTDIR)@CONFIGDIR@
	chown -R @DEFAULT_USER@:@DEFAULT_GROUP@ $(DESTDIR)@CONFIGDIR@ || echo "WARNING!!!!" ; exit 0
	chmod 644 $(DESTDIR)@CONFIGDIR@/*

	chmod 640 $(DESTDIR)@CONFIGDIR@/dcd.users
	chmod 640 $(DESTDIR)@CONFIGDIR@/console.allow
	chown @DEFAULT_USER@:@DEFAULT_GROUP@ $(DESTDIR)@CONFIGDIR@/dcd.banned || echo "WARNING!!!!" ; exit 0
