## Process this file with automake to create Makefile.in.

AUTOMAKE_OPTIONS = 1.5

SUBDIRS = intl po vfs slang edit src lib doc syntax pc

EXTRA_DIST =	FAQ INSTALL.FAST MAINTAINERS \
		README.PC README.QNX README.smbfs \
		extraconf.h

noinst_DATA = mc.spec

dist-hook:
	cp mc.spec $(distdir)
