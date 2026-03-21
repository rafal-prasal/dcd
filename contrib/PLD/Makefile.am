
EXTRA_DIST = dcd.spec dcd.spec.in dcd.init dcd.init.in

AUTOMAKE_OPTIONS=foreign

CLEANFILES = *~ 


# make rpms
rpm: dcd.spec
	$(MAKE) dist
	rpm -ta --clean --rmsource $(PACKAGE)-$(VERSION).tar.gz

